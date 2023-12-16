#include <mbuspico.h>
#include <stdio.h>
#include <semphr.h>
#include <string.h>
#include <pico/error.h>
#include <pico/bootrom.h>
#include <pico/flash.h>
#include <hardware/regs/rosc.h>
#include <hardware/regs/addressmap.h>
#include <hardware/sync.h>

#include <lfs.h>
#include <lfs_util.h>

static lfs_t g_MBUSPICO_FS;

static SemaphoreHandle_t g_FsMutex = NULL; // recursive mutex for filesystem access

/*
 * (defined in "hardware/flash.h")
 * PICO_FLASH_SIZE_BYTES	2097152 bytes (2MB)		- total size of the RP2040 flash
 * FLASH_SECTOR_SIZE		4096 bytes (4KB)		- size of one sector (minimum erase size)
 * FLASH_PAGE_SIZE			256 bytes				- size of one page (minimum write size)
*/

// filesystem size in Pico flash
#define MBUSPICO_FS_SIZE (32 * 1024) // 32 KB

#define MBUSPICO_FS_CONFIG_PATH "config"

// file system offset in flash
#define MBUSPICO_FS_BASE   (PICO_FLASH_SIZE_BYTES - MBUSPICO_FS_SIZE)

#define FLASH_OP_PROGRAM	(0)
#define FLASH_OP_ERASE		(1)

typedef struct {
	int op;
	uintptr_t p0;
	uintptr_t p1;
	union {
		lfs_size_t size;
	};
} _flash_operation_t;

static void _execute_flash_op(void *param) {
	const _flash_operation_t* fop = (const _flash_operation_t*)param;
	switch (fop->op) {
		case FLASH_OP_PROGRAM:
			flash_range_program(fop->p0, (const uint8_t *)fop->p1, fop->size);
			break;
		case FLASH_OP_ERASE:
			flash_range_erase(fop->p0, fop->size);
			break;
		default:
			break;
	}
}


static int _fs_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void* buffer, lfs_size_t size);
static int _fs_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void* buffer, lfs_size_t size);
static int _fs_erase(const struct lfs_config *c, lfs_block_t block);
static int _fs_lock(const struct lfs_config *c);
static int _fs_unlock(const struct lfs_config *c);
static int _fs_sync(const struct lfs_config *c);

static struct lfs_config g_MBUSPICO_FS_CFG = {
	// block device operation
	.read = _fs_read,
	.prog = _fs_prog,
	.erase = _fs_erase,
	.lock = _fs_lock,
	.unlock = _fs_unlock,
	.sync = _fs_sync,
	// block device configuration
	.read_size = FLASH_PAGE_SIZE, // 1
	.prog_size = FLASH_PAGE_SIZE,
	.block_size = FLASH_SECTOR_SIZE,
	.block_count = MBUSPICO_FS_SIZE / FLASH_SECTOR_SIZE,
	.cache_size = FLASH_PAGE_SIZE, //FLASH_SECTOR_SIZE / 4,
	.lookahead_size = FLASH_PAGE_SIZE, //32,
	.block_cycles = 100 //500
};

static int _fs_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void* buffer, lfs_size_t size) {
#if 1
	uintptr_t addr = XIP_BASE + MBUSPICO_FS_BASE + (block * c->block_size) + off;
	//MBUSPICO_LOG_D(LOG_TAG_FS, "READ: %p, %d", addr, size);
	memcpy(buffer, (unsigned char *)addr, size);
#else
	//assert(block < c->block_count);
	//assert(off + size <= c->block_size);
	// read flash via XIP mapped space
	memcpy(buffer, MBUSPICO_FS_BASE + XIP_NOCACHE_NOALLOC_BASE + (block * c->block_size) + off, size);
#endif
	return LFS_ERR_OK;
}

static int _fs_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void* buffer, lfs_size_t size) {
#if 1
	_flash_operation_t fop = {
		.op = FLASH_OP_PROGRAM,
		.p0 = MBUSPICO_FS_BASE + (block * c->block_size) + off,
		.p1 = (uintptr_t)buffer,
		.size = size
	};
	int res = flash_safe_execute(&_execute_flash_op, &fop, 5000);
	switch (res) {
		case PICO_OK: 							MBUSPICO_LOG_D(LOG_TAG_FS, "_fs_prog() OK: addr: %p, block: %d, size: %d", fop.p0, block, fop.size); return LFS_ERR_OK;
		case PICO_ERROR_TIMEOUT:				MBUSPICO_LOG_E(LOG_TAG_FS, "_fs_prog() ERROR_TIMEOUT: addr: %p, block: %d, size: %d", fop.p0, block, fop.size); return LFS_ERR_IO;
		case PICO_ERROR_NOT_PERMITTED:			MBUSPICO_LOG_E(LOG_TAG_FS, "_fs_prog() ERROR_NOT_PERMITTED: addr: %p, block: %d, size: %d", fop.p0, block, fop.size); return LFS_ERR_IO;
		case PICO_ERROR_INSUFFICIENT_RESOURCES:	MBUSPICO_LOG_E(LOG_TAG_FS, "_fs_prog() ERROR_INSUFFICIENT_RESOURCES: addr: %p, block: %d, size: %d", fop.p0, block, fop.size); return LFS_ERR_IO;
		default:								MBUSPICO_LOG_E(LOG_TAG_FS, "_fs_prog() ERROR_UNKNOWN (%d)): addr: %p, block: %d, size: %d", res, fop.p0, block, fop.size); return LFS_ERR_IO;
	}
#elif 0
	uint32_t ints = save_and_disable_interrupts();
	portDISABLE_INTERRUPTS();

    uint32_t addr = MBUSPICO_FS_BASE + (block * c->block_size) + off;
    printf("[FS] WRITE: %p, %d\n", addr, size); 
    //uint32_t ints = save_and_disable_interrupts();
    flash_range_program(addr, (const uint8_t *)buffer, size);
    //restore_interrupts(ints);
#else
	//assert(block < c->block_count);
	// program with Pico SDK
	uint32_t p = (uint32_t)MBUSPICO_FS_BASE + (block * c->block_size) + off;
	//uint32_t ints = save_and_disable_interrupts();
	flash_range_program(p, buffer, size);
	//restore_interrupts(ints);

	restore_interrupts (ints);
	portENABLE_INTERRUPTS();
#endif
	return LFS_ERR_OK;
}

static int _fs_erase(const struct lfs_config *c, lfs_block_t block) {
#if 1
	_flash_operation_t fop = {
		.op = FLASH_OP_ERASE,
		.p0 = MBUSPICO_FS_BASE + (block * c->block_size),
		.size = c->block_size
	};
	int res = flash_safe_execute(&_execute_flash_op, &fop, 5000);
	switch (res) {
		case PICO_OK: 							MBUSPICO_LOG_D(LOG_TAG_FS, "_fs_erase() OK: addr: %p, block: %d, size: %d", fop.p0, block, fop.size); return LFS_ERR_OK;
		case PICO_ERROR_TIMEOUT:				MBUSPICO_LOG_E(LOG_TAG_FS, "_fs_erase() ERROR_TIMEOUT: addr: %p, block: %d, size: %d", fop.p0, block, fop.size); return LFS_ERR_IO;
		case PICO_ERROR_NOT_PERMITTED:			MBUSPICO_LOG_E(LOG_TAG_FS, "_fs_erase() ERROR_NOT_PERMITTED: addr: %p, block: %d, size: %d", fop.p0, block, fop.size); return LFS_ERR_IO;
		case PICO_ERROR_INSUFFICIENT_RESOURCES:	MBUSPICO_LOG_E(LOG_TAG_FS, "_fs_erase() ERROR_INSUFFICIENT_RESOURCES: addr: %p, block: %d, size: %d", fop.p0, block, fop.size); return LFS_ERR_IO;
		default:								MBUSPICO_LOG_E(LOG_TAG_FS, "_fs_erase() ERROR_UNKNOWN (%d)): addr: %p, block: %d, size: %d", res, fop.p0, block, fop.size); return LFS_ERR_IO;
	}
#elif 0
	uint32_t ints = save_and_disable_interrupts();
	portDISABLE_INTERRUPTS();
    uint32_t addr = (uint32_t)MBUSPICO_FS_BASE + (block * c->block_size);
    printf("[FS] ERASE: %p, %d\n", offset, block);  
    //uint32_t ints = save_and_disable_interrupts();   
    flash_range_erase(addr, c->block_size);  
    //restore_interrupts(ints);
#else
	//assert(block < c->block_count);
	// erase with Pico SDK
	uint32_t p = (uint32_t)MBUSPICO_FS_BASE + (block * c->block_size);
	//uint32_t ints = save_and_disable_interrupts();
	flash_range_erase(p, c->block_size);
	//restore_interrupts(ints);

	restore_interrupts (ints);
	portENABLE_INTERRUPTS();
#endif
	return LFS_ERR_OK;
}

static int _fs_lock(const struct lfs_config *c) {
	return xSemaphoreTakeRecursive(g_FsMutex, 10) == pdTRUE ? LFS_ERR_OK : LFS_ERR_IO;
}

static int _fs_unlock(const struct lfs_config *c) {
	return xSemaphoreGiveRecursive(g_FsMutex) == pdTRUE ? LFS_ERR_OK : LFS_ERR_IO;
}

static int _fs_sync(const struct lfs_config *c) {
	return 0; // we dont use any buffers during writing, so simply return 0
}

void mbuspico_fs_init() {
	g_FsMutex = xSemaphoreCreateRecursiveMutex();
	xSemaphoreGive(g_FsMutex);
#if 0
	printf("PICO_FLASH_SIZE_BYTES: %d bytes\n", PICO_FLASH_SIZE_BYTES);
	printf("FLASH_SECTOR_SIZE: %d bytes\n", FLASH_SECTOR_SIZE);
	printf("FLASH_PAGE_SIZE: %d bytes\n", FLASH_PAGE_SIZE);
	printf("PICO_FLASH_SAFE_EXECUTE_SUPPORT_FREERTOS_SMP: "
#if PICO_FLASH_SAFE_EXECUTE_SUPPORT_FREERTOS_SMP
		"true"
#else
		"false"
#endif
	"\n");
#endif

    // Check we're not overlapping the binary in flash
    extern char __flash_binary_end;
	if ((uintptr_t)&__flash_binary_end - XIP_BASE > MBUSPICO_FS_BASE) {
		#if MBUSPICO_LOG_LEVEL >= LOG_ERROR
			printf("FS ERROR: fs overlapping binary flash region\n");
		#endif
		assert(false);
	}

	int fs_err = lfs_mount(&g_MBUSPICO_FS, &g_MBUSPICO_FS_CFG);
	// reformat if we can't mount the filesystem - this should only happen on the first boot
	if (fs_err) {
	#if MBUSPICO_LOG_LEVEL >= LOG_ERROR
		printf("Failed initialization of filesystem (%d bytes), formatting...\n", MBUSPICO_FS_SIZE);
	#endif
		lfs_format(&g_MBUSPICO_FS, &g_MBUSPICO_FS_CFG);
		fs_err = lfs_mount(&g_MBUSPICO_FS, &g_MBUSPICO_FS_CFG);
	}
	if (fs_err) {
	#if MBUSPICO_LOG_LEVEL >= LOG_ERROR
		printf("Failed initialization of filesystem (%d bytes)!\n", MBUSPICO_FS_SIZE);
	#endif
	}
	else {
		printf("Filesystem (%d bytes) initialized successfully!\n", MBUSPICO_FS_SIZE);
	}
}

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

static int _mbuspico_get_config_filepath(mbuspico_config_t config, char* filepath) {
	if (filepath) {
		switch (config) {
			case MBUSPICO_CONFIG_ENCRYPTION_KEY: strncpy(filepath, MBUSPICO_FS_CONFIG_PATH "/meter_key", LFS_NAME_MAX); break;
			case MBUSPICO_CONFIG_WIFI_PWD:		strncpy(filepath, MBUSPICO_FS_CONFIG_PATH "/wifi_pwd", LFS_NAME_MAX); break;
			case MBUSPICO_CONFIG_WIFI_SSID:		strncpy(filepath, MBUSPICO_FS_CONFIG_PATH "/wifi_ssid", LFS_NAME_MAX); break;
			case MBUSPICO_CONFIG_WIFI_HOSTNAME:	strncpy(filepath, MBUSPICO_FS_CONFIG_PATH "/wifi_hostname", LFS_NAME_MAX); break;
			case MBUSPICO_CONFIG_HTTP_PORT:		strncpy(filepath, MBUSPICO_FS_CONFIG_PATH "/http_port", LFS_NAME_MAX); break;
			case MBUSPICO_CONFIG_HTTP_AUTH_USER: strncpy(filepath, MBUSPICO_FS_CONFIG_PATH "/http_auth_user", LFS_NAME_MAX); break;
			case MBUSPICO_CONFIG_HTTP_AUTH_PWD:	strncpy(filepath, MBUSPICO_FS_CONFIG_PATH "/http_auth_pwd", LFS_NAME_MAX); break;
			case MBUSPICO_CONFIG_UDP_ENABLED:	strncpy(filepath, MBUSPICO_FS_CONFIG_PATH "/udp_enabled", LFS_NAME_MAX); break;
			case MBUSPICO_CONFIG_UDP_RECEIVER:	strncpy(filepath, MBUSPICO_FS_CONFIG_PATH "/udp_receiver", LFS_NAME_MAX); break;
			case MBUSPICO_CONFIG_UDP_PORT:		strncpy(filepath, MBUSPICO_FS_CONFIG_PATH "/udp_port", LFS_NAME_MAX); break;
			case MBUSPICO_CONFIG_UDP_INTERVAL:	strncpy(filepath, MBUSPICO_FS_CONFIG_PATH "/udp_interval", LFS_NAME_MAX); break;
			default:
				strncpy(filepath, "", LFS_NAME_MAX);
				return 1;
		}
		return 0;
	}
	return 1;
}

static int _mbuspico_read_file(const char* filepath, byte* data, uint32_t data_size) {
	lfs_file_t f;
	int err;
	err = lfs_file_open(&g_MBUSPICO_FS, &f, filepath, LFS_O_RDONLY);
	if (err == LFS_ERR_OK) {
		lfs_soff_t file_size = lfs_file_size(&g_MBUSPICO_FS, &f);
		if (file_size < 0) {
			err = LFS_ERR_INVAL;
		}
		else {
			uint32_t bytes_read = lfs_file_read(&g_MBUSPICO_FS, &f, data, lfs_min(data_size,file_size));
			if (bytes_read < 0) {
				err = bytes_read;
			}
		}
		lfs_file_close(&g_MBUSPICO_FS, &f);
	}
	return err;
}

static int _mbuspico_write_file(const char* filepath, const byte* data, uint32_t data_size) {
	lfs_file_t f;
	int err;
	err = lfs_file_open(&g_MBUSPICO_FS, &f, filepath, LFS_O_WRONLY | LFS_O_CREAT);
	if (err == LFS_ERR_OK) {
		err = lfs_file_rewind(&g_MBUSPICO_FS, &f);
		if (err == LFS_ERR_OK) {
			uint32_t bytes_written = lfs_file_write(&g_MBUSPICO_FS, &f, data, data_size);
			if (bytes_written < 0) {
				err = bytes_written;
			}
		}
		lfs_file_close(&g_MBUSPICO_FS, &f);
	}
	return err;
}


int mbuspico_read_config(mbuspico_config_t config, byte* data, uint32_t data_size) {
	char filepath[LFS_NAME_MAX] = {0};
	_mbuspico_get_config_filepath(config, filepath);

	struct lfs_info info;
	int err = lfs_stat(&g_MBUSPICO_FS, filepath, &info);
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

	err = _mbuspico_read_file(filepath, data, data_size);
	if (err != LFS_ERR_OK) {
		char err_str[32];
		_mbuspico_file_error_str(err, err_str, sizeof(err_str));
		MBUSPICO_LOG_E(LOG_TAG_FS, "Failed reading config file: [%s] %s", filepath, err_str);
	}
	else {
		MBUSPICO_LOG_D(LOG_TAG_FS, "Successfully read config file: [%s]", filepath);
	}
	return err;
}

int mbuspico_read_config_num(mbuspico_config_t config, int* data) {
	return mbuspico_read_config(config, (byte*)data, sizeof(int));
}

int mbuspico_write_config(mbuspico_config_t config, const byte* data, uint32_t data_size) {
	char filepath[LFS_NAME_MAX] = {0};
	_mbuspico_get_config_filepath(config, filepath);
	MBUSPICO_LOG_D(LOG_TAG_FS, "Writing config: (%d bytes) '%s'", data_size, filepath);
	// ensure config base dir exists
	struct lfs_info info;
	int err = lfs_stat(&g_MBUSPICO_FS, MBUSPICO_FS_CONFIG_PATH, &info);
	if (err < 0) {
		char err_str[32];
		_mbuspico_file_error_str(err, err_str, sizeof(err_str));
		MBUSPICO_LOG_D(LOG_TAG_FS, "Config base dir '" MBUSPICO_FS_CONFIG_PATH "' does not exist, creating it. -> %s", err_str);
		err = lfs_mkdir(&g_MBUSPICO_FS, MBUSPICO_FS_CONFIG_PATH);
		if (err < 0) {
			_mbuspico_file_error_str(err, err_str, sizeof(err_str));
			MBUSPICO_LOG_E(LOG_TAG_FS, "Failed to create config base dir at '" MBUSPICO_FS_CONFIG_PATH "': %s", err_str);
		}
	}

	err = _mbuspico_write_file(filepath, data, data_size);
	if (err != LFS_ERR_OK) {
		char err_str[32];
		_mbuspico_file_error_str(err, err_str, sizeof(err_str));
		MBUSPICO_LOG_E(LOG_TAG_FS, "Failed writing '%d bytes' to config file: [%s] %s", data_size, filepath, err_str);
	}
	else {
		MBUSPICO_LOG_D(LOG_TAG_FS, "Successfully wrote config file: [%s]", filepath);
	}
	return err;
}

int mbuspico_write_config_num(mbuspico_config_t config, const int* val) {
	return mbuspico_write_config(config, (const byte*)val, sizeof(int));
}

void mbuspico_fs_task(void* arg) {
	MBUSPICO_LOG_D(LOG_TAG_HTTP, "mbuspico_fs_task()");
	
	mbuspico_fs_init();

	for(;;) {
		
		
		vTaskDelay(pdMS_TO_TICKS(100));
	}
}
