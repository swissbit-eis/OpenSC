cmake_minimum_required(VERSION 3.15.0 FATAL_ERROR)

if(NOT DEFINED OPENSC_VERSION)
    set(OPENSC_VERSION "0.0.0")
endif()
if (NOT DEFINED OPENSC_SCM_REVISION)
    set(OPENSC_SCM_REVISION "0")
endif()

project("OpenSC" LANGUAGES "C" "CXX" VERSION ${OPENSC_VERSION})
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules)

include(CheckIncludeFile)
include(CheckFunctionExists)
include(GNUInstallDirs)

check_include_file("unistd.h"  HAVE_UNISTD_H)
check_include_file("pthread.h" HAVE_PTHREAD_H)
check_include_file("dlfcn.h"   HAVE_DLFCN_H)

find_package(Threads REQUIRED)
set(HAVE_PTHREAD 0)
if(Threads_FOUND AND HAVE_PTHREAD_H)
  set(HAVE_PTHREAD 1)
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(SC_PKCS15_PROFILE_DIRECTORY "${CMAKE_INSTALL_FULL_DATADIR}/opensc")

find_package(OpenSSL REQUIRED)

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
        This is still an experimental feature, therefore the clang_rt.asan_*.dll DLLs are not installed in system32.
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
set(OPENSC_VERSION_MAJOR ${CMAKE_PROJECT_VERSION_MAJOR})
set(OPENSC_VERSION_MINOR ${CMAKE_PROJECT_VERSION_MINOR})
set(OPENSC_VERSION_FIX ${CMAKE_PROJECT_VERSION_PATCH})
set(OPENSC_VERSION_REVISION 0)
set(OPENSC_VS_FF_PRODUCT_NAME "OpenSC for Swissbit iShield Key")
set(OPENSC_VS_FF_COMPANY_NAME "Swissbit AG")
set(OPENSC_VS_FF_COMMENTS "Provided under the terms of the GNU Lesser General Public License (LGPLv2.1+).")
set(OPENSC_VS_FF_LEGAL_COPYRIGHT "OpenSC Project")
set(OPENSC_VS_FF_COMPANY_URL "https://www.swissbit.com")
set(OPENSC_VS_FF_PRODUCT_UPDATES "https://www.swissbit.com/ishield-key")
set(PACKAGE_BUGREPORT "support@swissbit.com")
set(PACKAGE_URL "https://www.swissbit.com/ishield-key")
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
set_target_properties(common PROPERTIES LINKER_LANGUAGE C)
target_compile_definitions(common PUBLIC HAVE_CONFIG_H)
if(HAVE_BUILTIN_OVERFLOW)
    target_compile_definitions(common PRIVATE HAVE_BUILTIN_OVERFLOW)
endif()

add_library(libpkcs11 STATIC
    src/common/libpkcs11.c
    src/common/libpkcs11.h
)
set_target_properties(libpkcs11 PROPERTIES LINKER_LANGUAGE C)

add_library(libscdl STATIC
    src/common/libscdl.c
    src/common/libscdl.h
)
set_target_properties(libscdl PROPERTIES LINKER_LANGUAGE C)

add_library(notify STATIC
    src/ui/notify.h
    src/ui/notify.c
    src/ui/invisible_window.h
)
set_target_properties(notify PROPERTIES LINKER_LANGUAGE C)
target_compile_definitions(notify PUBLIC HAVE_CONFIG_H)

add_library(strings STATIC
    src/ui/strings.h
    src/ui/strings.c
    src/ui/wchar_from_char_str.h
    src/ui/char_str_from_wchar.h
)
set_target_properties(strings PROPERTIES LINKER_LANGUAGE C)

add_library(scconf STATIC
    src/scconf/scconf.c
    src/scconf/scconf.h
    src/scconf/parse.c
    src/scconf/sclex.c
    src/scconf/write.c
    src/scconf/internal.h
)
set_target_properties(scconf PROPERTIES LINKER_LANGUAGE C)

add_library(pkcs15init STATIC
    src/pkcs15init/pkcs15-init.h
    src/pkcs15init/pkcs15-lib.c
    src/pkcs15init/profile.c
    src/pkcs15init/profile.h
)
target_compile_definitions(pkcs15init PRIVATE HAVE_CONFIG_H)
target_include_directories(pkcs15init BEFORE PRIVATE "${CMAKE_BINARY_DIR}/include")
set_target_properties(pkcs15init PROPERTIES LINKER_LANGUAGE C)
target_link_libraries(pkcs15init PRIVATE OpenSSL::Crypto)

add_library(libsmiso STATIC
    src/sm/sm-iso.c
    src/sm/sm-iso.h
    src/sm/sm-iso-internal.h
)
set_target_properties(libsmiso PROPERTIES LINKER_LANGUAGE C)

add_library(libsmeac STATIC
    src/sm/sm-eac.c
    src/sm/sm-eac.h
)
set_target_properties(libsmeac PROPERTIES LINKER_LANGUAGE C)

set(opensc_SOURCES
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
    src/libopensc/card-iasecc.c
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
    src/libopensc/pkcs15-sec.c
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

add_library(opensc ${opensc_SOURCES} $<$<BOOL:${WIN32}>:win32/versioninfo.rc>)
set_target_properties(opensc PROPERTIES LINKER_LANGUAGE C)
target_link_libraries(opensc PRIVATE pkcs15init scconf common notify strings libsmiso libsmeac PUBLIC OpenSSL::Crypto)
target_compile_definitions(opensc PRIVATE HAVE_WINSCARD_H PUBLIC HAVE_CONFIG_H)

if(WIN32)
    target_link_libraries(opensc PUBLIC Winscard ws2_32 shlwapi Comctl32)
    target_compile_definitions(opensc PRIVATE DEFAULT_PCSC_PROVIDER="winscard.dll")
else()
    find_package(PCSC REQUIRED)
    target_link_libraries(opensc PUBLIC ${PCSC_LIBRARIES})
    target_include_directories(opensc PUBLIC ${PCSC_INCLUDE_DIRS})
    target_compile_definitions(opensc PRIVATE HAVE_PCSCLITE_H HAVE_INTTYPES_H DEFAULT_PCSC_PROVIDER="$<IF:$<BOOL:${APPLE}>,/System/Library/Frameworks/PCSC.framework/PCSC,libpcsclite.so.1>")
endif()

if(MSVC)
    target_link_options(opensc PRIVATE
        "/SUBSYSTEM:WINDOWS"
        "/MANIFEST:EMBED"
        "/MANIFESTINPUT:${CMAKE_SOURCE_DIR}/src/libopensc/opensc.dll.manifest"
    )
endif()

file(READ "${CMAKE_SOURCE_DIR}/src/libopensc/libopensc.exports" LIBOPENSC_EXPORTS)
# Remove undefined symbols in ONLY_PIV build
string(REPLACE "npa_reset_retry_counter\n" "" LIBOPENSC_EXPORTS "${LIBOPENSC_EXPORTS}")
string(REGEX REPLACE "sc_pkcs15init_get_.*_ops\n" "" LIBOPENSC_EXPORTS "${LIBOPENSC_EXPORTS}")
file(WRITE "${CMAKE_BINARY_DIR}/libopensc.def" "LIBRARY libopensc\nEXPORTS\n${LIBOPENSC_EXPORTS}")
target_sources(opensc PRIVATE "${CMAKE_BINARY_DIR}/libopensc.def")

install(TARGETS opensc
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
)

add_library(opensc_a STATIC ${opensc_SOURCES})
set_target_properties(opensc_a PROPERTIES LINKER_LANGUAGE C)
target_link_libraries(opensc_a PRIVATE pkcs15init scconf common notify strings libsmiso libsmeac PUBLIC OpenSSL::Crypto)
target_compile_definitions(opensc_a PRIVATE HAVE_WINSCARD_H PUBLIC HAVE_CONFIG_H)

if(WIN32)
    target_link_libraries(opensc_a PUBLIC Winscard ws2_32 shlwapi Comctl32)
    target_compile_definitions(opensc_a PRIVATE DEFAULT_PCSC_PROVIDER="winscard.dll")
else()
    find_package(PCSC REQUIRED)
    target_link_libraries(opensc_a PUBLIC ${PCSC_LIBRARIES})
    target_include_directories(opensc_a PUBLIC ${PCSC_INCLUDE_DIRS})
    target_compile_definitions(opensc_a PRIVATE HAVE_PCSCLITE_H HAVE_INTTYPES_H DEFAULT_PCSC_PROVIDER="$<IF:$<BOOL:${APPLE}>,/System/Library/Frameworks/PCSC.framework/PCSC,libpcsclite.so.1>")
endif()

add_library(opensc-pkcs11 SHARED
    src/pkcs11/debug.c
    src/pkcs11/framework-pkcs15.c
    src/pkcs11/framework-pkcs15init.c
    src/pkcs11/mechanism.c
    src/pkcs11/misc.c
    src/pkcs11/openssl.c
    src/pkcs11/pkcs11-display.c
    src/pkcs11/pkcs11-display.h
    src/pkcs11/pkcs11-global.c
    src/pkcs11/pkcs11-object.c
    src/pkcs11/pkcs11-opensc.h
    src/pkcs11/pkcs11-session.c
    src/pkcs11/pkcs11.h
    src/pkcs11/sc-pkcs11.h
    src/pkcs11/slot.c
    $<$<BOOL:${WIN32}>:src/pkcs11/versioninfo-pkcs11.rc>
)
target_include_directories(opensc-pkcs11
  BEFORE PRIVATE "${CMAKE_BINARY_DIR}/include"
)
set_target_properties(opensc-pkcs11 PROPERTIES LINKER_LANGUAGE C)
target_link_libraries(opensc-pkcs11 PRIVATE opensc_a pkcs15init common scconf notify strings libsmiso libsmeac Threads::Threads PUBLIC OpenSSL::Crypto)
target_compile_definitions(opensc-pkcs11 PUBLIC PKCS11_THREAD_LOCKING HAVE_CONFIG_H)
if(WIN32)
    target_link_libraries(opensc-pkcs11 PUBLIC shlwapi Comctl32)
endif()

if(MSVC)
    target_link_options(opensc-pkcs11 PRIVATE
        "/SUBSYSTEM:WINDOWS"
        "/MANIFEST:EMBED"
        "/MANIFESTINPUT:${CMAKE_SOURCE_DIR}/src/pkcs11/opensc-pkcs11.dll.manifest"
    )
endif()

file(READ "${CMAKE_SOURCE_DIR}/src/pkcs11/pkcs11.exports" PKCS11_EXPORTS)
file(WRITE "${CMAKE_BINARY_DIR}/pkcs11.def" "LIBRARY pkcs11\nEXPORTS\n${PKCS11_EXPORTS}")
target_sources(opensc-pkcs11 PRIVATE "${CMAKE_BINARY_DIR}/pkcs11.def")

install(TARGETS opensc-pkcs11
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

add_library(libsm STATIC
    src/sm/sm-common.c
    src/sm/sm-common.h
)
set_target_properties(libsm PROPERTIES LINKER_LANGUAGE C)
target_link_libraries(libsm PUBLIC OpenSSL::Crypto)
target_compile_definitions(libsm PUBLIC HAVE_CONFIG_H)

add_library(smm-local SHARED
    src/smm/smm-local.c
    src/smm/sm-module.h
    src/smm/sm-global-platform.c
    src/smm/sm-cwa14890.c
    src/smm/sm-card-authentic.c
    src/smm/sm-card-iasecc.c
)
set_target_properties(smm-local PROPERTIES LINKER_LANGUAGE C)
target_link_libraries(smm-local PRIVATE libsm opensc_a pkcs15init common scconf notify strings libsmiso libsmeac)
if(WIN32)
    target_link_libraries(smm-local PUBLIC shlwapi Comctl32)
endif()

if(MSVC)
    target_link_options(smm-local PRIVATE
        "/SUBSYSTEM:WINDOWS"
        "/MANIFEST:EMBED"
        "/MANIFESTINPUT:${CMAKE_SOURCE_DIR}/src/smm/smm-local.dll.manifest"
    )
endif()

file(READ "${CMAKE_SOURCE_DIR}/src/smm/smm-local.exports" SMM_LOCAL_EXPORTS)
file(WRITE "${CMAKE_BINARY_DIR}/smm-local.def" "LIBRARY smm-local\nEXPORTS\n${SMM_LOCAL_EXPORTS}")
target_sources(smm-local PRIVATE "${CMAKE_BINARY_DIR}/smm-local.def")

install(TARGETS smm-local
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
)

add_library(pkcs11-spy SHARED
    src/pkcs11/pkcs11-spy.c
    src/pkcs11/pkcs11-display.c
    src/pkcs11/pkcs11-display.h
    $<$<BOOL:${WIN32}>:src/pkcs11/versioninfo-pkcs11-spy.rc>
)
set_target_properties(pkcs11-spy PROPERTIES LINKER_LANGUAGE C)
target_link_libraries(pkcs11-spy PRIVATE opensc_a pkcs15init common scconf notify strings libsmiso libsmeac PUBLIC OpenSSL::Crypto)
if(WIN32)
    target_link_libraries(pkcs11-spy PUBLIC shlwapi)
endif()

install(TARGETS pkcs11-spy
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
)

add_executable(opensc-tool
    src/tools/opensc-tool.c
    src/tools/util.c
    src/tools/util.h
    $<$<BOOL:${WIN32}>:src/tools/versioninfo-tools.rc>
)
set_target_properties(opensc-tool PROPERTIES LINKER_LANGUAGE C)
target_link_libraries(opensc-tool PRIVATE opensc_a scconf common notify strings)
if(WIN32)
    target_link_libraries(opensc-tool PUBLIC shlwapi Comctl32)
endif()

if(MSVC)
    target_link_options(opensc-tool PRIVATE
        "/MANIFEST:EMBED"
        "/MANIFESTINPUT:${CMAKE_SOURCE_DIR}/src/tools/exe.manifest"
    )
endif()

if(WIN32)
    add_executable(opensc-notify
        src/tools/opensc-notify.c
        $<$<BOOL:${WIN32}>:src/tools/versioninfo-opensc-notify.rc>
    )
else()
    add_executable(opensc-notify
        src/tools/opensc-notify.c
        src/tools/opensc-notify-cmdline.c
        $<$<BOOL:${WIN32}>:src/tools/versioninfo-opensc-notify.rc>
    )
endif()
target_include_directories(opensc-notify PRIVATE ${CMAKE_SOURCE_DIR}/src/tools)
set_target_properties(opensc-notify PROPERTIES LINKER_LANGUAGE C)
target_link_libraries(opensc-notify PRIVATE opensc_a scconf common notify strings)
if(WIN32)
    target_link_libraries(opensc-notify PUBLIC shlwapi Comctl32)
endif()

if(MSVC)
    target_link_options(opensc-notify PRIVATE
        "/SUBSYSTEM:WINDOWS"
        "/MANIFEST:EMBED"
        "/MANIFESTINPUT:${CMAKE_SOURCE_DIR}/src/tools/exe.manifest"
    )
endif()

if(WIN32)
    add_library(opensc-minidriver SHARED
        src/minidriver/minidriver.c
        src/minidriver/versioninfo-minidriver.rc
    )
    set_target_properties(opensc-minidriver PROPERTIES LINKER_LANGUAGE C)
    target_include_directories(opensc-minidriver PRIVATE "C:/Program Files (x86)/Windows Kits/10/Cryptographic Provider Development Kit/Include")

    # Link libraries matching Makefile.mak
    target_link_libraries(opensc-minidriver PRIVATE
        opensc_a pkcs15init scconf common notify strings libsmiso libsmeac
        PUBLIC ws2_32 gdi32 Comctl32 advapi32 Crypt32 User32
        bcrypt DelayImp Rpcrt4 Shell32 Winmm shlwapi
    )

    if(MSVC)
        target_link_options(opensc-minidriver PRIVATE
            "/SUBSYSTEM:WINDOWS"
            "/MANIFEST:EMBED"
            "/MANIFESTINPUT:${CMAKE_SOURCE_DIR}/src/minidriver/opensc-minidriver.dll.manifest"
        )
    endif()

    # Create .def file with exports
    file(READ "${CMAKE_SOURCE_DIR}/src/minidriver/minidriver.exports" MINIDRIVER_EXPORTS)
    file(WRITE "${CMAKE_BINARY_DIR}/opensc-minidriver.def" "LIBRARY opensc-minidriver\nEXPORTS\n${MINIDRIVER_EXPORTS}")
    target_sources(opensc-minidriver PRIVATE "${CMAKE_BINARY_DIR}/opensc-minidriver.def")

    install(TARGETS opensc-minidriver
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION lib
            ARCHIVE DESTINATION lib
    )

    # Install WcaUtil package before building customactions
    add_custom_command(
        OUTPUT "${CMAKE_BINARY_DIR}/wcautil.stamp"
        COMMAND dotnet new console --force --name CustomAction
        COMMAND dotnet add CustomAction package WixToolset.WcaUtil --version 5.0.2 --package-directory packages
        COMMAND ${CMAKE_COMMAND} -E touch "${CMAKE_BINARY_DIR}/wcautil.stamp"
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        COMMENT "Installing WixToolset.WcaUtil package"
        VERBATIM
    )

    add_custom_target(wcautil-headers DEPENDS "${CMAKE_BINARY_DIR}/wcautil.stamp")

    add_library(customactions SHARED
        win32/customactions.cpp
        win32/versioninfo-customactions.rc
    )
    set_target_properties(customactions PROPERTIES LINKER_LANGUAGE CXX)
    target_compile_definitions(customactions PRIVATE ENABLE_MINIDRIVER)

    # Add WcaUtil include directories
    target_include_directories(customactions PRIVATE
        "${CMAKE_BINARY_DIR}/packages/wixtoolset.wcautil/5.0.2/lib/net8.0"
        "${CMAKE_BINARY_DIR}/packages/wixtoolset.wcautil/5.0.2/lib"
        "${CMAKE_BINARY_DIR}/packages/wixtoolset.wcautil/5.0.2/build/native/include"
        "${CMAKE_BINARY_DIR}/packages/wixtoolset.dutil/5.0.2/build/native/include"
    )

    # Map CMAKE_C_COMPILER_ARCHITECTURE_ID to NuGet library architecture
    if(CMAKE_C_COMPILER_ARCHITECTURE_ID STREQUAL "AMD64")
        set(NUGET_ARCH "x64")
    else()
        set(NUGET_ARCH "${CMAKE_C_COMPILER_ARCHITECTURE_ID}")
    endif()

    # Link WcaUtil and DUtil libraries
    target_link_libraries(customactions PRIVATE
        msi Advapi32 User32 Version Shell32
        "${CMAKE_BINARY_DIR}/packages/wixtoolset.wcautil/5.0.2/build/native/v14/${NUGET_ARCH}/wcautil.lib"
        "${CMAKE_BINARY_DIR}/packages/wixtoolset.dutil/5.0.2/build/native/v14/${NUGET_ARCH}/dutil.lib"
    )

    # Make customactions depend on WcaUtil installation
    add_dependencies(customactions wcautil-headers)

    file(READ "${CMAKE_SOURCE_DIR}/win32/customactions.exports" CUSTOMACTIONS_EXPORTS)
    file(WRITE "${CMAKE_BINARY_DIR}/customactions.def" "LIBRARY customactions\nEXPORTS\n${CUSTOMACTIONS_EXPORTS}")
    target_sources(customactions PRIVATE "${CMAKE_BINARY_DIR}/customactions.def")

    configure_file(
        "${CMAKE_SOURCE_DIR}/etc/opensc.conf.in"
        "${CMAKE_BINARY_DIR}/opensc.conf"
        @ONLY
    )

    configure_file(
        "${CMAKE_SOURCE_DIR}/win32/OpenSC.wxs.in"
        "${CMAKE_BINARY_DIR}/OpenSC.wxs"
        @ONLY
    )

    set(WIX_COMMAND
        wix build -arch ${NUGET_ARCH}
        -ext WixToolset.UI.wixext
        -ext WixToolset.Util.wixext
        -d SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -d BINARY_DIR=${CMAKE_BINARY_DIR}
        -d BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -d OpenSSL="${OPENSSL_BINARY_DIR}"
        -d CMAKE_BUILD
        -o "${CMAKE_BINARY_DIR}/OpenSC.msi"
        "${CMAKE_BINARY_DIR}/OpenSC.wxs"
    )

    if(SIGNED_DLLS)
        list(APPEND WIX_COMMAND -d SignedDLLs=1)
    endif()

    add_custom_target(msi-installer
        COMMAND ${WIX_COMMAND}
        DEPENDS customactions opensc-minidriver opensc-pkcs11 opensc smm-local pkcs11-spy opensc-tool opensc-notify
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/win32"
        COMMENT "Building MSI installer package using WiX Toolset (${NUGET_ARCH})"
        VERBATIM
    )
endif()