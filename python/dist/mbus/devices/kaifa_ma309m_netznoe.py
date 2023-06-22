from . import _mbusdevice
from .. import meterdata
import binascii
import math
import sys

if sys.implementation.name == "micropython":
	from mbus._mp_datetime import datetime
else:
	from datetime import datetime

_DLMS_HEADER1_START = 0 # Start of first DLMS header
_DLMS_HEADER1_LENGTH = 26
_DLMS_HEADER1_END = _DLMS_HEADER1_START + _DLMS_HEADER1_LENGTH
_DLMS_HEADER2_START = 256 # Start of second DLMS header
_DLMS_HEADER2_LENGTH = 9
_DLMS_HEADER2_END = _DLMS_HEADER2_START + _DLMS_HEADER2_LENGTH
_DLMS_SYST_START = 11
_DLMS_SYST_LENGTH = 8
_DLMS_SYST_END = _DLMS_SYST_START + _DLMS_SYST_LENGTH
_DLMS_IC_START = 22
_DLMS_IC_LENGTH = 4
_DLMS_IC_END = _DLMS_IC_START + _DLMS_IC_LENGTH

_OBIS_TYPE_OFFSET = 0
_OBIS_LENGTH_OFFSET = 1
_OBIS_CODE_OFFSET = 2
_OBIS_A = 0
_OBIS_B = 1
_OBIS_C = 2
_OBIS_D = 3
_OBIS_E = 4
_OBIS_F = 5

_DataType_NullData = 0x00
_DataType_Boolean = 0x03
_DataType_BitString = 0x04
_DataType_DoubleLong = 0x05
_DataType_DoubleLongUnsigned = 0x06
_DataType_OctetString = 0x09
_DataType_VisibleString = 0x0A
_DataType_Utf8String = 0x0C
_DataType_BinaryCodedDecimal = 0x0D
_DataType_Integer = 0x0F
_DataType_Long = 0x10
_DataType_Unsigned = 0x11
_DataType_LongUnsigned = 0x12
_DataType_Long64 = 0x14
_DataType_Long64Unsigned = 0x15
_DataType_Enum = 0x16
_DataType_Float32 = 0x17
_DataType_Float64 = 0x18
_DataType_DateTime = 0x19
_DataType_Date = 0x1A
_DataType_Time = 0x1B
_DataType_Array = 0x01
_DataType_Structure = 0x02
_DataType_CompactArray = 0x13

_Medium_Abstract = 0x00
_Medium_Electricity = 0x01
_Medium_Heat = 0x06
_Medium_Gas = 0x07
_Medium_Water = 0x08

_CodeType_Unknown = 0
_CodeType_Timestamp = 1
_CodeType_SerialNumber = 2
_CodeType_DeviceName = 3
_CodeType_MeterNumber = 4
_CodeType_VoltageL1 = 5
_CodeType_VoltageL2 = 6
_CodeType_VoltageL3 = 7
_CodeType_CurrentL1 = 8
_CodeType_CurrentL2 = 9
_CodeType_CurrentL3 = 10
_CodeType_ActivePowerPlus = 11
_CodeType_ActivePowerMinus = 12
_CodeType_PowerFactor = 13
_CodeType_ActiveEnergyPlus = 14
_CodeType_ActiveEnergyMinus = 15
_CodeType_ReactiveEnergyPlus = 16
_CodeType_ReactiveEnergyMinus = 17

_Accuracy_SingleDigit = 0xFF
_Accuracy_DoubleDigit = 0xFE


_DECODER_START_OFFSET = 20

# Metadata
_ESPDM_TIMESTAMP = bytes([0x01, 0x00])
_ESPDM_SERIAL_NUMBER = bytes([0x60, 0x01])
_ESPDM_DEVICE_NAME = bytes([0x2A, 0x00])

# Voltage
_ESPDM_VOLTAGE_L1 = bytes([0x20, 0x07])
_ESPDM_VOLTAGE_L2 = bytes([0x34, 0x07])
_ESPDM_VOLTAGE_L3 = bytes([0x48, 0x07])

# Current
_ESPDM_CURRENT_L1 = bytes([0x1F, 0x07])
_ESPDM_CURRENT_L2 = bytes([0x33, 0x07])
_ESPDM_CURRENT_L3 = bytes([0x47, 0x07])

# Power
_ESPDM_ACTIVE_POWER_PLUS = bytes([0x01, 0x07])
_ESPDM_ACTIVE_POWER_MINUS = bytes([0x02, 0x07])
_ESPDM_POWER_FACTOR = bytes([0x0D, 0x07])

# Active energy
_ESPDM_ACTIVE_ENERGY_PLUS = bytes([0x01, 0x08])
_ESPDM_ACTIVE_ENERGY_MINUS = bytes([0x02, 0x08])

# Reactive energy
_ESPDM_REACTIVE_ENERGY_PLUS = bytes([0x03, 0x08])
_ESPDM_REACTIVE_ENERGY_MINUS = bytes([0x04, 0x08])


class Kaifa_MA309M_NetzNoe(_mbusdevice._MBusDevice):
	def __init__(self, key):
		self._key = binascii.unhexlify(key)
	
	def parse_data(self,data):
		if len(data) < 256:
			print("Received packet with invalid size:", len(data), "< 256")
			return False
		
		# Decrypting
		
		payloadLength = 243
		
		if len(data) <= payloadLength:
			print("data len:", data.length, ", payloadLength:", payloadLength)
			print("Payload length is too big for received data")
			return False
		
		payloadLength1 = 228 # TODO: read from data
		payloadLength2 = payloadLength - payloadLength1
		
		if payloadLength2 >= (len(data) - _DLMS_HEADER2_START - _DLMS_HEADER2_LENGTH):
			print("data len:", data.length, ", payloadLength2:", payloadLength2)
			print("Payload length 2 is too big")
			return False
		
		iv = bytes() # always 12 bytes
		iv += data[_DLMS_SYST_START:_DLMS_SYST_END] # copy system title to IV
		iv += data[_DLMS_IC_START:_DLMS_IC_END] # copy invocation counter to IV
		
		ciphertext = bytes()
		ciphertext += data[_DLMS_HEADER1_END:_DLMS_HEADER1_END+payloadLength1]
		ciphertext += data[_DLMS_HEADER2_END:_DLMS_HEADER2_END+payloadLength2]
		
		plaintext = self._decrypt_aes_gcm(self._key, ciphertext, iv)
		
		if plaintext[0] != 0x0F or plaintext[5] != 0x0C:
			print("Packet was decrypted but data is invalid")
			return False
		
		# Decoding
		
		meter = meterdata.MeterData()
		
		currentPosition = _DECODER_START_OFFSET # Offset for start of OBIS decoding, skip header, timestamp and break block
		
		while True:
			print("currentPosition:", currentPosition)
			print("OBIS header type:", plaintext[currentPosition + _OBIS_TYPE_OFFSET])
			if plaintext[currentPosition + _OBIS_TYPE_OFFSET] != _DataType_OctetString:
				print("Unsupported OBIS header type")
				return False
			
			obisCodeLength = plaintext[currentPosition + _OBIS_LENGTH_OFFSET]
			print("OBIS code/header length:", obisCodeLength)
			
			if obisCodeLength != 0x06 and obisCodeLength != 0x0C:
				print("Unsupported OBIS header length")
				return False
			
			obisCode = plaintext[currentPosition+_OBIS_CODE_OFFSET:currentPosition+_OBIS_CODE_OFFSET+obisCodeLength+1] # Copy OBIS code to array
			
			timestampFound = False
			meterNumberFound = False
			
			# Do not advance Position when reading the Timestamp at DECODER_START_OFFSET
			if obisCodeLength == 0x0C and currentPosition == _DECODER_START_OFFSET:
				timestampFound = True 
			elif currentPosition != _DECODER_START_OFFSET and plaintext[currentPosition-1] == 0xFF:
				meterNumberFound = True
			else:
				currentPosition += obisCodeLength + 2 # Advance past code and position
			
			dataType = plaintext[currentPosition]
			currentPosition += 1 # Advance past data type
			
			dataLength = 0x00
			
			codeType = _CodeType_Unknown
			
			print("obisCode (OBIS_A):", obisCode[_OBIS_A])
			print("currentPosition:", currentPosition)
			
			if obisCode[_OBIS_A] == _Medium_Electricity:
				# Compare C and D against code
				if obisCode[_OBIS_C:_OBIS_D+1] == _ESPDM_VOLTAGE_L1:
					codeType = _CodeType_VoltageL1
				elif obisCode[_OBIS_C:_OBIS_D+1] == _ESPDM_VOLTAGE_L2:
					codeType = _CodeType_VoltageL2
				elif obisCode[_OBIS_C:_OBIS_D+1] == _ESPDM_VOLTAGE_L3:
					codeType = _CodeType_VoltageL3
				elif obisCode[_OBIS_C:_OBIS_D+1] == _ESPDM_CURRENT_L1:
					codeType = _CodeType_CurrentL1
				elif obisCode[_OBIS_C:_OBIS_D+1] == _ESPDM_CURRENT_L2:
					codeType = _CodeType_CurrentL2
				elif obisCode[_OBIS_C:_OBIS_D+1] == _ESPDM_CURRENT_L3:
					codeType = _CodeType_CurrentL3
				elif obisCode[_OBIS_C:_OBIS_D+1] == _ESPDM_ACTIVE_POWER_PLUS:
					codeType = _CodeType_ActivePowerPlus
				elif obisCode[_OBIS_C:_OBIS_D+1] == _ESPDM_ACTIVE_POWER_MINUS:
					codeType = _CodeType_ActivePowerMinus
				elif obisCode[_OBIS_C:_OBIS_D+1] == _ESPDM_POWER_FACTOR:
					codeType = _CodeType_PowerFactor
				elif obisCode[_OBIS_C:_OBIS_D+1] == _ESPDM_ACTIVE_ENERGY_PLUS:
					codeType = _CodeType_ActiveEnergyPlus
				elif obisCode[_OBIS_C:_OBIS_D+1] == _ESPDM_ACTIVE_ENERGY_MINUS:
					codeType = _CodeType_ActiveEnergyMinus
				elif obisCode[_OBIS_C:_OBIS_D+1] == _ESPDM_REACTIVE_ENERGY_PLUS:
					codeType = _CodeType_ReactiveEnergyPlus
				elif obisCode[_OBIS_C:_OBIS_D+1] == _ESPDM_REACTIVE_ENERGY_MINUS:
					codeType = _CodeType_ReactiveEnergyMinus
				else:
					print("Unsupported OBIS code OBIS_C:", obisCode[_OBIS_C], "OBIS_D:", obisCode[_OBIS_D])
			elif obisCode[_OBIS_A] == _Medium_Abstract:
				if obisCode[_OBIS_C:_OBIS_D+1] == _ESPDM_TIMESTAMP:
					codeType = _CodeType_Timestamp;
				elif obisCode[_OBIS_C:_OBIS_D+1] == _ESPDM_SERIAL_NUMBER:
					codeType = _CodeType_SerialNumber;
				elif obisCode[_OBIS_C:_OBIS_D+1] == _ESPDM_DEVICE_NAME:
					codeType = _CodeType_DeviceName;
				else:
					print("Unsupported OBIS code OBIS_C:", obisCode[OBIS_C], "OBIS_D:", obisCode[OBIS_D])
			# Needed so the Timestamp at DECODER_START_OFFSET gets read correctly
			# as it doesn't have an obisMedium
			elif timestampFound == True:
				print("Found Timestamp without obisMedium")
				codeType = _CodeType_Timestamp
			elif meterNumberFound == True:
				print("Found MeterNumber without obisMedium")
				codeType = _CodeType_MeterNumber
			else:
				print("Unsupported OBIS medium")
				return False
			
			if dataType == _DataType_DoubleLongUnsigned:
				dataLength = 4
				dataValue = int.from_bytes(plaintext[currentPosition:currentPosition+dataLength], 'big')
				if codeType == _CodeType_ActivePowerPlus:
					meter.activePowerPlus = dataValue
				elif codeType == _CodeType_ActivePowerMinus:
					meter.activePowerMinus = dataValue
				elif codeType == _CodeType_ActiveEnergyPlus:
					meter.activeEnergyPlus = dataValue
				elif codeType == _CodeType_ActiveEnergyMinus:
					meter.activeEnergyMinus = dataValue
				elif codeType == _CodeType_ReactiveEnergyPlus:
					meter.reactiveEnergyPlus = dataValue
				elif codeType == _CodeType_ReactiveEnergyMinus:
					meter.reactiveEnergyMinus = dataValue
			elif dataType == _DataType_LongUnsigned:
				dataLength = 2
				dataValue = int.from_bytes(plaintext[currentPosition:currentPosition+dataLength], 'big')
				if plaintext[currentPosition + 5] == _Accuracy_SingleDigit:
					dataValue = dataValue / 10.0 # Divide by 10 to get decimal places
				elif plaintext[currentPosition + 5] == _Accuracy_DoubleDigit:
					dataValue = dataValue / 100.0 # Divide by 100 to get decimal places
				else:
					dataValue = dataValue # No decimal places
				
				if codeType == _CodeType_VoltageL1:
					meter.voltageL1 = dataValue
				elif codeType == _CodeType_VoltageL2:
					meter.voltageL2 = dataValue
				elif codeType == _CodeType_VoltageL3:
					meter.voltageL3 = dataValue
				elif codeType == _CodeType_CurrentL1:
					meter.currentL1 = dataValue
				elif codeType == _CodeType_CurrentL2:
					meter.currentL2 = dataValue
				elif codeType == _CodeType_CurrentL3:
					meter.currentL3 = dataValue
				elif codeType == _CodeType_PowerFactor:
					meter.powerFactor = dataValue
			elif dataType == _DataType_OctetString:
				print("Arrived on OctetString")
				print("currentPosition:", currentPosition, "plaintext:", plaintext[currentPosition])
				
				dataLength = plaintext[currentPosition]
				currentPosition += 1 # Advance past string length
				
				if codeType == _CodeType_Timestamp: # Handle timestamp
					year = int.from_bytes(plaintext[currentPosition:currentPosition+2], 'big')
					month = plaintext[currentPosition+2]
					day = plaintext[currentPosition+3]
					hour = plaintext[currentPosition+5]
					minute = plaintext[currentPosition+6]
					seconds = plaintext[currentPosition+7]
					ts = datetime(year,month,day,hour,minute,seconds)
					meter.timestamp = self.strftime(ts)
					meter.lxTimestamp = math.floor((ts - datetime(2009,1,1,0,0,0)).total_seconds())
					print("Timestamp:", meter.timestamp, "lxts:", meter.lxTimestamp)
				elif codeType == _CodeType_MeterNumber:
					meter.meterNumber = plaintext[currentPosition:currentPosition+dataLength+1].decode('utf8')
					print("Constructed MeterNumber:", meter.meterNumber)
			else:
				print("Unsupported OBIS data type")
				return False
			
			currentPosition += dataLength # Skip data length
			
			# Don't skip the break on the first Timestamp, as there's none
			if timestampFound == False:
				currentPosition += 2 # Skip break after data
			
			if currentPosition > payloadLength:
				break
			if plaintext[currentPosition] == 0x0F: # There is still additional data for this type, skip it
				#currentPosition += 6; // Skip additional data and additional break; this will jump out of bounds on last frame
				# on EVN Meters the additional data (no additional Break) is only 3 Bytes + 1 Byte for the "there is data" Byte
				currentPosition += 4 # Skip additional data and additional break; this will jump out of bounds on last frame
		
		return meter
