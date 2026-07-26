from . import _mbusdevice
from .. import meterdata
import binascii
import math
import sys

if sys.implementation.name == "micropython":
	from mbus._mp_datetime import datetime
else:
	from datetime import datetime


_DLMS_HEADER1_LENGTH = 26
_DLMS_HEADER2_START = 256
_DLMS_HEADER2_LENGTH = 9
_DLMS_HEADER2_END = _DLMS_HEADER2_START + _DLMS_HEADER2_LENGTH
_DLMS_SYST_START = 11
_DLMS_SYST_END = _DLMS_SYST_START + 8
_DLMS_IC_START = 22
_DLMS_IC_END = _DLMS_IC_START + 4

_DATA_TYPE_OCTET_STRING = 0x09
_DATA_TYPE_DOUBLE_LONG_UNSIGNED = 0x06
_DATA_TYPE_LONG_UNSIGNED = 0x12

_OBIS_MAP = {
	bytes([0x01, 0x00, 0x01, 0x07, 0x00, 0xFF]): ("activePowerPlus", 1.0),
	bytes([0x01, 0x00, 0x02, 0x07, 0x00, 0xFF]): ("activePowerMinus", 1.0),
	bytes([0x01, 0x00, 0x01, 0x08, 0x00, 0xFF]): ("activeEnergyPlus", 1.0),
	bytes([0x01, 0x00, 0x02, 0x08, 0x00, 0xFF]): ("activeEnergyMinus", 1.0),
	bytes([0x01, 0x00, 0x03, 0x08, 0x00, 0xFF]): ("reactiveEnergyPlus", 1.0),
	bytes([0x01, 0x00, 0x04, 0x08, 0x00, 0xFF]): ("reactiveEnergyMinus", 1.0),
	bytes([0x01, 0x00, 0x20, 0x07, 0x00, 0xFF]): ("voltageL1", 10.0),
	bytes([0x01, 0x00, 0x34, 0x07, 0x00, 0xFF]): ("voltageL2", 10.0),
	bytes([0x01, 0x00, 0x48, 0x07, 0x00, 0xFF]): ("voltageL3", 10.0),
	bytes([0x01, 0x00, 0x1F, 0x07, 0x00, 0xFF]): ("currentL1", 100.0),
	bytes([0x01, 0x00, 0x33, 0x07, 0x00, 0xFF]): ("currentL2", 100.0),
	bytes([0x01, 0x00, 0x47, 0x07, 0x00, 0xFF]): ("currentL3", 100.0),
	bytes([0x01, 0x00, 0x0D, 0x07, 0x00, 0xFF]): ("powerFactor", 1000.0),
}
_OBIS_METER_NUMBER = bytes([0x00, 0x00, 0x60, 0x01, 0x00, 0xFF])


class Sagemcom_T210D(_mbusdevice._MBusDevice):
	def __init__(self, key):
		self._key = binascii.unhexlify(key)
		self._frame1 = None

	def uart_config(self):
		return {
			"baudrate": 2400,
			"data_bits": 8,
			"stop_bits": 1,
			"parity": "EVEN",
		}

	def parse_data(self, data):
		if len(data) == 256:
			self._frame1 = data
			print("Buffered frame 1, waiting for frame 2")
			return None
		if len(data) == 26 and self._frame1 is not None:
			data = self._frame1 + data
			self._frame1 = None
		elif len(data) == 26:
			print("Received frame 2 without frame 1")
			return False

		if len(data) < 256:
			print("Received packet with invalid size:", len(data), "< 256")
			return False

		payload_length = 243
		payload_length1 = 228
		payload_length2 = payload_length - payload_length1

		if len(data) <= payload_length:
			print("Payload length is too big for received data")
			return False
		if payload_length2 >= (len(data) - _DLMS_HEADER2_START - _DLMS_HEADER2_LENGTH):
			print("Payload length 2 is too big")
			return False

		iv = data[_DLMS_SYST_START:_DLMS_SYST_END] + data[_DLMS_IC_START:_DLMS_IC_END]
		ciphertext = data[_DLMS_HEADER1_LENGTH:_DLMS_HEADER1_LENGTH + payload_length1]
		ciphertext += data[_DLMS_HEADER2_END:_DLMS_HEADER2_END + payload_length2]
		plaintext = self._decrypt_aes_gcm(self._key, ciphertext, iv)

		if len(plaintext) < 6 or plaintext[0] != 0x0F or plaintext[5] != 0x0C:
			print("Packet was decrypted but data is invalid")
			return False

		meter = meterdata.MeterData()
		self._parse_timestamp(plaintext, meter)
		self._parse_meter_number(plaintext, meter)
		self._parse_values(plaintext, meter)
		return meter

	def _find_obis_value(self, data, obis):
		pattern = bytes([_DATA_TYPE_OCTET_STRING, 0x06]) + obis
		pos = data.find(pattern)
		if pos < 0:
			return None

		pos += len(pattern)
		if pos >= len(data):
			return None
		tag = data[pos]
		if tag == _DATA_TYPE_DOUBLE_LONG_UNSIGNED and pos + 5 <= len(data):
			return int.from_bytes(data[pos + 1:pos + 5], "big")
		if tag == _DATA_TYPE_LONG_UNSIGNED and pos + 3 <= len(data):
			return int.from_bytes(data[pos + 1:pos + 3], "big")
		return None

	def _parse_values(self, plaintext, meter):
		for obis, field in _OBIS_MAP.items():
			raw_value = self._find_obis_value(plaintext, obis)
			if raw_value is not None:
				setattr(meter, field[0], raw_value / field[1])

	def _parse_timestamp(self, plaintext, meter):
		pattern = bytes([_DATA_TYPE_OCTET_STRING, 0x0C])
		pos = plaintext.find(pattern)
		if pos < 0 or pos + 14 > len(plaintext):
			return

		ts = plaintext[pos + 2:pos + 14]
		year = int.from_bytes(ts[0:2], "big")
		month = ts[2]
		day = ts[3]
		hour = ts[5]
		minute = ts[6]
		seconds = ts[7]
		ts = datetime(year, month, day, hour, minute, seconds)
		meter.timestamp = self.strftime(ts)
		meter.lxTimestamp = math.floor((ts - datetime(2009, 1, 1, 0, 0, 0)).total_seconds())

	def _parse_meter_number(self, plaintext, meter):
		pattern = bytes([_DATA_TYPE_OCTET_STRING, 0x06]) + _OBIS_METER_NUMBER
		pos = plaintext.find(pattern)
		if pos < 0:
			return

		pos += len(pattern)
		if pos + 2 > len(plaintext) or plaintext[pos] != _DATA_TYPE_OCTET_STRING:
			return

		data_length = plaintext[pos + 1]
		start = pos + 2
		end = start + data_length
		if end > len(plaintext):
			return

		candidate = plaintext[start:end]
		digits = bytes([b for b in candidate if 0x30 <= b <= 0x39])
		if len(digits) > 0:
			meter.meterNumber = digits[:12].decode("ascii")
