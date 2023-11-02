#include <mbuspico.h>
#include <stdio.h>
#include <semphr.h>
#include <string.h>
#include <pico/bootrom.h>
#include <hardware/watchdog.h>
#include <hardware/regs/rosc.h>
#include <hardware/regs/addressmap.h>
#include <hardware/flash.h>
#include <hardware/regs/addressmap.h>
#include <hardware/sync.h>
#include <mbedtls_config.h>
#include <timers.h>
#if defined(MBUSPICO_HTTP_ENABLED) || defined(MBUSPICO_UDP_ENABLED) 
# include <mongoose.h>
#endif

#include <lfs.h>
#include <lfs_util.h>

QueueHandle_t g_DeviceEventQueue = NULL;

static lfs_t g_MBUSPICO_FS;

static SemaphoreHandle_t g_ValueMutex = NULL;
static SemaphoreHandle_t g_LogMutex = NULL;
static SemaphoreHandle_t g_FsMutex = NULL; // recursive mutex for filesystem access
static MeterData_t g_MeterData;

static TimerHandle_t hRebootTimer = NULL;
static TimerHandle_t hRebootUsbTimer = NULL;

void mbuspico_print_meterdata(void) {
	if (xSemaphoreTake(g_ValueMutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
		MBUSPICO_LOG_D(LOG_TAG_MAIN,
				"\n"
				"Timestamp: %s" "\n"
				"Loxone Timestamp: %lld" "\n"
				"Meter #: %s" "\n"
				"activePowerPlus: %.2f [W]" "\n"
				"activePowerMinus: %.2f [W]" "\n"
				"activeEnergyPlus: %.2f [Wh]" "\n"
				"activeEnergyMinus: %.2f [Wh]" "\n"
				//"reactiveEnergyPlus: %.2f" "\n"
				//"reactiveEnergyMinus: %.2f" "\n"
				"voltageL1: %.2f [V]" "\n"
				"voltageL2: %.2f [V]" "\n"
				"voltageL3: %.2f [V]" "\n"
				"currentL1: %.2f [A]" "\n"
				"currentL2: %.2f [A]" "\n"
				"currentL3: %.2f [A]" "\n"
				"powerFactor: %.2f" "\n"
				"================" "\n",
				g_MeterData.timestamp,
				g_MeterData.lxTimestamp,
				g_MeterData.meterNumber,
				g_MeterData.activePowerPlus,
				g_MeterData.activePowerMinus,
				g_MeterData.activeEnergyPlus,
				g_MeterData.activeEnergyMinus,
				//g_MeterData.reactiveEnergyPlus,
				//g_MeterData.reactiveEnergyMinus,
				g_MeterData.voltageL1,
				g_MeterData.voltageL2,
				g_MeterData.voltageL3,
				g_MeterData.currentL1,
				g_MeterData.currentL2,
				g_MeterData.currentL3,
				g_MeterData.powerFactor
			);
		xSemaphoreGive(g_ValueMutex);
	}
	else {
		MBUSPICO_LOG_D(LOG_TAG_MAIN, "Failed to aquire semaphore in mbuspico_print_meterdata()");
	}
}

size_t mbuspico_get_meterdata_json(char* data_buffer, size_t buffer_size) {
	size_t data_len = 0;
	MBUSPICO_LOG_D(LOG_TAG_MAIN, "mbuspico_get_meterdata_json() called");
	if (xSemaphoreTake(g_ValueMutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
		data_len = snprintf(
				data_buffer, buffer_size,
				"{" "\n"
				"\"timestamp\": \"%s\"," "\n"
				"\"lxTimestamp\": %lld," "\n"
				"\"meterNumber\": \"%s\"," "\n"
				"\"activePowerPlus\": %.2f," "\n"
				"\"activePowerMinus\": %.2f," "\n"
				"\"activeEnergyPlus\": %.2f," "\n"
				"\"activeEnergyMinus\": %.2f," "\n"
				"\"voltageL1\": %.2f," "\n"
				"\"voltageL2\": %.2f," "\n"
				"\"voltageL3\": %.2f," "\n"
				"\"currentL1\": %.2f," "\n"
				"\"currentL2\": %.2f," "\n"
				"\"currentL3\": %.2f," "\n"
				"\"powerFactor\": %.2f" "\n"
				"}",
				g_MeterData.timestamp,
				g_MeterData.lxTimestamp,
				g_MeterData.meterNumber,
				g_MeterData.activePowerPlus,
				g_MeterData.activePowerMinus,
				g_MeterData.activeEnergyPlus,
				g_MeterData.activeEnergyMinus,
				g_MeterData.voltageL1,
				g_MeterData.voltageL2,
				g_MeterData.voltageL3,
				g_MeterData.currentL1,
				g_MeterData.currentL2,
				g_MeterData.currentL3,
				g_MeterData.powerFactor
			);
		xSemaphoreGive(g_ValueMutex);
	}
	else {
		MBUSPICO_LOG_E(LOG_TAG_MAIN, "Failed to aquire semaphore in mbuspico_get_meterdata_json()");
	}
	return data_len;
}

void mbuspico_set_meterdata(MeterData_t* data) {
	MBUSPICO_LOG_D(LOG_TAG_MAIN, "mbuspico_set_meterdata() called");
	if (xSemaphoreTake(g_ValueMutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
		memcpy(&g_MeterData, data, sizeof(MeterData_t));
		xSemaphoreGive(g_ValueMutex);
#if MBUSPICO_LOG_LEVEL >= MBUSPICO_LOG_DEBUG
		mbuspico_print_meterdata();
#endif
	}
	else {
		MBUSPICO_LOG_E(LOG_TAG_MAIN, "Failed to aquire semaphore in mbuspico_set_meterdata()");
	}
}

#ifdef MONGOOSE_H
static void mongoose_log_redirect(char ch, void *param) {
	static char buf[256];
	static size_t len = 0;
	if (ch != '\n') {
		buf[len++] = ch;
	}
	if (ch == '\n' || len >= sizeof(buf)) {
		MBUSPICO_LOG_D(LOG_TAG_MG, buf);
		len = 0;
		memset(buf, sizeof(buf), 0);
	}
}
#endif

// FILESYSTEM

// filesystem size in Pico flash
#define MBUSPICO_FS_SIZE (128 * 1024) // 64 KB

#define MBUSPICO_FS_CONFIG_PATH "/config"

// file system offset in flash
static const char* g_MBUSPICO_FS_BASE = (char*)(PICO_FLASH_SIZE_BYTES - MBUSPICO_FS_SIZE);

int (*read)(const struct lfs_config *c, lfs_block_t block,
			lfs_off_t off, void *buffer, lfs_size_t size);

	// Program a region in a block. The block must have previously
	// been erased. Negative error codes are propagated to the user.
	// May return LFS_ERR_CORRUPT if the block should be considered bad.
	int (*prog)(const struct lfs_config *c, lfs_block_t block,
			lfs_off_t off, const void *buffer, lfs_size_t size);

	// Erase a block. A block must be erased before being programmed.
	// The state of an erased block is undefined. Negative error codes
	// are propagated to the user.
	// May return LFS_ERR_CORRUPT if the block should be considered bad.
	int (*erase)(const struct lfs_config *c, lfs_block_t block);



static int _fs_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void* buffer, lfs_size_t size);
static int _fs_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void* buffer, lfs_size_t size);
static int _fs_erase(const struct lfs_config *c, lfs_block_t block);
static int _fs_lock(const struct lfs_config *c);
static int _fs_unlock(const struct lfs_config *c);

static struct lfs_config g_MBUSPICO_FS_CFG = {
	// block device operations
	.read = _fs_read,
	.prog = _fs_prog,
	.erase = _fs_erase,
	.lock = _fs_lock,
	.unlock = _fs_unlock,
	// block device configuration
	.read_size = 1,
	.prog_size = FLASH_PAGE_SIZE,
	.block_size = FLASH_SECTOR_SIZE,
	.block_count = MBUSPICO_FS_SIZE / FLASH_SECTOR_SIZE,
	.cache_size = FLASH_SECTOR_SIZE / 4,
	.lookahead_size = 32,
	.block_cycles = 500
};

static int _fs_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void* buffer, lfs_size_t size) {
	assert(block < c->block_count);
	assert(off + size <= c->block_size);
	// read flash via XIP mapped space
	memcpy(buffer, g_MBUSPICO_FS_BASE + XIP_NOCACHE_NOALLOC_BASE + (block * c->block_size) + off, size);
	return LFS_ERR_OK;
}

static int _fs_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void* buffer, lfs_size_t size) {
	assert(block < c->block_count);
	// program with Pico SDK
	uint32_t p = (uint32_t)g_MBUSPICO_FS_BASE + (block * c->block_size) + off;
	uint32_t ints = save_and_disable_interrupts();
	flash_range_program(p, buffer, size);
	restore_interrupts(ints);
	return LFS_ERR_OK;
}

static int _fs_erase(const struct lfs_config *c, lfs_block_t block) {
	assert(block < c->block_count);
	// erase with Pico SDK
	uint32_t p = (uint32_t)g_MBUSPICO_FS_BASE + (block * c->block_size);
	uint32_t ints = save_and_disable_interrupts();
	flash_range_erase(p, c->block_size);
	restore_interrupts(ints);
	return LFS_ERR_OK;
}

static int _fs_lock(const struct lfs_config *c) {
	return xSemaphoreTakeRecursive(g_FsMutex, 10) == pdTRUE ? LFS_ERR_OK : LFS_ERR_IO;
}

static int _fs_unlock(const struct lfs_config *c) {
	return xSemaphoreGiveRecursive(g_FsMutex) == pdTRUE ? LFS_ERR_OK : LFS_ERR_IO;
}

// MBUSPICO INTERFACE

void mbuspico_init(void) {
	memset(&g_MeterData, 0, sizeof(MeterData_t));

	g_ValueMutex = xSemaphoreCreateBinary();
	xSemaphoreGive(g_ValueMutex);
	
	g_LogMutex = xSemaphoreCreateBinary();
	xSemaphoreGive(g_LogMutex);

	g_DeviceEventQueue = xQueueCreate(15, sizeof(xMBusData_t));

	// Filesystem
	g_FsMutex = xSemaphoreCreateRecursiveMutex();
	xSemaphoreGive(g_FsMutex);

	int fs_err = lfs_mount(&g_MBUSPICO_FS, &g_MBUSPICO_FS_CFG);
	// reformat if we can't mount the filesystem - this should only happen on the first boot
	if (fs_err) {
	#if MBUSPICO_LOG_LEVEL >= LOG_ERROR
		printf("Failed initialization of filesystem, formatting...\n");
	#endif
		lfs_format(&g_MBUSPICO_FS, &g_MBUSPICO_FS_CFG);
		fs_err = lfs_mount(&g_MBUSPICO_FS, &g_MBUSPICO_FS_CFG);
	}
	if (fs_err) {
	#if MBUSPICO_LOG_LEVEL >= LOG_ERROR
		printf("Failed initialization of filesystem!\n");
	#endif
	}
	else {
	#if MBUSPICO_LOG_LEVEL >= LOG_INFO
		printf("Filesystem initialized successfully!\n");
	#endif
	}

	// Mongoose
#ifdef MONGOOSE_H
	mg_log_set_fn(mongoose_log_redirect, NULL);

#if MBUSPICO_LOG_LEVEL >= LOG_DEBUG
	mg_log_set(MG_LL_DEBUG);
#elif MBUSPICO_LOG_LEVEL >= LOG_INFO
	mg_log_set(MG_LL_INFO);
#elif MBUSPICO_LOG_LEVEL >= LOG_ERROR
	mg_log_set(MG_LL_ERROR);
#else
	mg_log_set(MG_LL_NONE);
#endif
#endif
}

static void execute_reboot_timer(TimerHandle_t hTimer) {
	if (hTimer == hRebootTimer) {
		mbuspico_reboot();
	}
	else if (hTimer == hRebootUsbTimer) {
		mbuspico_reboot_into_bootloader();
	}
}

int mbuspico_schedule_reboot(uint32_t ms) {
	hRebootTimer = xTimerCreate("Reboot_Timer", pdMS_TO_TICKS(ms), pdFALSE, NULL, execute_reboot_timer);
	if (hRebootTimer == NULL) {
	#if MBUSPICO_LOG_LEVEL >= LOG_ERROR
		printf("Failed to create REBOOT Timer!\n");
	#endif
	}
	else {
		BaseType_t result = xTimerStart(hRebootTimer, 0);
		if (result == pdPASS) {
			return 1;
		}
		else {
		#if MBUSPICO_LOG_LEVEL >= LOG_ERROR
			printf("Failed to start REBOOT Timer!\n");
		#endif
			xTimerDelete(hRebootTimer, 0);
			hRebootTimer = NULL;
		}
	}
	return 0;
}

int mbuspico_schedule_reboot_usb(uint32_t ms) {
	hRebootUsbTimer = xTimerCreate("RebootUsb_Timer", pdMS_TO_TICKS(ms), pdFALSE, NULL, execute_reboot_timer);
	if (hRebootUsbTimer == NULL) {
	#if MBUSPICO_LOG_LEVEL >= LOG_ERROR
		printf("Failed to create REBOOT-USB Timer!\n");
	#endif
	}
	else {
		BaseType_t result = xTimerStart(hRebootUsbTimer, 0);
		if (result == pdPASS) {
			return 1;
		}
		else {
		#if MBUSPICO_LOG_LEVEL >= LOG_ERROR
			printf("Failed to start REBOOT-USB Timer!\n");
		#endif
			xTimerDelete(hRebootUsbTimer, 0);
			hRebootUsbTimer = NULL;
		}
	}
	return 0;
}

uint64_t mbuspico_time_ms(void) {
	uint64_t us = time_us_64();	
	return us/1000;
}

// taken from https://stackoverflow.com/a/53579348
void mbuspico_hex_to_bin(const char* in, size_t len, byte* out) {
	static const unsigned char TBL[] = {
		0,   1,   2,   3,   4,   5,   6,   7,   8,   9,  58,  59,  60,  61,
		62,  63,  64,  10,  11,  12,  13,  14,  15,  71,  72,  73,  74,  75,
		76,  77,  78,  79,  80,  81,  82,  83,  84,  85,  86,  87,  88,  89,
		90,  91,  92,  93,  94,  95,  96,  10,  11,  12,  13,  14,  15
	};

	static const unsigned char *LOOKUP = TBL - 48;

	const char* end = in + len;
	while(in < end) *(out++) = LOOKUP[*(in++)] << 4 | LOOKUP[*(in++)];
}

void mbuspico_reboot() {
	watchdog_reboot	(0, 0, 1000);
}

void mbuspico_reboot_into_bootloader() {
	reset_usb_boot(0,0);
}

// ENTROPY RNG
#ifdef MBEDTLS_ENTROPY_HARDWARE_ALT
// based on https://forums.raspberrypi.com/viewtopic.php?t=302960
int mbedtls_hardware_poll (void *data, unsigned char *output, size_t len, size_t *olen) {
	int k, i, rnd = 0;
	volatile uint32_t *rnd_reg=(uint32_t *)(ROSC_BASE + ROSC_RANDOMBIT_OFFSET);
	for (k = 0; k < 32; k++) {
		rnd = rnd << 1;
		rnd = rnd + (0x00000001 & (*rnd_reg));
	}
	for (i = 0; i < len; i++) {
		output[i] = ((unsigned char*)&rnd)[i%sizeof(rnd)];
	}
	*olen = len;
	return 0;
}
#endif

// CONFIG

static void _mbuspico_file_error_str(int err, char* err_str, uint32_t size) {
	switch (err) {
		case LFS_ERR_OK:			snprintf(err_str, size, "(%d) No error"); break;
		case LFS_ERR_IO:			snprintf(err_str, size, "(%d) Error during device operation", err); break;
		case LFS_ERR_CORRUPT:		snprintf(err_str, size, "(%d) Corrupted", err); break;
		case LFS_ERR_NOENT:			snprintf(err_str, size, "(%d) No directory entry", err); break;
		case LFS_ERR_EXIST:			snprintf(err_str, size, "(%d) Entry already exists", err); break;
		case LFS_ERR_NOTDIR:		snprintf(err_str, size, "(%d) Entry is not a dir", err); break;
		case LFS_ERR_ISDIR:			snprintf(err_str, size, "(%d) Entry is a dir", err); break;
		case LFS_ERR_NOTEMPTY:		snprintf(err_str, size, "(%d) Dir is not empty", err); break;
		case LFS_ERR_BADF:			snprintf(err_str, size, "(%d) Bad file number", err); break;
		case LFS_ERR_FBIG:			snprintf(err_str, size, "(%d) File too large", err); break;
		case LFS_ERR_INVAL:			snprintf(err_str, size, "(%d) Invalid parameter", err); break;
		case LFS_ERR_NOSPC:			snprintf(err_str, size, "(%d) No space left on device", err); break;
		case LFS_ERR_NOMEM:			snprintf(err_str, size, "(%d) No more memory available", err); break;
		case LFS_ERR_NOATTR:		snprintf(err_str, size, "(%d) No data/attr available", err); break;
		case LFS_ERR_NAMETOOLONG:	snprintf(err_str, size, "(%d) File name too long", err); break;
		default:					snprintf(err_str, size, "(%d) UNKOWN ERROR", err); break;
	}
}

static int _mbuspico_get_config_filenpath(mbuspico_config_t config, char* filenpath) {
	if (filenpath) {
		switch (config) {
			case MBUSPICO_CONFIG_ENCRYPTION_KEY: strncpy(filenpath, MBUSPICO_FS_CONFIG_PATH "/meter_key", LFS_NAME_MAX); break;
			case MBUSPICO_CONFIG_WIFI_PWD:		strncpy(filenpath, MBUSPICO_FS_CONFIG_PATH "/wifi_pwd", LFS_NAME_MAX); break;
			case MBUSPICO_CONFIG_WIFI_SSID:		strncpy(filenpath, MBUSPICO_FS_CONFIG_PATH "/wifi_ssid", LFS_NAME_MAX); break;
			case MBUSPICO_CONFIG_WIFI_HOSTNAME:	strncpy(filenpath, MBUSPICO_FS_CONFIG_PATH "/wifi_hostname", LFS_NAME_MAX); break;
			case MBUSPICO_CONFIG_HTTP_PORT:		strncpy(filenpath, MBUSPICO_FS_CONFIG_PATH "/http_port", LFS_NAME_MAX); break;
			case MBUSPICO_CONFIG_HTTP_AUTH_USER: strncpy(filenpath, MBUSPICO_FS_CONFIG_PATH "/http_auth_user", LFS_NAME_MAX); break;
			case MBUSPICO_CONFIG_HTTP_AUTH_PWD:	strncpy(filenpath, MBUSPICO_FS_CONFIG_PATH "/http_auth_pwd", LFS_NAME_MAX); break;
			case MBUSPICO_CONFIG_UDP_ENABLED:	strncpy(filenpath, MBUSPICO_FS_CONFIG_PATH "/udp_enabled", LFS_NAME_MAX); break;
			case MBUSPICO_CONFIG_UDP_RECEIVER:	strncpy(filenpath, MBUSPICO_FS_CONFIG_PATH "/udp_receiver", LFS_NAME_MAX); break;
			case MBUSPICO_CONFIG_UDP_PORT:		strncpy(filenpath, MBUSPICO_FS_CONFIG_PATH "/udp_port", LFS_NAME_MAX); break;
			case MBUSPICO_CONFIG_UDP_INTERVAL:	strncpy(filenpath, MBUSPICO_FS_CONFIG_PATH "/udp_interval", LFS_NAME_MAX); break;
			default:
				strncpy(filenpath, "", LFS_NAME_MAX);
				return 1;
		}
		return 0;
	}
	return 1;
}

static int _mbuspico_read_file(const char* filenpath, byte* data, uint32_t data_size) {
	lfs_file_t f;
	int err;
	err = lfs_file_open(&g_MBUSPICO_FS, &f, filenpath, LFS_O_RDONLY);
	if (err == LFS_ERR_OK) {
		lfs_soff_t file_size = lfs_file_size(&g_MBUSPICO_FS, &f);
		if (file_size < 0) {
			err = LFS_ERR_INVAL;
		}
		else {
			err = lfs_file_read(&g_MBUSPICO_FS, &f, data, lfs_min(data_size,file_size));
		}
		lfs_file_close(&g_MBUSPICO_FS, &f);
	}
	return err;
}

static int _mbuspico_write_file(const char* filenpath, byte* data, uint32_t data_size) {
	lfs_file_t f;
	int err;
	err = lfs_file_open(&g_MBUSPICO_FS, &f, filenpath, LFS_O_WRONLY | LFS_O_CREAT);
	if (err == LFS_ERR_OK) {
		err = lfs_file_rewind(&g_MBUSPICO_FS, &f);
		if (err == LFS_ERR_OK) {
			err = lfs_file_write(&g_MBUSPICO_FS, &f, data, data_size);
		}
		lfs_file_close(&g_MBUSPICO_FS, &f);
	}
	return err;
}


int mbuspico_read_config(mbuspico_config_t config, byte* data, uint32_t data_size) {
	char filenpath[LFS_NAME_MAX] = {0};
	_mbuspico_get_config_filenpath(config, filenpath);

	struct lfs_info info;
	int err = lfs_stat(&g_MBUSPICO_FS, filenpath, &info);
	if (err != LFS_ERR_OK) {
		// file doesnt exists yet, return default value
		switch (config) {
			case MBUSPICO_CONFIG_ENCRYPTION_KEY:
			#ifdef MBUSPICO_DEVICE_KEY
				strncpy(data, MBUSPICO_DEVICE_KEY, data_size);
			#else
				strncpy(data, "", data_size);
			#endif
				return 0;
			case MBUSPICO_CONFIG_WIFI_SSID:
			#ifdef MBUSPICO_WIFI_SSID
				strncpy(data, MBUSPICO_WIFI_SSID, data_size);
			#else
				strncpy(data, "", data_size);
			#endif
				return 0;
			case MBUSPICO_CONFIG_WIFI_PWD:
			#ifdef MBUSPICO_WIFI_PWD
				strncpy(data, MBUSPICO_WIFI_PWD, data_size);
			#else
				strncpy(data, "", data_size);
			#endif
				return 0;
			case MBUSPICO_CONFIG_WIFI_HOSTNAME:
			#ifdef CYW43_HOST_NAME
				strncpy(data, CYW43_HOST_NAME, data_size);
			#else
				strncpy(data, "", data_size);
			#endif
				return 0;
			case MBUSPICO_CONFIG_HTTP_PORT:
			#ifdef MBUSPICO_HTTP_SERVER_PORT
				*((int*)data) = MBUSPICO_HTTP_SERVER_PORT;
			#else
				*((int*)data) = 80;
			#endif
				return 0;
			case MBUSPICO_CONFIG_HTTP_AUTH_USER:
			#ifdef MBUSPICO_HTTP_AUTH_USER
				strncpy(data, MBUSPICO_HTTP_AUTH_USER, data_size);
			#else
				strncpy(data, "", data_size);
			#endif
				return 0;
			case MBUSPICO_CONFIG_HTTP_AUTH_PWD:
			#ifdef MBUSPICO_HTTP_AUTH_PWD
				strncpy(data, MBUSPICO_HTTP_AUTH_PWD, data_size);
			#else
				strncpy(data, "", data_size);
			#endif
				return 0;
			case MBUSPICO_CONFIG_UDP_ENABLED:
				*((int*)data) = 0;
				return 0;
			case MBUSPICO_CONFIG_UDP_RECEIVER:
			#ifdef MBUSPICO_UDP_RECEIVER_HOST
				strncpy(data, MBUSPICO_UDP_RECEIVER_HOST, data_size);
			#else
				strncpy(data, "", data_size);
			#endif
				return 0;
			case MBUSPICO_CONFIG_UDP_PORT:
			#ifdef MBUSPICO_UDP_RECEIVER_PORT
				*((int*)data) = MBUSPICO_UDP_RECEIVER_PORT;
			#else
				*((int*)data) = 0;
			#endif
				return 0;
			case MBUSPICO_CONFIG_UDP_INTERVAL:
			#ifdef MBUSPICO_UDP_INTERVAL_S
				*((int*)data) = MBUSPICO_UDP_INTERVAL_S;
			#else
				*((int*)data) = 10;
			#endif
				return 0;
			default:
				return 1;
		}
	}

	err = _mbuspico_read_file(filenpath, data, data_size);
	if (err != LFS_ERR_OK) {
		char err_str[32];
		_mbuspico_file_error_str(err, err_str, sizeof(err_str));
		MBUSPICO_LOG_E(LOG_TAG_FS, "Failed reading config file: [%s] %s", filenpath, err_str);
	}
	else {
		MBUSPICO_LOG_D(LOG_TAG_FS, "Successfully read config file: [%s]", filenpath);
	}
	return err;
}

int mbuspico_write_config(mbuspico_config_t config, byte* data, uint32_t data_size) {
	char filenpath[LFS_NAME_MAX] = {0};
	_mbuspico_get_config_filenpath(config, filenpath);

	// ensure config base dir exists
	struct lfs_info info;
	int err = lfs_stat(&g_MBUSPICO_FS, MBUSPICO_FS_CONFIG_PATH, &info);
	if (err < 0) {
		err = lfs_mkdir(&g_MBUSPICO_FS, MBUSPICO_FS_CONFIG_PATH);
		if (err < 0) {
		#if MBUSPICO_LOG_LEVEL >= LOG_ERROR
			printf("Failed to create config base dir at '" MBUSPICO_FS_CONFIG_PATH "'\n");
		#endif
		}
	}

	err = _mbuspico_write_file(filenpath, data, data_size);
	if (err != LFS_ERR_OK) {
		char err_str[32];
		_mbuspico_file_error_str(err, err_str, sizeof(err_str));
		MBUSPICO_LOG_E(LOG_TAG_FS, "Failed writing '%d bytes' to config file: [%s] %s", data_size, filenpath, err_str);
	}
	else {
		MBUSPICO_LOG_D(LOG_TAG_FS, "Successfully wrote config file: [%s]", filenpath);
	}
	return err;
}

// LOGGING

static void get_log_tag(uint16_t id, char* tag) {
	switch(id) {
		case LOG_TAG_MAIN:		sprintf(tag, "%s", "MAIN"); break;
		case LOG_TAG_WIFI:		sprintf(tag, "%s", "WIFI"); break;
		case LOG_TAG_DEVICE:	sprintf(tag, "%s", "DEVICE"); break;
		case LOG_TAG_UART:		sprintf(tag, "%s", "UART"); break;
		case LOG_TAG_HTTP:		sprintf(tag, "%s", "HTTP"); break;
		case LOG_TAG_UDP:		sprintf(tag, "%s", "UDP"); break;
		case LOG_TAG_MG:		sprintf(tag, "%s", "MG"); break;
		case LOG_TAG_FS:		sprintf(tag, "%s", "FS"); break;
		default:				sprintf(tag, "%s", "???"); break;
	}
}

#if MBUSPICO_LOG_LEVEL >= LOG_ERROR
void MBUSPICO_LOG_E(uint16_t id, const char* format, ...) {
	if (LOG_FILTER & id && xSemaphoreTake(g_LogMutex, pdMS_TO_TICKS(3000)) == pdTRUE) {
		char tag[10];
		get_log_tag(id,tag);
		va_list args;
		va_start (args, format);
		printf("[ERROR]\t%s\t", tag);
		vprintf(format, args);
		printf("\n");
		va_end (args);
		xSemaphoreGive(g_LogMutex);
	}
}
#endif

#if MBUSPICO_LOG_LEVEL >= LOG_INFO
void MBUSPICO_LOG_I(uint16_t id, const char* format, ...) {
	if (LOG_FILTER & id && xSemaphoreTake(g_LogMutex, pdMS_TO_TICKS(3000)) == pdTRUE) {
		char tag[10];
		get_log_tag(id,tag);
		va_list args;
		va_start (args, format);
		printf("[INFO]\t%s\t", tag);
		vprintf(format, args);
		printf("\n");
		va_end (args);
		xSemaphoreGive(g_LogMutex);
	}
}
#endif

#if MBUSPICO_LOG_LEVEL >= LOG_DEBUG
void MBUSPICO_LOG_D(uint16_t id, const char* format, ...) {
	if (LOG_FILTER & id && xSemaphoreTake(g_LogMutex, pdMS_TO_TICKS(3000)) == pdTRUE) {
		char tag[10];
		get_log_tag(id,tag);
		va_list args;
		va_start (args, format);
		printf("[DEBUG]\t%s\t", tag);
		vprintf(format, args);
		printf("\n");
		va_end (args);
		xSemaphoreGive(g_LogMutex);
	}
}
#endif
