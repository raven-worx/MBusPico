from . import _mbusdevice
from .. import meterdata
import binascii
import math
import sys

if sys.implementation.name == "micropython":
	from mbus._mp_datetime import datetime
else:
	from datetime import datetime


_GENERAL_GLO_CIPHER_TAG = 0xDB
_AUTHENTICATION_TAG_LENGTH = 12


class Sagemcom_T210D(_mbusdevice._MBusDevice):
	def __init__(self, key, auth_key=""):
		self._key = binascii.unhexlify(key)
		self._auth_key = binascii.unhexlify(auth_key) if auth_key else bytes()

	def parse_data(self, data):
		start = data.find(bytes([_GENERAL_GLO_CIPHER_TAG]))
		if start < 0:
			print("No Sagemcom frame start found")
			return False

		plaintext = self._decrypt_frame(data[start:])
		if plaintext is False:
			return False

		return self._parse_telegram(plaintext)

	def _decode_length(self, data, offset):
		if offset >= len(data):
			return None, None

		first = data[offset]
		offset += 1
		if first & 0x80 == 0:
			return first, offset

		count = first & 0x7F
		if count == 0 or offset + count > len(data):
			return None, None

		value = 0
		for i in range(count):
			value = (value << 8) | data[offset + i]
		return value, offset + count

	def _decrypt_frame(self, data):
		if len(data) < 18:
			print("Frame length too short:", len(data))
			return False
		if data[0] != _GENERAL_GLO_CIPHER_TAG:
			print("Wrong Sagemcom frame start:", data[0])
			return False

		system_title_length = data[1]
		if system_title_length != 8 or 2 + system_title_length >= len(data):
			print("Unsupported system title length:", system_title_length)
			return False

		offset = 2 + system_title_length
		encrypted_section_length, offset = self._decode_length(data, offset)
		if encrypted_section_length is None:
			print("Failed to decode BER length")
			return False

		expected_frame_length = offset + encrypted_section_length
		if len(data) < expected_frame_length:
			print("Incomplete Sagemcom frame:", len(data), "<", expected_frame_length)
			return False
		data = data[:expected_frame_length]

		if encrypted_section_length <= 5 + _AUTHENTICATION_TAG_LENGTH:
			print("Encrypted payload too short:", encrypted_section_length)
			return False

		security_control = data[offset]
		frame_counter = data[offset + 1:offset + 5]
		ciphertext = data[offset + 5:-_AUTHENTICATION_TAG_LENGTH]
		auth_tag = data[-_AUTHENTICATION_TAG_LENGTH:]
		iv = data[2:2 + system_title_length] + frame_counter

		if sys.implementation.name == "micropython":
			return self._decrypt_aes_gcm(self._key, ciphertext, iv)

		if len(self._auth_key) not in (0, 16):
			print("Device authentication key must be exactly 32 hex-characters")
			return False

		if len(self._auth_key) == 16:
			try:
				try:
					from Cryptodome.Cipher import AES
				except ImportError:
					from Crypto.Cipher import AES

				cipher = AES.new(self._key, AES.MODE_GCM, nonce=iv, mac_len=_AUTHENTICATION_TAG_LENGTH)
				cipher.update(bytes([security_control]) + self._auth_key)
				return cipher.decrypt_and_verify(ciphertext, auth_tag)
			except ValueError:
				print("Unable to decrypt ciphertext. Keys are wrong or the frame is corrupted.")
				return False

		return self._decrypt_aes_gcm(self._key, ciphertext, iv)

	def _crc16_arc(self, data):
		crc = 0
		for byte in data:
			crc ^= byte
			for _ in range(8):
				if crc & 1:
					crc = (crc >> 1) ^ 0xA001
				else:
					crc >>= 1
		return crc & 0xFFFF

	def _extract_field_value(self, telegram, obis_code):
		prefix = obis_code + "("
		for line in telegram.split("\r\n"):
			if not line.startswith(prefix):
				continue

			end = line.find(")", len(prefix))
			if end < 0:
				return None
			return line[len(prefix):end]
		return None

	def _parse_number(self, value):
		if value is None:
			return None
		value = value.split("*", 1)[0]
		try:
			return float(value)
		except ValueError:
			return None

	def _parse_meter_number(self, value):
		if value is None:
			return ""
		digits = ""
		for ch in value:
			if ch.isdigit():
				digits += ch
		if len(digits) > 0:
			return digits[:12]
		return value[:12]

	def _parse_timestamp(self, value, meter):
		if value is None or len(value) < 12:
			return

		year = 2000 + int(value[0:2])
		month = int(value[2:4])
		day = int(value[4:6])
		hour = int(value[6:8])
		minute = int(value[8:10])
		seconds = int(value[10:12])
		ts = datetime(year, month, day, hour, minute, seconds)
		meter.timestamp = self.strftime(ts)
		meter.lxTimestamp = math.floor((ts - datetime(2009, 1, 1, 0, 0, 0)).total_seconds())

	def _parse_telegram(self, plaintext):
		plaintext = plaintext.rstrip(b"\x00")
		if len(plaintext) < 7 or plaintext[0:1] != b"/" or plaintext[-2:] != b"\r\n":
			print("Decrypted telegram framing invalid")
			return False

		bang = plaintext.rfind(b"!")
		if bang < 0 or len(plaintext) - bang < 7:
			print("Failed to locate DSMR CRC trailer")
			return False

		try:
			expected_crc = int(plaintext[bang + 1:bang + 5].decode("ascii"), 16)
		except ValueError:
			print("Invalid DSMR CRC trailer")
			return False

		actual_crc = self._crc16_arc(plaintext[:bang + 1])
		if actual_crc != expected_crc:
			print("DSMR CRC mismatch:", "%04X" % actual_crc, "!=", "%04X" % expected_crc)
			return False

		try:
			telegram = plaintext[:bang].decode("ascii")
		except UnicodeError:
			print("Decrypted telegram contains invalid ASCII")
			return False

		meter = meterdata.MeterData()
		self._parse_timestamp(self._extract_field_value(telegram, "0-0:1.0.0"), meter)
		meter.meterNumber = self._parse_meter_number(
			self._extract_field_value(telegram, "0-0:96.1.0")
			or self._extract_field_value(telegram, "0-0:96.1.1")
		)

		field_map = {
			"1-0:1.7.0": "activePowerPlus",
			"1-0:2.7.0": "activePowerMinus",
			"1-0:1.8.0": "activeEnergyPlus",
			"1-0:2.8.0": "activeEnergyMinus",
			"1-0:3.8.0": "reactiveEnergyPlus",
			"1-0:4.8.0": "reactiveEnergyMinus",
			"1-0:32.7.0": "voltageL1",
			"1-0:52.7.0": "voltageL2",
			"1-0:72.7.0": "voltageL3",
			"1-0:31.7.0": "currentL1",
			"1-0:51.7.0": "currentL2",
			"1-0:71.7.0": "currentL3",
			"1-0:13.7.0": "powerFactor",
		}

		for obis_code, field_name in field_map.items():
			parsed_value = self._parse_number(self._extract_field_value(telegram, obis_code))
			if parsed_value is not None:
				setattr(meter, field_name, parsed_value)

		return meter
