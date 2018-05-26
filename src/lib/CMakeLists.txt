# Copyright (c) 2018 Ribose Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

include(GNUInstallDirs)

# these could probably be optional but are currently not
find_package(BZip2 REQUIRED)
find_package(ZLIB REQUIRED)

# required packages
find_package(JSON-C 0.11 REQUIRED)
find_package(Botan2 2.5.0 REQUIRED)

# generate a config.h
include(CheckIncludeFile)
check_include_file(fcntl.h HAVE_FCNTL_H)
check_include_file(inttypes.h HAVE_INTTYPES_H)
check_include_file(limits.h HAVE_LIMITS_H)
check_include_file(stdint.h HAVE_STDINT_H)
check_include_file(string.h HAVE_STRING_H)
check_include_file(sys/cdefs.h HAVE_SYS_CDEFS_H)
check_include_file(sys/cdefs.h HAVE_SYS_MMAN_H)
check_include_file(sys/resource.h HAVE_SYS_RESOURCE_H)
check_include_file(sys/stat.h HAVE_SYS_STAT_H)
check_include_file(sys/types.h HAVE_SYS_TYPES_H)
check_include_file(unistd.h HAVE_UNISTD_H)
set(HAVE_ZLIB_H "${ZLIB_FOUND}")
set(HAVE_BZLIB_H "${BZIP2_FOUND}")
configure_file(config.h.in config.h)

# check botan's enabled features
set(CMAKE_REQUIRED_INCLUDES "${BOTAN2_INCLUDE_DIRS}")
set(_botan_required_features
    # base
    BIGINT FFI HEX_CODEC PGP_S2K
    # AEAD
    BLOCK_CIPHER AEAD_EAX AEAD_OCB
    # symmetric ciphers
    AES BLOWFISH CAMELLIA CAST_128 DES IDEA RIPEMD_160 SM4 TWOFISH
    # cipher modes
    MODE_CBC MODE_CFB
    # RNG
    AUTO_RNG AUTO_SEEDING_RNG HMAC HMAC_DRBG
    # hash
    CRC24 HASH MD5 SHA1 SHA2_32 SHA2_64 SHA3 SM3
    # public-key core
    DL_GROUP DL_PUBLIC_KEY_FAMILY ECC_GROUP ECC_PUBLIC_KEY_CRYPTO PUBLIC_KEY_CRYPTO
    # public-key algs
    DSA ECDH ECDSA ED25519 ELGAMAL RSA SM2
    # public-key operations etc
    EME_PKCS1v15 EMSA_PKCS1 EMSA_RAW KDF_BASE RFC3394_KEYWRAP SP800_56A
)
include(CheckCXXSymbolExists)
foreach(feature ${_botan_required_features})
  check_cxx_symbol_exists("BOTAN_HAS_${feature}" botan/build.h _botan_has_${feature})
  if (NOT _botan_has_${feature})
    message(FATAL_ERROR "A required botan feature is missing: ${feature}")
  endif()
endforeach()
set(CMAKE_REQUIRED_INCLUDES)

add_library(librnp
  # librepgp
  ../librepgp/packet-parse.c
  ../librepgp/packet-print.c
  ../librepgp/packet-show.c
  ../librepgp/reader.c
  ../librepgp/repgp.c
  ../librepgp/stream-armor.c
  ../librepgp/stream-common.c
  ../librepgp/stream-dump.c
  ../librepgp/stream-key.c
  ../librepgp/stream-packet.c
  ../librepgp/stream-parse.c
  ../librepgp/stream-sig.c
  ../librepgp/stream-write.c
  ../librepgp/validate.c

  # librekey
  ../librekey/key_store_g10.c
  ../librekey/key_store_kbx.c
  ../librekey/key_store_pgp.c
  ../librekey/key_store_ssh.c
  ../librekey/rnp_key_store.c

  bufgap.c
  crypto/bn.c
  crypto/dsa.c
  crypto/ec.c
  crypto/ecdh.c
  crypto/ecdsa.c
  crypto/eddsa.c
  crypto/elgamal.c
  crypto/hash.c
  crypto/mpi.c
  crypto/rng.c
  crypto/rsa.c
  crypto/s2k.c
  crypto/sm2.c
  crypto/symmetric.c
  crypto.c
  fingerprint.c
  generate-key.c
  key-provider.c
  list.c
  misc.c
  packet-create.c
  pass-provider.c
  pem.c
  pgp-key.c
  rnp2.c
  rnp.c
  signature.c
  writer.c
)

set_target_properties(librnp
  PROPERTIES
    VERSION "${VERSION}"
    SOVERSION 0
    PREFIX ""
)

target_include_directories(librnp
  PUBLIC
    "$<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/src/lib>"
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:include>"
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}/src"
)

# This has precedence and can cause some confusion when the binary
# dir one isn't actually being used. To be improved.
file(REMOVE "${CMAKE_CURRENT_SOURCE_DIR}/config.h")

target_link_libraries(librnp
  PRIVATE
    Botan2::Botan2
    JSON-C::JSON-C
)

if (TARGET BZip2::BZip2)
  target_link_libraries(librnp PRIVATE BZip2::BZip2)
endif()

if (TARGET ZLIB::ZLIB)
  target_link_libraries(librnp PRIVATE ZLIB::ZLIB)
endif()

set(LIBRNP_INCLUDEDIR "rnp-${PROJECT_VERSION_MAJOR}")

# add these to the rnp-targets export
install(TARGETS librnp
  EXPORT rnp-targets
  LIBRARY DESTINATION  "${CMAKE_INSTALL_LIBDIR}"
  ARCHIVE DESTINATION  "${CMAKE_INSTALL_LIBDIR}"
  INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${LIBRNP_INCLUDEDIR}"
)

# install headers
install(
  FILES
    "${PROJECT_SOURCE_DIR}/include/rnp/rnp2.h"
  DESTINATION
    "${CMAKE_INSTALL_INCLUDEDIR}/${LIBRNP_INCLUDEDIR}"
  RENAME
    rnp.h
)

# .cmake installs
set(INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/rnp")

install(EXPORT rnp-targets
  FILE rnp-targets.cmake
  NAMESPACE rnp::
  DESTINATION "${INSTALL_CMAKEDIR}"
)

include(CMakePackageConfigHelpers)
configure_package_config_file(
  "${PROJECT_SOURCE_DIR}/cmake/rnp-config.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/rnp-config.cmake"
  INSTALL_DESTINATION "${INSTALL_CMAKEDIR}"
)
write_basic_package_version_file(rnp-config-version.cmake
  VERSION "${PROJECT_VERSION}"
  COMPATIBILITY SameMajorVersion
)
install (
  FILES
    "${CMAKE_CURRENT_BINARY_DIR}/rnp-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/rnp-config-version.cmake"
  DESTINATION "${INSTALL_CMAKEDIR}"
)

# create a pkgconfig .pc too
find_package(PkgConfig)
if (PKG_CONFIG_FOUND)
  configure_file(
    "${PROJECT_SOURCE_DIR}/cmake/librnp.pc.in"
    "${PROJECT_BINARY_DIR}/librnp-${PROJECT_VERSION_MAJOR}.pc"
    @ONLY
  )
  install(
    FILES "${PROJECT_BINARY_DIR}/librnp-${PROJECT_VERSION_MAJOR}.pc"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
  )
endif()
