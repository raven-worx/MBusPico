#
# mbedtls
# Just building mbedTLS crypto lib here. Using the offical CMakeLists results in incompatible binaries
#

SET(mbedtls_src_crypto
	aes.c
	aesni.c
	aria.c
	asn1parse.c
	asn1write.c
	base64.c
	bignum.c
	camellia.c
	ccm.c
	chacha20.c
	chachapoly.c
	cipher.c
	cipher_wrap.c
	constant_time.c
	cmac.c
	ctr_drbg.c
	des.c
	dhm.c
	ecdh.c
	ecdsa.c
	ecjpake.c
	ecp.c
	ecp_curves.c
	entropy.c
	entropy_poll.c
	error.c
	gcm.c
	hkdf.c
	hmac_drbg.c
	md.c
	md5.c
	memory_buffer_alloc.c
	mps_reader.c
	mps_trace.c
	nist_kw.c
	oid.c
	padlock.c
	pem.c
	pk.c
	pk_wrap.c
	pkcs12.c
	pkcs5.c
	pkparse.c
	pkwrite.c
	platform.c
	platform_util.c
	poly1305.c
	psa_crypto.c
	psa_crypto_aead.c
	psa_crypto_cipher.c
	psa_crypto_client.c
	psa_crypto_driver_wrappers.c
	psa_crypto_ecp.c
	psa_crypto_hash.c
	psa_crypto_mac.c
	psa_crypto_rsa.c
	psa_crypto_se.c
	psa_crypto_slot_management.c
	psa_crypto_storage.c
	psa_its_file.c
	ripemd160.c
	rsa.c
	rsa_alt_helpers.c
	sha1.c
	sha256.c
	sha512.c
	ssl_debug_helpers_generated.c
	threading.c
	timing.c
	version.c
	version_features.c
)
list(TRANSFORM mbedtls_src_crypto PREPEND ${CMAKE_CURRENT_LIST_DIR}/mbedtls/library/)

add_library(mbedtls_crypto STATIC)
target_sources(mbedtls_crypto PRIVATE ${mbedtls_src_crypto})

set_target_properties(mbedtls_crypto PROPERTIES COMPILE_FLAGS "-I${PROJECT_SOURCE_DIR}/include -DMBEDTLS_CONFIG_FILE=\"<mbedtls_config.h>\"")
target_include_directories(mbedtls_crypto
PUBLIC
	${CMAKE_CURRENT_LIST_DIR}/mbedtls/include
	${CMAKE_CURRENT_LIST_DIR}/mbedtls/library
)


#
# mongoose
#
add_library(mongoose STATIC)
target_sources(mongoose PRIVATE ${CMAKE_CURRENT_LIST_DIR}/mongoose/mongoose.c)

set_target_properties(mongoose PROPERTIES COMPILE_FLAGS "-DMG_ARCH=MG_ARCH_FREERTOS_LWIP -DMG_ENABLE_LWIP=1")

target_include_directories(mongoose 
PUBLIC
	${CMAKE_CURRENT_LIST_DIR}/mongoose
)

target_link_libraries(mongoose
PRIVATE
	pico_cyw43_arch_lwip_sys_freertos
	pico_stdlib
	FreeRTOS-Kernel-Heap4
)


#
# littlefs
#
add_library(littlefs STATIC)
target_sources(littlefs
PRIVATE
	${CMAKE_CURRENT_LIST_DIR}/littlefs/lfs.c
	${CMAKE_CURRENT_LIST_DIR}/littlefs/lfs_util.c
)

target_compile_options(littlefs
PUBLIC
	"-DLFS_THREADSAFE=1"
)

target_include_directories(littlefs 
PUBLIC
	${CMAKE_CURRENT_LIST_DIR}/littlefs
)
