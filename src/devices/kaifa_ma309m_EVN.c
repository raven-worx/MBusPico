#include <mbuspico.h>

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include <time.h>

#include <mbedtls/gcm.h>

//
// implementation of data intepretation based on
// https://github.com/firegore/esphome-dlms-meter
//

#define DLMS_HEADER1_OFFSET		0 // Start of first DLMS header
#define DLMS_HEADER1_LENGTH		26
#define DLMS_HEADER2_OFFSET		256 // Start of second DLMS header
#define DLMS_HEADER2_LENGTH		9
#define DLMS_SYST_OFFSET		11
#define DLMS_SYST_LENGTH		8
#define DLMS_IC_OFFSET			22
#define DLMS_IC_LENGTH			4

#define DataType_NullData  			0x00
#define DataType_Boolean  			0x03
#define DataType_BitString  		0x04
#define DataType_DoubleLong  		0x05
#define DataType_DoubleLongUnsigned  0x06
#define DataType_OctetString  		0x09
#define DataType_VisibleString  	0x0A
#define DataType_Utf8String  		0x0C
#define DataType_BinaryCodedDecimal  0x0D
#define DataType_Integer  			0x0F
#define DataType_Long  				0x10
#define DataType_Unsigned  			0x11
#define DataType_LongUnsigned  		0x12
#define DataType_Long64  			0x14
#define DataType_Long64Unsigned  	0x15
#define DataType_Enum				0x16
#define DataType_Float32			0x17
#define DataType_Float64			0x18
#define DataType_DateTime			0x19
#define DataType_Date				0x1A
#define DataType_Time				0x1B
#define DataType_Array				0x01
#define DataType_Structure			0x02
#define DataType_CompactArray		0x13

#define Medium_Abstract				0x00
#define Medium_Electricity			0x01
#define Medium_Heat					0x06
#define Medium_Gas					0x07
#define Medium_Water				0x08

#define CodeType_Unknown			0
#define CodeType_Timestamp			1
#define CodeType_SerialNumber		2
#define CodeType_DeviceName			3
#define CodeType_MeterNumber		4
#define CodeType_VoltageL1			5
#define CodeType_VoltageL2			6
#define CodeType_VoltageL3			7
#define CodeType_CurrentL1			8
#define CodeType_CurrentL2			9
#define CodeType_CurrentL3			10
#define CodeType_ActivePowerPlus	11
#define CodeType_ActivePowerMinus	12
#define CodeType_PowerFactor		13
#define CodeType_ActiveEnergyPlus	14
#define CodeType_ActiveEnergyMinus	15
#define CodeType_ReactiveEnergyPlus	16
#define CodeType_ReactiveEnergyMinus 17

#define Accuracy_SingleDigit		0xFF
#define Accuracy_DoubleDigit		0xFE

static mbedtls_gcm_context aes; // AES context used for decryption

// Data structure
#define DECODER_START_OFFSET  	20 // Offset for start of OBIS decoding, skip header, timestamp and break block

#define OBIS_TYPE_OFFSET  	0
#define OBIS_LENGTH_OFFSET  1
#define OBIS_CODE_OFFSET  	2
#define OBIS_A				0
#define OBIS_B				1
#define OBIS_C				2
#define OBIS_D				3
#define OBIS_E				4
#define OBIS_F				5

// Metadata
static byte ESPDM_TIMESTAMP[] 			= {0x01, 0x00};
static const byte ESPDM_SERIAL_NUMBER[]	= {0x60, 0x01};
static const byte ESPDM_DEVICE_NAME[] 	= {0x2A, 0x00};

// Voltage
static byte ESPDM_VOLTAGE_L1[] 			= {0x20, 0x07};
static const byte ESPDM_VOLTAGE_L2[]	= {0x34, 0x07};
static const byte ESPDM_VOLTAGE_L3[]	= {0x48, 0x07};

// Current
static const byte ESPDM_CURRENT_L1[]	= {0x1F, 0x07};
static const byte ESPDM_CURRENT_L2[]	= {0x33, 0x07};
static const byte ESPDM_CURRENT_L3[]	= {0x47, 0x07};

// Power
static const byte ESPDM_ACTIVE_POWER_PLUS[] = {0x01, 0x07};
static const byte ESPDM_ACTIVE_POWER_MINUS[] = {0x02, 0x07};
static const byte ESPDM_POWER_FACTOR[]	= {0x0D, 0x07};

// Active energy
static const byte ESPDM_ACTIVE_ENERGY_PLUS[] = {0x01, 0x08};
static const byte ESPDM_ACTIVE_ENERGY_MINUS[] = {0x02, 0x08};

// Reactive energy
static const byte ESPDM_REACTIVE_ENERGY_PLUS[] = {0x03, 0x08};
static const byte ESPDM_REACTIVE_ENERGY_MINUS[] = {0x04, 0x08};


static int receiveBufferIndex = 0; // Current position of the receive buffer
#define READ_BUFFER_SIZE 1024 // Size of the receive buffer
static byte receiveBuffer[READ_BUFFER_SIZE]; // Stores the packet currently being received
static unsigned long lastRead = 0; // Timestamp when data was last read

static byte key[16];
static size_t keyLength = 0;

static void abort() {
	receiveBufferIndex = 0;
}

static uint16_t swap_uint16(uint16_t val) {
	return (val << 8) | (val >> 8);
}

static uint32_t swap_uint32(uint32_t val) {
	val = ((val << 8) & 0xFF00FF00) | ((val >> 8) & 0xFF00FF);
	return (val << 16) | (val >> 16);
}

static void set_key(byte k[], size_t keyLength) {
	memcpy(&key[0], &k[0], keyLength);
	keyLength = keyLength;
}

static void log_packet(byte array[], size_t length) {
	char buffer[(length*3)];

	for (unsigned int i = 0; i < length; i++) {
		byte nib1 = (array[i] >> 4) & 0x0F;
		byte nib2 = (array[i] >> 0) & 0x0F;
		buffer[i*3] = nib1  < 0xA ? '0' + nib1  : 'A' + nib1  - 0xA;
		buffer[i*3+1] = nib2  < 0xA ? '0' + nib2  : 'A' + nib2  - 0xA;
		buffer[i*3+2] = ' ';
	}

	buffer[(length*3)-1] = '\0';

	MBUSPICO_LOG_D(LOG_TAG_DEVICE, buffer);
}

static time_t timestamp_to_lx(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second) {
	static time_t lxTime = 0;
	if (lxTime <= 0) {
		struct tm t;
   		t.tm_year = 2009-1900;
   		t.tm_mon = 1-1;
   		t.tm_mday = 1;
   		t.tm_hour = 0;
   		t.tm_min = 0;
   		t.tm_sec = 0;
   		t.tm_isdst = -1;
		lxTime = mktime(&t);
		if (lxTime <= 0) {
			MBUSPICO_LOG_E(LOG_TAG_DEVICE, "Loxone timestamp initializion failed: %lld", lxTime);
			return 0;
		}
		else {
			MBUSPICO_LOG_I(LOG_TAG_DEVICE, "Loxone timestamp initialized: %lld", lxTime);
		}
	}
   
	struct tm t;
   		t.tm_year = year-1900;
   		t.tm_mon = month-1;
   		t.tm_mday = day;
   		t.tm_hour = hour;
   		t.tm_min = minute;
   		t.tm_sec = second;
   		t.tm_isdst = -1;
	time_t timestamp = mktime(&t);
	if (timestamp <= 0) {
		MBUSPICO_LOG_E(LOG_TAG_DEVICE, "Failed to create timestamp from: '%d-%02d-%02d %02d:%02d:%02d'", year, month, day, hour, minute, second);
		return 0;
	}

	if (timestamp < lxTime) {
		MBUSPICO_LOG_E(LOG_TAG_DEVICE, "Invalid timestamp: timestamp invalid or earlier than Loxone timestamp: %lld (ts) < %lld (lx)", timestamp, lxTime);
		return 0;
	}

	return timestamp - lxTime;
}

static int available(xMBusData_t* data) {
	return xQueueReceive(g_DeviceEventQueue, data, 0) == pdPASS ? 1 : 0;
}

static void loop() {
	uint64_t currentTime = mbuspico_time_ms();

	xMBusData_t d;
	while(available(&d)) { // read while data is available
		if(receiveBufferIndex >= READ_BUFFER_SIZE) {
			MBUSPICO_LOG_E(LOG_TAG_DEVICE, "Buffer overflow");
			receiveBufferIndex = 0;
		}
		for (int i = 0; i < d.len; ++i) {
			receiveBuffer[receiveBufferIndex] = d.data[i];
			receiveBufferIndex++;
		}
		lastRead = currentTime;
	}

	if(receiveBufferIndex > 0 && currentTime - lastRead > 2500 /*ms*/) {
		MBUSPICO_LOG_D(LOG_TAG_DEVICE, "receiveBufferIndex: %d", receiveBufferIndex);
		if(receiveBufferIndex < 256) {
			MBUSPICO_LOG_E(LOG_TAG_DEVICE, "Received packet with invalid size");
			return abort();
		}

		MBUSPICO_LOG_D(LOG_TAG_DEVICE, "Handling packet", "");
		log_packet(receiveBuffer, receiveBufferIndex);

		// Decrypting

		uint16_t payloadLength;
		//memcpy(&payloadLength, &receiveBuffer[20], 2); // Copy payload length
		memcpy(&payloadLength, &receiveBuffer[18], 2); // Copy payload length
		//payloadLength = swap_uint16(payloadLength) - 5;
		payloadLength = 243;

		if(receiveBufferIndex <= payloadLength) {
			MBUSPICO_LOG_D(LOG_TAG_DEVICE, "receiveBufferIndex: %d, payloadLength: %d", receiveBufferIndex, payloadLength);
			MBUSPICO_LOG_E(LOG_TAG_DEVICE, "Payload length is too big for received data");
			return abort();
		}

		/*
		uint16_t payloadLengthPacket1;
		memcpy(&payloadLengthPacket1, &receiveBuffer[9], 2); // Copy payload length of first telegram
		payloadLengthPacket1 = swap_uint16(payloadLengthPacket1);
		if(payloadLengthPacket1 >= payloadLength) {
			MBUSPICO_LOG_E(LOG_TAG_DEVICE, "Payload length 1 is too big");
			return abort();
		}
		*/
		uint16_t payloadLength1 = 228; // TODO: Read payload length 1 from data
		uint16_t payloadLength2 = payloadLength - payloadLength1;

		if(payloadLength2 >= receiveBufferIndex - DLMS_HEADER2_OFFSET - DLMS_HEADER2_LENGTH) {
			MBUSPICO_LOG_D(LOG_TAG_DEVICE, "receiveBufferIndex: %d, payloadLength2: %d", receiveBufferIndex, payloadLength2);
			MBUSPICO_LOG_E(LOG_TAG_DEVICE, "Payload length 2 is too big");
			return abort();
		}

		byte iv[12]; // Reserve space for the IV, always 12 bytes

		memcpy(&iv[0], &receiveBuffer[DLMS_SYST_OFFSET], DLMS_SYST_LENGTH); // copy system title to IV
		memcpy(&iv[8], &receiveBuffer[DLMS_IC_OFFSET], DLMS_IC_LENGTH); // copy invocation counter to IV

		byte ciphertext[payloadLength];
		memcpy(&ciphertext[0], &receiveBuffer[DLMS_HEADER1_OFFSET + DLMS_HEADER1_LENGTH], payloadLength1);
		memcpy(&ciphertext[payloadLength1], &receiveBuffer[DLMS_HEADER2_OFFSET + DLMS_HEADER2_LENGTH], payloadLength2);

		byte plaintext[payloadLength];

		mbedtls_gcm_init(&aes);
		mbedtls_gcm_setkey(&aes, MBEDTLS_CIPHER_ID_AES , key, keyLength * 8 /*128bit*/);
		mbedtls_gcm_auth_decrypt(&aes, payloadLength, iv, sizeof(iv), NULL, 0, NULL, 0, ciphertext, plaintext);
		mbedtls_gcm_free(&aes);

		if(plaintext[0] != 0x0F || plaintext[5] != 0x0C) {
			MBUSPICO_LOG_E(LOG_TAG_DEVICE, "Packet was decrypted but data is invalid");
			return abort();
		}

		// Decoding

		MBUSPICO_LOG_D(LOG_TAG_DEVICE, "Plaintext Packet:");
		log_packet(plaintext, 300);
		int currentPosition = DECODER_START_OFFSET;
		
		MeterData_t meterData;
		memset(&meterData, 0, sizeof(MeterData_t));

		do
		{
			MBUSPICO_LOG_D(LOG_TAG_DEVICE, "currentPosition: %d", currentPosition);
			MBUSPICO_LOG_D(LOG_TAG_DEVICE, "OBIS header type: %d", plaintext[currentPosition + OBIS_TYPE_OFFSET]);
			if(plaintext[currentPosition + OBIS_TYPE_OFFSET] != DataType_OctetString) {
				MBUSPICO_LOG_E(LOG_TAG_DEVICE, "Unsupported OBIS header type");
				return abort();
			}

			byte obisCodeLength = plaintext[currentPosition + OBIS_LENGTH_OFFSET];
			MBUSPICO_LOG_D(LOG_TAG_DEVICE, "OBIS code/header length: %d", obisCodeLength);

			if(obisCodeLength != 0x06 && obisCodeLength != 0x0C) {
				MBUSPICO_LOG_E(LOG_TAG_DEVICE, "Unsupported OBIS header length");
				return abort();
			}

			byte obisCode[obisCodeLength];
			memcpy(&obisCode[0], &plaintext[currentPosition + OBIS_CODE_OFFSET], obisCodeLength); // Copy OBIS code to array

			bool timestampFound = false;
			bool meterNumberFound = false;
			// Do not advance Position when reading the Timestamp at DECODER_START_OFFSET
			if ((obisCodeLength == 0x0C) && (currentPosition  == DECODER_START_OFFSET)) {
				timestampFound = true;
			} 
			else if ((currentPosition != DECODER_START_OFFSET) && plaintext[currentPosition - 1] == 0xFF) {
				meterNumberFound = true;
			}
			else {
				currentPosition += obisCodeLength + 2; // Advance past code and position
			}

			byte dataType = plaintext[currentPosition];
			currentPosition++; // Advance past data type

			byte dataLength = 0x00;

			uint16_t codeType = CodeType_Unknown;

			MBUSPICO_LOG_D(LOG_TAG_DEVICE, "obisCode (OBIS_A): %d", obisCode[OBIS_A]);
			MBUSPICO_LOG_D(LOG_TAG_DEVICE, "currentPosition: %d", currentPosition);

			if(obisCode[OBIS_A] == Medium_Electricity) {
				// Compare C and D against code
				if(memcmp(&obisCode[OBIS_C], ESPDM_VOLTAGE_L1, 2) == 0) {
					codeType = CodeType_VoltageL1;
				}
				else if(memcmp(&obisCode[OBIS_C], ESPDM_VOLTAGE_L2, 2) == 0) {
					codeType = CodeType_VoltageL2;
				}
				else if(memcmp(&obisCode[OBIS_C], ESPDM_VOLTAGE_L3, 2) == 0) {
					codeType = CodeType_VoltageL3;
				}
				else if(memcmp(&obisCode[OBIS_C], ESPDM_CURRENT_L1, 2) == 0) {
					codeType = CodeType_CurrentL1;
				}
				else if(memcmp(&obisCode[OBIS_C], ESPDM_CURRENT_L2, 2) == 0) {
					codeType = CodeType_CurrentL2;
				}
				else if(memcmp(&obisCode[OBIS_C], ESPDM_CURRENT_L3, 2) == 0) {
					codeType = CodeType_CurrentL3;
				}
				else if(memcmp(&obisCode[OBIS_C], ESPDM_ACTIVE_POWER_PLUS, 2) == 0) {
					codeType = CodeType_ActivePowerPlus;
				}
				else if(memcmp(&obisCode[OBIS_C], ESPDM_ACTIVE_POWER_MINUS, 2) == 0) {
					codeType = CodeType_ActivePowerMinus;
				}
				else if(memcmp(&obisCode[OBIS_C], ESPDM_POWER_FACTOR, 2) == 0) {
					codeType = CodeType_PowerFactor;
				}
				else if(memcmp(&obisCode[OBIS_C], ESPDM_ACTIVE_ENERGY_PLUS, 2) == 0) {
					codeType = CodeType_ActiveEnergyPlus;
				}
				else if(memcmp(&obisCode[OBIS_C], ESPDM_ACTIVE_ENERGY_MINUS, 2) == 0) {
					codeType = CodeType_ActiveEnergyMinus;
				}
				else if(memcmp(&obisCode[OBIS_C], ESPDM_REACTIVE_ENERGY_PLUS, 2) == 0) {
					codeType = CodeType_ReactiveEnergyPlus;
				}
				else if(memcmp(&obisCode[OBIS_C], ESPDM_REACTIVE_ENERGY_MINUS, 2) == 0) {
					codeType = CodeType_ReactiveEnergyMinus;
				}
				else {
					MBUSPICO_LOG_I(LOG_TAG_DEVICE, "Unsupported OBIS code OBIS_C: %d, OBIS_D: %d", obisCode[OBIS_C], obisCode[OBIS_D]);
				}
			}
			else if(obisCode[OBIS_A] == Medium_Abstract) {
				if(memcmp(&obisCode[OBIS_C], ESPDM_TIMESTAMP, 2) == 0) {
					codeType = CodeType_Timestamp;
				}
				else if(memcmp(&obisCode[OBIS_C], ESPDM_SERIAL_NUMBER, 2) == 0) {
					codeType = CodeType_SerialNumber;
				}
				else if(memcmp(&obisCode[OBIS_C], ESPDM_DEVICE_NAME, 2) == 0) {
					codeType = CodeType_DeviceName;
				}
				else {
					MBUSPICO_LOG_I(LOG_TAG_DEVICE, "Unsupported OBIS code OBIS_C: %d, OBIS_D: %d", obisCode[OBIS_C], obisCode[OBIS_D]);
				}
			}
			// Needed so the Timestamp at DECODER_START_OFFSET gets read correctly
			// as it doesn't have an obisMedium
			else if (timestampFound == true) {
				MBUSPICO_LOG_D(LOG_TAG_DEVICE, "Found Timestamp without obisMedium");
				codeType = CodeType_Timestamp;
			}
			else if (meterNumberFound == true) {
				MBUSPICO_LOG_D(LOG_TAG_DEVICE, "Found MeterNumber without obisMedium");
				codeType = CodeType_MeterNumber;
			}
			else {
				MBUSPICO_LOG_E(LOG_TAG_DEVICE, "Unsupported OBIS medium");
				return abort();
			}

			uint8_t uint8Value;
			uint16_t uint16Value;
			uint32_t uint32Value;
			float floatValue;

			switch(dataType) {
				case DataType_DoubleLongUnsigned:
					dataLength = 4;

					memcpy(&uint32Value, &plaintext[currentPosition], 4); // Copy bytes to integer
					uint32Value = swap_uint32(uint32Value); // Swap bytes

					floatValue = uint32Value; // Ignore decimal digits for now
					
					switch(codeType) {
						case CodeType_ActivePowerPlus:
							meterData.activePowerPlus = floatValue;
							break;
						case CodeType_ActivePowerMinus:
							meterData.activePowerMinus = floatValue;
							break;
						case CodeType_ActiveEnergyPlus:
							meterData.activeEnergyPlus = floatValue;
							break;
						case CodeType_ActiveEnergyMinus:
							meterData.activeEnergyMinus = floatValue;
							break;
						case CodeType_ReactiveEnergyPlus:
							meterData.reactiveEnergyPlus = floatValue;
							break;
						case CodeType_ReactiveEnergyMinus:
							meterData.reactiveEnergyMinus = floatValue;
							break;
					}

				break;
				case DataType_LongUnsigned:
					dataLength = 2;

					memcpy(&uint16Value, &plaintext[currentPosition], 2); // Copy bytes to integer
					uint16Value = swap_uint16(uint16Value); // Swap bytes

					if(plaintext[currentPosition + 5] == Accuracy_SingleDigit) {
						floatValue = uint16Value / 10.0; // Divide by 10 to get decimal places
					}
					else if(plaintext[currentPosition + 5] == Accuracy_DoubleDigit) {
						floatValue = uint16Value / 100.0; // Divide by 100 to get decimal places
					}
					else {
						floatValue = uint16Value; // No decimal places
					}
					
					switch(codeType) {
						case CodeType_VoltageL1:
							meterData.voltageL1 = floatValue;
							break;
						case CodeType_VoltageL2:
							meterData.voltageL2 = floatValue;
							break;
						case CodeType_VoltageL3:
							meterData.voltageL3 = floatValue;
							break;
						case CodeType_CurrentL1:
							meterData.currentL1 = floatValue;
							break;
						case CodeType_CurrentL2:
							meterData.currentL2 = floatValue;
							break;
						case CodeType_CurrentL3:
							meterData.currentL3 = floatValue;
							break;
						case CodeType_PowerFactor:
							meterData.powerFactor = floatValue;
							break;
					}

				break;
				case DataType_OctetString:
					MBUSPICO_LOG_D(LOG_TAG_DEVICE, "Arrived on OctetString");
					MBUSPICO_LOG_D(LOG_TAG_DEVICE, "currentPosition: %d, plaintext: %d", currentPosition, plaintext[currentPosition]);

					dataLength = plaintext[currentPosition];
					currentPosition++; // Advance past string length

					if(codeType == CodeType_Timestamp) { // Handle timestamp generation
						uint16_t year;
						uint8_t month;
						uint8_t day;

						uint8_t hour;
						uint8_t minute;
						uint8_t second;

						memcpy(&uint16Value, &plaintext[currentPosition], 2);
						year = swap_uint16(uint16Value);

						memcpy(&month, &plaintext[currentPosition + 2], 1);
						memcpy(&day, &plaintext[currentPosition + 3], 1);

						memcpy(&hour, &plaintext[currentPosition + 5], 1);
						memcpy(&minute, &plaintext[currentPosition + 6], 1);
						memcpy(&second, &plaintext[currentPosition + 7], 1);

						sprintf(meterData.timestamp, "%04u-%02u-%02uT%02u:%02u:%02uZ", year, month, day, hour, minute, second);

						meterData.lxTimestamp = timestamp_to_lx(year, month, day, hour, minute, second);
					}
					else if(codeType == CodeType_MeterNumber) {
						MBUSPICO_LOG_D(LOG_TAG_DEVICE, "Constructing MeterNumber...");
						memcpy(meterData.meterNumber, &plaintext[currentPosition], dataLength);
						meterData.meterNumber[12] = '\0';
					}

				break;
				default:
					MBUSPICO_LOG_E(LOG_TAG_DEVICE, "Unsupported OBIS data type");
					return abort();
				break;
			}

			currentPosition += dataLength; // Skip data length

			// Don't skip the break on the first Timestamp, as there's none
			if(timestampFound == false) {
				currentPosition += 2; // Skip break after data
			}

			if(plaintext[currentPosition] == 0x0F) { // There is still additional data for this type, skip it
				//currentPosition += 6; // Skip additional data and additional break; this will jump out of bounds on last frame
				// on EVN Meters the additional data (no additional Break) is only 3 Bytes + 1 Byte for the "there is data" Byte
				currentPosition += 4; // Skip additional data and additional break; this will jump out of bounds on last frame
			}
		}
		while (currentPosition <= payloadLength); // Loop until arrived at end

		receiveBufferIndex = 0;
		
		MBUSPICO_LOG_I(LOG_TAG_DEVICE, "Received valid data");
		mbuspico_set_meterdata(&meterData);
	}
}

const char* mbuspico_device_name() {
	return "Kaifa MA309M (NetzNÖ)";
}

static int mbuspico_device_init() {
#ifdef MBUSPICO_DEVICE_KEY
	keyLength = strlen(MBUSPICO_DEVICE_KEY);
	if (keyLength == 32) { // hex string of 16 bytes (=32 characters)
		mbuspico_hex_to_bin(MBUSPICO_DEVICE_KEY, keyLength, key);
		keyLength /= 2;
		return 0;
	}
	else {
		MBUSPICO_LOG_E(LOG_TAG_DEVICE, "Device encryption key must be exactly 32 hex-characters, got %d", keyLength);
	}
#else
	#error "MBUSPICO_DEVICE_KEY not defined. Not specified via options?"
#endif
	return 1;
}

void mbuspico_device_task(void* arg) {
	MBUSPICO_LOG_D(LOG_TAG_DEVICE, "mbuspico_device_task()");

	if (mbuspico_device_init()) {
		vTaskDelete(NULL);
		return;
	}
	
	for (;;) {
		loop();
		vTaskDelay(pdMS_TO_TICKS(50));
	}
}
