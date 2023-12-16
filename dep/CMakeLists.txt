#
# WEB RESOURCE FILES
#

set(mbuspico_web_res_in
	bootstrap.min.css
	bootstrap.bundle.min.js
	config.html
	config.js
)

file(WRITE "web/include/web_res.h" "#pragma once\n\n")

foreach(res_file IN LISTS mbuspico_web_res_in)
	string(REPLACE "." "_" out_name ${res_file})
	set(out_file "${CMAKE_CURRENT_LIST_DIR}/web/raw/${out_name}.raw.c")
	add_custom_command(
		DEPENDS web/${res_file}
		OUTPUT ${out_file}
		WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/web/
		COMMAND "${CMAKE_CURRENT_LIST_DIR}/web/create_raw_res.sh" "${res_file}" "${out_name}"
		COMMENT "-- Creating RAW webresource file: ${res_file} ..."
		USES_TERMINAL
	)
	list(APPEND mbuspico_web_res_map "{\"/${res_file}\", __${out_name}, __${out_name}_size}")
	list(APPEND mbuspico_web_res_inc ${out_name})
	list(APPEND mbuspico_web_res_out ${out_file})
endforeach()

foreach(inc ${mbuspico_web_res_inc})
	file(APPEND "web/include/web_res.h" "#include \"${inc}.raw.h\"\n")
	#file(APPEND "web/include/web_res.h" "extern const unsigned char __${inc}[];\n")
	#file(APPEND "web/include/web_res.h" "extern const unsigned int __${inc}_size;\n")
endforeach()

file(APPEND "web/include/web_res.h" "\nstruct web_res { const char* url; const unsigned char* data; const unsigned int size; };\n")
file(APPEND "web/include/web_res.h" "const struct web_res g_web_resources[] = {\n")
foreach(map ${mbuspico_web_res_map})
	file(APPEND "web/include/web_res.h" "\t${map},\n")
endforeach()
file(APPEND "web/include/web_res.h" "};")

add_library(mbuspico_web_res STATIC)
target_sources(mbuspico_web_res PRIVATE ${mbuspico_web_res_out})
target_include_directories(mbuspico_web_res
PUBLIC
	${CMAKE_CURRENT_LIST_DIR}/web/include
)

#
# mbedtls
# Just building mbedTLS crypto lib here. Using the offical CMakeLists results in incompatible binaries
#

set(mbedtls_src_crypto
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

set_target_properties(mongoose PROPERTIES COMPILE_FLAGS "-DMG_ARCH=MG_ARCH_FREERTOS -DMG_ENABLE_LWIP=1")

target_include_directories(mongoose 
PUBLIC
	${CMAKE_CURRENT_LIST_DIR}/mongoose
PRIVATE
	${PROJECT_SOURCE_DIR}/include
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
