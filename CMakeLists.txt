cmake_minimum_required(VERSION 3.15.0 FATAL_ERROR)

if(NOT DEFINED OPENSC_VERSION)
    set(OPENSC_VERSION "0.0.0")
endif()
if (NOT DEFINED OPENSC_SCM_REVISION)
    set(OPENSC_SCM_REVISION "0")
endif()

project("OpenSC" LANGUAGES "C" VERSION ${OPENSC_VERSION})

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

find_package(OpenSSL REQUIRED)

# Create empty config.h -> all definitions that are usually in config.h will be added with add_compile_definitions.
file(TOUCH "${CMAKE_BINARY_DIR}/include/config.h")
include_directories(common PUBLIC src ${CMAKE_BINARY_DIR}/include/)

include(GNUInstallDirs)
add_compile_definitions(
    PACKAGE_NAME="OpenSC"
    PACKAGE_VERSION="${CMAKE_PROJECT_VERSION}-swissbit-piv"
    OPENSC_SCM_REVISION="${OPENSC_SCM_REVISION}"
    OPENSC_FEATURES="openssl"
    OPENSC_CONF_PATH="${CMAKE_INSTALL_FULL_SYSCONFDIR}/opensc.conf"
    ENABLE_OPENSSL
    ENABLE_SM
    DEFAULT_SM_MODULE="$<$<NOT:$<BOOL:${WIN32}>>:lib>smm-local$<IF:$<BOOL:${WIN32}>,.dll,.so>"
    DEFAULT_SM_MODULE_PATH="$<IF:$<BOOL:${WIN32}>,%PROGRAMFILES%\\Swissbit AG\\OpenSC\\tools,${CMAKE_INSTALL_FULL_LIBDIR}>"
    ENABLE_PIV_SM
)

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

add_library(opensc
    src/libopensc/apdu.c
    src/libopensc/asn1.c
    src/libopensc/asn1.h
    #src/libopensc/authentic.h
    #src/libopensc/aux-data.c
    #src/libopensc/aux-data.h
    src/libopensc/base64.c
    #src/libopensc/card-asepcos.c
    #src/libopensc/card-atrust-acos.c
    #src/libopensc/card-authentic.c
    #src/libopensc/card-belpic.c
    src/libopensc/card.c
    #src/libopensc/card-cac1.c
    #src/libopensc/card-cac.c
    #src/libopensc/card-cac-common.c
    #src/libopensc/card-cac-common.h
    #src/libopensc/card-cardos.c
    #src/libopensc/card-cardos-common.c
    #src/libopensc/card-cardos-common.h
    #src/libopensc/card-coolkey.c
    src/libopensc/cardctl.h
    src/libopensc/card-default.c
    #src/libopensc/card-dnie.c
    #src/libopensc/card-dtrust.c
    #src/libopensc/card-edo.c
    #src/libopensc/card-entersafe.c
    #src/libopensc/card-eoi.c
    #src/libopensc/card-eoi.h
    #src/libopensc/card-epass2003.c
    #src/libopensc/card-esteid2018.c
    #src/libopensc/card-esteid2025.c
    #src/libopensc/card-flex.c
    #src/libopensc/card-gemsafeV1.c
    #src/libopensc/card-gids.c
    #src/libopensc/card-gids.h
    #src/libopensc/card-iasecc.c
    #src/libopensc/card-idprime.c
    #src/libopensc/card-isoApplet.c
    #src/libopensc/card-itacns.c
    #src/libopensc/card-jpki.c
    #src/libopensc/card-masktech.c
    #src/libopensc/card-mcrd.c
    #src/libopensc/card-muscle.c
    #src/libopensc/card-myeid.c
    #src/libopensc/card-npa.c
    #src/libopensc/card-npa.h
    #src/libopensc/card-nqApplet.c
    #src/libopensc/card-oberthur.c
    #src/libopensc/card-openpgp.c
    #src/libopensc/card-openpgp.h
    src/libopensc/card-piv.c
    #src/libopensc/card-rtecp.c
    #src/libopensc/card-rutoken.c
    #src/libopensc/card-sc-hsm.c
    #src/libopensc/card-sc-hsm.h
    #src/libopensc/card-setcos.c
    src/libopensc/cards.h
    #src/libopensc/card-skeid.c
    #src/libopensc/card-starcos.c
    #src/libopensc/card-tcos.c
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
    #src/libopensc/pkcs15-actalis.c
    #src/libopensc/pkcs15-algo.c
    #src/libopensc/pkcs15-atrust-acos.c
    src/libopensc/pkcs15.c
    #src/libopensc/pkcs15-cac.c
    #src/libopensc/pkcs15-cache.c
    #src/libopensc/pkcs15-cardos.c
    #src/libopensc/pkcs15-cert.c
    #src/libopensc/pkcs15-coolkey.c
    src/libopensc/pkcs15-data.c
    #src/libopensc/pkcs15-din-66291.c
    #src/libopensc/pkcs15-dnie.c
    #src/libopensc/pkcs15-dtrust.c
    src/libopensc/pkcs15-emulator-filter.c
    src/libopensc/pkcs15-emulator-filter.h
    #src/libopensc/pkcs15-eoi.c
    #src/libopensc/pkcs15-esinit.c
    #src/libopensc/pkcs15-esteid2018.c
    #src/libopensc/pkcs15-esteid2025.c
    #src/libopensc/pkcs15-gemsafeV1.c
    #src/libopensc/pkcs15-gids.c
    src/libopensc/pkcs15.h
    #src/libopensc/pkcs15-iasecc.c
    #src/libopensc/pkcs15-idprime.c
    #src/libopensc/pkcs15-itacns.c
    #src/libopensc/pkcs15-jpki.c
    #src/libopensc/pkcs15-nqApplet.c
    #src/libopensc/pkcs15-oberthur.c
    #src/libopensc/pkcs15-openpgp.c
    #src/libopensc/pkcs15-pin.c
    src/libopensc/pkcs15-piv.c
    #src/libopensc/pkcs15-prkey.c
    #src/libopensc/pkcs15-pteid.c
    src/libopensc/pkcs15-pubkey.c
    #src/libopensc/pkcs15-sc-hsm.c
    #src/libopensc/pkcs15-sec.c
    #src/libopensc/pkcs15-skeid.c
    #src/libopensc/pkcs15-skey.c
    #src/libopensc/pkcs15-starcert.c
    #src/libopensc/pkcs15-starcos-esign.c
    src/libopensc/pkcs15-syn.c
    src/libopensc/pkcs15-syn.h
    #src/libopensc/pkcs15-tccardos.c
    #src/libopensc/pkcs15-tcos.c
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

target_link_libraries(opensc PRIVATE common PUBLIC OpenSSL::Crypto)

add_executable(opensc-tool
    src/tools/opensc-tool.c
)

target_link_libraries(opensc-tool PRIVATE opensc scconf util)