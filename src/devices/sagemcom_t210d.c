#include <mbuspico.h>

#include <ctype.h>
#include <mbedtls/gcm.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define DLMS_HEADER1_LENGTH 26
#define DLMS_HEADER2_OFFSET 256
#define DLMS_HEADER2_LENGTH 9
#define DLMS_SYST_OFFSET 11
#define DLMS_SYST_LENGTH 8
#define DLMS_IC_OFFSET 22
#define DLMS_IC_LENGTH 4
#define READ_BUFFER_SIZE 1024
#define MBUS_START 0x68
#define MBUS_STOP 0x16
#define FRAME1_LENGTH 256
#define FRAME2_LENGTH 26

#define DATA_TYPE_OCTET_STRING 0x09
#define DATA_TYPE_DOUBLE_LONG_UNSIGNED 0x06
#define DATA_TYPE_LONG_UNSIGNED 0x12

static const byte OBIS_ACTIVE_POWER_PLUS[] = {0x01, 0x00, 0x01, 0x07, 0x00, 0xFF};
static const byte OBIS_ACTIVE_POWER_MINUS[] = {0x01, 0x00, 0x02, 0x07, 0x00, 0xFF};
static const byte OBIS_ACTIVE_ENERGY_PLUS[] = {0x01, 0x00, 0x01, 0x08, 0x00, 0xFF};
static const byte OBIS_ACTIVE_ENERGY_MINUS[] = {0x01, 0x00, 0x02, 0x08, 0x00, 0xFF};
static const byte OBIS_REACTIVE_ENERGY_PLUS[] = {0x01, 0x00, 0x03, 0x08, 0x00, 0xFF};
static const byte OBIS_REACTIVE_ENERGY_MINUS[] = {0x01, 0x00, 0x04, 0x08, 0x00, 0xFF};
static const byte OBIS_VOLTAGE_L1[] = {0x01, 0x00, 0x20, 0x07, 0x00, 0xFF};
static const byte OBIS_VOLTAGE_L2[] = {0x01, 0x00, 0x34, 0x07, 0x00, 0xFF};
static const byte OBIS_VOLTAGE_L3[] = {0x01, 0x00, 0x48, 0x07, 0x00, 0xFF};
static const byte OBIS_CURRENT_L1[] = {0x01, 0x00, 0x1F, 0x07, 0x00, 0xFF};
static const byte OBIS_CURRENT_L2[] = {0x01, 0x00, 0x33, 0x07, 0x00, 0xFF};
static const byte OBIS_CURRENT_L3[] = {0x01, 0x00, 0x47, 0x07, 0x00, 0xFF};
static const byte OBIS_POWER_FACTOR[] = {0x01, 0x00, 0x0D, 0x07, 0x00, 0xFF};
static const byte OBIS_METER_NUMBER[] = {0x00, 0x00, 0x60, 0x01, 0x00, 0xFF};

static mbedtls_gcm_context aes;

static int receiveBufferIndex = 0;
static byte receiveBuffer[READ_BUFFER_SIZE];
static unsigned long lastRead = 0;

static byte key[16];
static size_t keyLength = 0;
static byte pendingFrame1[FRAME1_LENGTH];
static size_t pendingFrame1Length = 0;

MBusPicoUARTConfig_t mbuspico_device_uart_config(void) {
	MBusPicoUARTConfig_t config = {2400, 8, 1, MBUSPICO_UART_PARITY_EVEN};
	return config;
}

static void abort_receive(void) {
	receiveBufferIndex = 0;
	pendingFrame1Length = 0;
}

static void log_packet(const byte array[], size_t length) {
	char buffer[(READ_BUFFER_SIZE * 3) + 1];
	if (length > READ_BUFFER_SIZE) {
		length = READ_BUFFER_SIZE;
	}

	for (unsigned int i = 0; i < length; ++i) {
		byte nib1 = (array[i] >> 4) & 0x0F;
		byte nib2 = (array[i] >> 0) & 0x0F;
		buffer[i * 3] = nib1 < 0xA ? '0' + nib1 : 'A' + nib1 - 0xA;
		buffer[i * 3 + 1] = nib2 < 0xA ? '0' + nib2 : 'A' + nib2 - 0xA;
		buffer[i * 3 + 2] = ' ';
	}

	buffer[length * 3] = '\0';
	MBUSPICO_LOG_D(LOG_TAG_DEVICE, buffer);
}

static time_t timestamp_to_lx(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second) {
	static time_t lxTime = 0;
	if (lxTime <= 0) {
		struct tm t = {0};
		t.tm_year = 2009 - 1900;
		t.tm_mon = 1 - 1;
		t.tm_mday = 1;
		t.tm_isdst = -1;
		lxTime = mktime(&t);
		if (lxTime <= 0) {
			MBUSPICO_LOG_E(LOG_TAG_DEVICE, "Loxone timestamp initialization failed: %lld", lxTime);
			return 0;
		}
	}

	struct tm t = {0};
	t.tm_year = year - 1900;
	t.tm_mon = month - 1;
	t.tm_mday = day;
	t.tm_hour = hour;
	t.tm_min = minute;
	t.tm_sec = second;
	t.tm_isdst = -1;
	time_t timestamp = mktime(&t);
	if (timestamp <= 0 || timestamp < lxTime) {
		MBUSPICO_LOG_E(LOG_TAG_DEVICE, "Invalid timestamp: '%d-%02d-%02d %02d:%02d:%02d'", year, month, day, hour, minute, second);
		return 0;
	}

	return timestamp - lxTime;
}

static int available(xMBusData_t* data) {
	return xQueueReceive(g_DeviceEventQueue, data, 0) == pdPASS ? 1 : 0;
}

static int find_pattern(const byte* data, size_t length, const byte* pattern, size_t patternLength) {
	if (patternLength == 0 || patternLength > length) {
		return -1;
	}

	for (size_t i = 0; i + patternLength <= length; ++i) {
		if (memcmp(&data[i], pattern, patternLength) == 0) {
			return (int)i;
		}
	}
	return -1;
}

static int find_obis_numeric_value(const byte* data, size_t length, const byte* obis, size_t obisLength, uint32_t* value) {
	byte pattern[8] = {DATA_TYPE_OCTET_STRING, 0x06, 0, 0, 0, 0, 0, 0};
	memcpy(&pattern[2], obis, obisLength);
	int pos = find_pattern(data, length, pattern, sizeof(pattern));
	if (pos < 0) {
		return 0;
	}

	pos += (int)sizeof(pattern);
	if ((size_t)pos >= length) {
		return 0;
	}

	if (data[pos] == DATA_TYPE_DOUBLE_LONG_UNSIGNED) {
		if ((size_t)pos + 5 > length) {
			return 0;
		}
		*value = ((uint32_t)data[pos + 1] << 24) | ((uint32_t)data[pos + 2] << 16) | ((uint32_t)data[pos + 3] << 8) | (uint32_t)data[pos + 4];
		return 1;
	}
	if (data[pos] == DATA_TYPE_LONG_UNSIGNED) {
		if ((size_t)pos + 3 > length) {
			return 0;
		}
		*value = ((uint32_t)data[pos + 1] << 8) | (uint32_t)data[pos + 2];
		return 1;
	}
	return 0;
}

static void parse_timestamp(const byte* data, size_t length, MeterData_t* meterData) {
	static const byte pattern[] = {DATA_TYPE_OCTET_STRING, 0x0C};
	int pos = find_pattern(data, length, pattern, sizeof(pattern));
	if (pos < 0 || (size_t)pos + 14 > length) {
		return;
	}

	const byte* ts = &data[pos + 2];
	uint16_t year = ((uint16_t)ts[0] << 8) | ts[1];
	uint8_t month = ts[2];
	uint8_t day = ts[3];
	uint8_t hour = ts[5];
	uint8_t minute = ts[6];
	uint8_t second = ts[7];

	snprintf(meterData->timestamp, sizeof(meterData->timestamp), "%04d-%02d-%02dT%02d:%02d:%02dZ", year, month, day, hour, minute, second);
	meterData->lxTimestamp = timestamp_to_lx(year, month, day, hour, minute, second);
}

static void parse_meter_number(const byte* data, size_t length, MeterData_t* meterData) {
	byte pattern[8] = {DATA_TYPE_OCTET_STRING, 0x06, 0, 0, 0, 0, 0, 0};
	memcpy(&pattern[2], OBIS_METER_NUMBER, sizeof(OBIS_METER_NUMBER));
	int pos = find_pattern(data, length, pattern, sizeof(pattern));
	if (pos < 0) {
		return;
	}

	pos += (int)sizeof(pattern);
	if ((size_t)pos + 2 > length || data[pos] != DATA_TYPE_OCTET_STRING) {
		return;
	}

	byte strLen = data[pos + 1];
	if ((size_t)pos + 2 + strLen > length) {
		return;
	}

	char digits[13] = {0};
	size_t j = 0;
	for (size_t i = 0; i < strLen && j + 1 < sizeof(digits); ++i) {
		char ch = (char)data[pos + 2 + i];
		if (isdigit((unsigned char)ch)) {
			digits[j++] = ch;
		}
	}
	if (j > 0) {
		strncpy(meterData->meterNumber, digits, sizeof(meterData->meterNumber) - 1);
	}
}

static void parse_obis_values(const byte* plaintext, size_t plaintextLength, MeterData_t* meterData) {
	struct {
		const byte* obis;
		size_t obisLength;
		float divisor;
		float* field;
	} fields[] = {
		{OBIS_ACTIVE_POWER_PLUS, sizeof(OBIS_ACTIVE_POWER_PLUS), 1.0f, &meterData->activePowerPlus},
		{OBIS_ACTIVE_POWER_MINUS, sizeof(OBIS_ACTIVE_POWER_MINUS), 1.0f, &meterData->activePowerMinus},
		{OBIS_ACTIVE_ENERGY_PLUS, sizeof(OBIS_ACTIVE_ENERGY_PLUS), 1.0f, &meterData->activeEnergyPlus},
		{OBIS_ACTIVE_ENERGY_MINUS, sizeof(OBIS_ACTIVE_ENERGY_MINUS), 1.0f, &meterData->activeEnergyMinus},
		{OBIS_REACTIVE_ENERGY_PLUS, sizeof(OBIS_REACTIVE_ENERGY_PLUS), 1.0f, &meterData->reactiveEnergyPlus},
		{OBIS_REACTIVE_ENERGY_MINUS, sizeof(OBIS_REACTIVE_ENERGY_MINUS), 1.0f, &meterData->reactiveEnergyMinus},
		{OBIS_VOLTAGE_L1, sizeof(OBIS_VOLTAGE_L1), 10.0f, &meterData->voltageL1},
		{OBIS_VOLTAGE_L2, sizeof(OBIS_VOLTAGE_L2), 10.0f, &meterData->voltageL2},
		{OBIS_VOLTAGE_L3, sizeof(OBIS_VOLTAGE_L3), 10.0f, &meterData->voltageL3},
		{OBIS_CURRENT_L1, sizeof(OBIS_CURRENT_L1), 100.0f, &meterData->currentL1},
		{OBIS_CURRENT_L2, sizeof(OBIS_CURRENT_L2), 100.0f, &meterData->currentL2},
		{OBIS_CURRENT_L3, sizeof(OBIS_CURRENT_L3), 100.0f, &meterData->currentL3},
		{OBIS_POWER_FACTOR, sizeof(OBIS_POWER_FACTOR), 1000.0f, &meterData->powerFactor},
	};

	for (size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); ++i) {
		uint32_t rawValue = 0;
		if (find_obis_numeric_value(plaintext, plaintextLength, fields[i].obis, fields[i].obisLength, &rawValue)) {
			*fields[i].field = (float)rawValue / fields[i].divisor;
		}
	}
}

static int extract_mbus_frame(byte* frame, size_t* frameLength) {
	int start = -1;
	for (int i = 0; i < receiveBufferIndex; ++i) {
		if (receiveBuffer[i] == MBUS_START) {
			start = i;
			break;
		}
	}
	if (start < 0) {
		receiveBufferIndex = 0;
		return 0;
	}
	if (start > 0) {
		memmove(receiveBuffer, &receiveBuffer[start], receiveBufferIndex - start);
		receiveBufferIndex -= start;
	}

	if (receiveBufferIndex < 4) {
		return 0;
	}
	if (receiveBuffer[1] != receiveBuffer[2] || receiveBuffer[3] != MBUS_START) {
		memmove(receiveBuffer, &receiveBuffer[1], receiveBufferIndex - 1);
		receiveBufferIndex -= 1;
		return 0;
	}

	size_t totalLength = 4 + receiveBuffer[1] + 2;
	if (totalLength > READ_BUFFER_SIZE) {
		memmove(receiveBuffer, &receiveBuffer[1], receiveBufferIndex - 1);
		receiveBufferIndex -= 1;
		return 0;
	}
	if ((size_t)receiveBufferIndex < totalLength) {
		return 0;
	}
	if (receiveBuffer[totalLength - 1] != MBUS_STOP) {
		memmove(receiveBuffer, &receiveBuffer[1], receiveBufferIndex - 1);
		receiveBufferIndex -= 1;
		return 0;
	}

	memcpy(frame, receiveBuffer, totalLength);
	memmove(receiveBuffer, &receiveBuffer[totalLength], receiveBufferIndex - totalLength);
	receiveBufferIndex -= (int)totalLength;
	*frameLength = totalLength;
	return 1;
}

static int handle_packet(const byte* data, size_t dataLength) {
	const uint16_t payloadLength = 243;
	const uint16_t payloadLength1 = 228;
	const uint16_t payloadLength2 = payloadLength - payloadLength1;

	if (dataLength < FRAME1_LENGTH) {
		MBUSPICO_LOG_E(LOG_TAG_DEVICE, "Received packet with invalid size");
		return 0;
	}
	if (dataLength <= payloadLength || payloadLength2 >= dataLength - DLMS_HEADER2_OFFSET - DLMS_HEADER2_LENGTH) {
		MBUSPICO_LOG_E(LOG_TAG_DEVICE, "Payload length is too big for received data");
		return 0;
	}

	MBUSPICO_LOG_D(LOG_TAG_DEVICE, "Handling packet");
	log_packet(data, dataLength);

	byte iv[12] = {0};
	memcpy(&iv[0], &data[DLMS_SYST_OFFSET], DLMS_SYST_LENGTH);
	memcpy(&iv[8], &data[DLMS_IC_OFFSET], DLMS_IC_LENGTH);

	byte ciphertext[payloadLength];
	memcpy(&ciphertext[0], &data[DLMS_HEADER1_LENGTH], payloadLength1);
	memcpy(&ciphertext[payloadLength1], &data[DLMS_HEADER2_OFFSET + DLMS_HEADER2_LENGTH], payloadLength2);

	byte plaintext[payloadLength];
	mbedtls_gcm_init(&aes);
	mbedtls_gcm_setkey(&aes, MBEDTLS_CIPHER_ID_AES, key, keyLength * 8);
	mbedtls_gcm_auth_decrypt(&aes, payloadLength, iv, sizeof(iv), NULL, 0, NULL, 0, ciphertext, plaintext);
	mbedtls_gcm_free(&aes);

	if (plaintext[0] != 0x0F || plaintext[5] != 0x0C) {
		MBUSPICO_LOG_E(LOG_TAG_DEVICE, "Packet was decrypted but data is invalid");
		return 0;
	}

	MeterData_t meterData;
	memset(&meterData, 0, sizeof(MeterData_t));
	parse_timestamp(plaintext, payloadLength, &meterData);
	parse_meter_number(plaintext, payloadLength, &meterData);
	parse_obis_values(plaintext, payloadLength, &meterData);

	MBUSPICO_LOG_I(LOG_TAG_DEVICE, "Received valid data");
	mbuspico_set_meterdata(&meterData);
	return 1;
}

static void handle_frame(const byte* frame, size_t frameLength) {
	if (frameLength == FRAME1_LENGTH) {
		memcpy(pendingFrame1, frame, frameLength);
		pendingFrame1Length = frameLength;
		MBUSPICO_LOG_D(LOG_TAG_DEVICE, "Buffered frame 1, waiting for frame 2");
		return;
	}
	if (frameLength == FRAME2_LENGTH && pendingFrame1Length == FRAME1_LENGTH) {
		byte packet[FRAME1_LENGTH + FRAME2_LENGTH];
		memcpy(packet, pendingFrame1, pendingFrame1Length);
		memcpy(&packet[pendingFrame1Length], frame, frameLength);
		pendingFrame1Length = 0;
		handle_packet(packet, sizeof(packet));
		return;
	}
	if (frameLength == FRAME2_LENGTH) {
		MBUSPICO_LOG_E(LOG_TAG_DEVICE, "Received frame 2 without frame 1");
		return;
	}

	MBUSPICO_LOG_E(LOG_TAG_DEVICE, "Unexpected M-Bus frame length: %u", (unsigned int)frameLength);
}

static void loop(void) {
	uint64_t currentTime = mbuspico_time_ms();

	xMBusData_t d;
	while (available(&d)) {
		if (receiveBufferIndex + (int)d.len > READ_BUFFER_SIZE) {
			MBUSPICO_LOG_E(LOG_TAG_DEVICE, "Buffer overflow");
			abort_receive();
			break;
		}
		for (int i = 0; i < d.len; ++i) {
			receiveBuffer[receiveBufferIndex++] = d.data[i];
		}
		lastRead = currentTime;
	}

	byte frame[READ_BUFFER_SIZE] = {0};
	size_t frameLength = 0;
	while (extract_mbus_frame(frame, &frameLength)) {
		handle_frame(frame, frameLength);
	}

	if (receiveBufferIndex > 0 && currentTime - lastRead > 2500) {
		MBUSPICO_LOG_E(LOG_TAG_DEVICE, "Timed out waiting for complete M-Bus frame");
		receiveBufferIndex = 0;
	}
}

const char* mbuspico_device_name(void) {
	return "Sagemcom T210-D (NetzNOe)";
}

static int mbuspico_device_init(void) {
#ifdef MBUSPICO_DEVICE_KEY
	keyLength = strlen(MBUSPICO_DEVICE_KEY);
	if (keyLength == 32) {
		mbuspico_hex_to_bin(MBUSPICO_DEVICE_KEY, keyLength, key);
		keyLength /= 2;
		return 0;
	}
	MBUSPICO_LOG_E(LOG_TAG_DEVICE, "Device encryption key must be exactly 32 hex-characters, got %u", (unsigned int)keyLength);
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
