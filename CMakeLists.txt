cmake_minimum_required(VERSION 3.15.0 FATAL_ERROR)

if(NOT DEFINED OPENSC_VERSION)
    set(OPENSC_VERSION "0.0.0")
endif()
if (NOT DEFINED OPENSC_SCM_REVISION)
    set(OPENSC_SCM_REVISION "0")
endif()

project("OpenSC" LANGUAGES "C" VERSION ${OPENSC_VERSION})
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules)

option(USE_ADDRESS_SANITIZER "Use fast memory error detector." OFF)
if (USE_ADDRESS_SANITIZER)
    # Enable AddressSanitizer, a fast memory error detector. Memory access instructions are instrumented to detect out-of-bounds and use-after-free bugs.
    if (MSVC)
        # MSVC's incremental linking is not compatible with fsanitize
        add_link_options(
            $<$<CONFIG:DEBUG>:/INCREMENTAL:NO>
        )
        add_compile_options(
            $<$<CONFIG:DEBUG>:/fsanitize=address>
            $<$<CONFIG:DEBUG>:/Zi>
        )
        message(STATUS "MSVC Address sanitizer enabled.
        This is still an experimantal feature, therefore the clang_rt.asan_*.dll DLLs are not installed in system32.
        You have to add the VC++ redistributable package to your PATH.
        E.g.: C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Community\\VC\\Tools\\MSVC\\14.28.29910\\bin\\Hostx64\\x64
        Otherwise you will get an errors like
            clang_rt.asan_dbg_dynamic-x86_64.dll: cannot open shared object file: No such file or directory
        when you try to run the test executables.
        ")
    else()
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address")
        set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fsanitize=address")
        set(CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fsanitize=address")
    endif()
endif()


include(GNUInstallDirs)
configure_file("config.h.in" "${CMAKE_BINARY_DIR}/include/config.h" @ONLY)
include_directories(common PUBLIC src ${CMAKE_BINARY_DIR}/include/)

include(CheckCSourceCompiles)
check_c_source_compiles("
    int main() {
        int result;
        return __builtin_add_overflow(1, 2, &result);
    }
" HAVE_BUILTIN_OVERFLOW)

add_library(common STATIC
    src/common/compat_dummy.c
    src/common/compat_getopt.c
    src/common/compat_getopt.h
    src/common/compat_getopt_main.c
    src/common/compat_getpass.c
    src/common/compat_getpass.h
    src/common/compat___iob_func.c
    src/common/compat_overflow.c
    src/common/compat_overflow.h
    src/common/compat_report_rangecheckfailure.c
    src/common/compat_strlcat.c
    src/common/compat_strlcat.h
    src/common/compat_strlcpy.c
    src/common/compat_strlcpy.h
    src/common/compat_strnlen.c
    src/common/compat_strnlen.h
    src/common/constant-time.h
    src/common/libpkcs11.c
    src/common/libpkcs11.h
    src/common/libscdl.c
    src/common/libscdl.h
    src/common/simclist.c
    src/common/simclist.h
)

if(HAVE_BUILTIN_OVERFLOW)
    target_compile_definitions(common PRIVATE HAVE_BUILTIN_OVERFLOW)
endif()

add_library(pkcs11 STATIC
    src/common/libpkcs11.c
    src/common/libpkcs11.h
)

add_library(scdl STATIC
    src/common/libscdl.c
    src/common/libscdl.h
)

add_library(notify STATIC
    src/ui/notify.h
    src/ui/notify.c
)

add_library(opensc
    src/libopensc/apdu.c
    src/libopensc/asn1.c
    src/libopensc/asn1.h
    src/libopensc/aux-data.c
    src/libopensc/aux-data.h
    src/libopensc/base64.c
    src/libopensc/card.c
    src/libopensc/cardctl.h
    src/libopensc/card-default.c
    src/libopensc/card-piv.c
    src/libopensc/cards.h
    src/libopensc/ccid-types.h
    src/libopensc/compression.c
    src/libopensc/compression.h
    src/libopensc/ctbcs.c
    src/libopensc/ctbcs.h
    src/libopensc/ctx.c
    src/libopensc/cwa14890.c
    src/libopensc/cwa14890.h
    src/libopensc/cwa-dnie.c
    src/libopensc/cwa-dnie.h
    src/libopensc/dir.c
    src/libopensc/ef-atr.c
    src/libopensc/ef-gdo.c
    src/libopensc/errors.c
    src/libopensc/errors.h
    src/libopensc/gp.c
    src/libopensc/gp.h
    src/libopensc/iasecc.h
    src/libopensc/iasecc-sdo.c
    src/libopensc/iasecc-sdo.h
    src/libopensc/iasecc-sm.c
    src/libopensc/internal.h
    src/libopensc/internal-winscard.h
    src/libopensc/iso7816.c
    src/libopensc/iso7816.h
    src/libopensc/itacns.h
    src/libopensc/jpki.h
    src/libopensc/log.c
    src/libopensc/log.h
    src/libopensc/muscle.c
    src/libopensc/muscle-filesystem.c
    src/libopensc/muscle-filesystem.h
    src/libopensc/muscle.h
    src/libopensc/opensc.h
    src/libopensc/pace.h
    src/libopensc/padding.c
    src/libopensc/pkcs15-algo.c
    src/libopensc/pkcs15.c
    src/libopensc/pkcs15-cache.c
    src/libopensc/pkcs15-cert.c
    src/libopensc/pkcs15-data.c
    src/libopensc/pkcs15-emulator-filter.c
    src/libopensc/pkcs15-emulator-filter.h
    src/libopensc/pkcs15.h
    src/libopensc/pkcs15-pin.c
    src/libopensc/pkcs15-piv.c
    src/libopensc/pkcs15-prkey.c
    src/libopensc/pkcs15-pubkey.c
    src/libopensc/pkcs15-skey.c
    src/libopensc/pkcs15-syn.c
    src/libopensc/pkcs15-syn.h
    src/libopensc/reader-ctapi.c
    src/libopensc/reader-openct.c
    src/libopensc/reader-pcsc.c
    src/libopensc/reader-tr03119.c
    src/libopensc/reader-tr03119.h
    src/libopensc/sc.c
    src/libopensc/sc-ossl-compat.h
    src/libopensc/sec.c
    src/libopensc/simpletlv.c
    src/libopensc/simpletlv.h
    src/libopensc/sm.c
    src/libopensc/sm.h
    src/libopensc/types.h
)

add_library(scconf STATIC
    src/scconf/scconf.c
    src/scconf/scconf.h
    src/scconf/parse.c
    src/scconf/sclex.c
    src/scconf/write.c
)

add_library(util STATIC
    src/tools/util.c
)

find_package(OpenSSL REQUIRED)

target_link_libraries(opensc PRIVATE common notify PUBLIC OpenSSL::Crypto)
target_compile_definitions(opensc PRIVATE ONLY_PIV HAVE_WINSCARD_H PUBLIC HAVE_CONFIG_H)

if(WIN32)
    target_link_libraries(opensc PUBLIC Winscard ws2_32 shlwapi)
    target_compile_definitions(opensc PRIVATE UNICODE _UNICODE DEFAULT_PCSC_PROVIDER="winscard.dll")
else()
    find_package(PCSC REQUIRED)
    target_link_libraries(opensc PUBLIC ${PCSC_LIBRARIES})
    target_include_directories(opensc PUBLIC ${PCSC_INCLUDE_DIRS})
    target_compile_definitions(opensc PRIVATE HAVE_PCSCLITE_H HAVE_INTTYPES_H DEFAULT_PCSC_PROVIDER="$<IF:$<BOOL:${APPLE}>,/System/Library/Frameworks/PCSC.framework/PCSC,libpcsclite.so.1>")
endif()

install(TARGETS opensc
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
)

install(DIRECTORY src/
        DESTINATION include
        FILES_MATCHING
        PATTERN "tests" EXCLUDE
        PATTERN "ui" EXCLUDE
        PATTERN "minidriver" EXCLUDE
        PATTERN "common" EXCLUDE
        PATTERN "scconf" EXCLUDE
        PATTERN "sm" EXCLUDE
        PATTERN "smm" EXCLUDE
        PATTERN "tools" EXCLUDE
        PATTERN "*.h")

add_executable(opensc-tool
    src/tools/opensc-tool.c
)

target_link_libraries(opensc-tool PRIVATE opensc scconf util)

if(WIN32)
add_library(opensc-minidriver SHARED
    src/minidriver/minidriver.c
    src/minidriver/versioninfo-minidriver.rc
)
endif()