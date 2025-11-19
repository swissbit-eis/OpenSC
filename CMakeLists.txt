cmake_minimum_required(VERSION 3.15.0 FATAL_ERROR)

if(NOT DEFINED OPENSC_VERSION)
    set(OPENSC_VERSION "0.0.0")
endif()
if (NOT DEFINED OPENSC_SCM_REVISION)
    set(OPENSC_SCM_REVISION "0")
endif()

project("OpenSC" LANGUAGES "C" "CXX" VERSION ${OPENSC_VERSION})
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules)

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
if(HAVE_BUILTIN_OVERFLOW)
    target_compile_definitions(common PRIVATE HAVE_BUILTIN_OVERFLOW)
endif()

add_library(pkcs11 STATIC
    src/common/libpkcs11.c
    src/common/libpkcs11.h
)
set_target_properties(pkcs11 PROPERTIES LINKER_LANGUAGE C)

add_library(scdl STATIC
    src/common/libscdl.c
    src/common/libscdl.h
)
set_target_properties(scdl PROPERTIES LINKER_LANGUAGE C)

add_library(notify STATIC
    src/ui/notify.h
    src/ui/notify.c
    src/ui/strings.h
    src/ui/strings.c
)
set_target_properties(notify PROPERTIES LINKER_LANGUAGE C)

add_library(scconf STATIC
    src/scconf/scconf.c
    src/scconf/scconf.h
    src/scconf/parse.c
    src/scconf/sclex.c
    src/scconf/write.c
)
set_target_properties(scconf PROPERTIES LINKER_LANGUAGE C)

add_library(util STATIC
    src/tools/util.c
)
set_target_properties(util PROPERTIES LINKER_LANGUAGE C)

add_library(pkcs15init STATIC
    src/pkcs15init/pkcs15-lib.c
    src/pkcs15init/profile.c
)
set_target_properties(pkcs15init PROPERTIES LINKER_LANGUAGE C)
target_link_libraries(pkcs15init PRIVATE OpenSSL::Crypto)

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
set_target_properties(opensc PROPERTIES LINKER_LANGUAGE C)
target_link_libraries(opensc PRIVATE common notify PUBLIC OpenSSL::Crypto)
target_compile_definitions(opensc PRIVATE HAVE_WINSCARD_H PUBLIC HAVE_CONFIG_H)

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
set_target_properties(opensc-tool PROPERTIES LINKER_LANGUAGE C)
target_link_libraries(opensc-tool PRIVATE opensc scconf util)

if(WIN32)
    add_library(opensc-minidriver SHARED
        src/minidriver/minidriver.c
        src/minidriver/versioninfo-minidriver.rc
    )
    set_target_properties(opensc-minidriver PROPERTIES LINKER_LANGUAGE C)
    target_include_directories(opensc-minidriver PRIVATE "C:/Program Files (x86)/Windows Kits/10/Cryptographic Provider Development Kit/Include")

    # Link libraries matching Makefile.mak
    target_link_libraries(opensc-minidriver PRIVATE
        opensc pkcs15init scconf common scdl notify
        ws2_32 gdi32 Comctl32 advapi32 Crypt32 User32
        bcrypt DelayImp Rpcrt4 Shell32 Winmm shlwapi
    )

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

    # Map CMAKE_SYSTEM_PROCESSOR to NuGet library architecture
    if(CMAKE_SYSTEM_PROCESSOR STREQUAL "AMD64")
        set(NUGET_ARCH "x64")
    else()
        set(NUGET_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
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
        "${CMAKE_SOURCE_DIR}/win32/OpenSC.wxs.in"
        "${CMAKE_BINARY_DIR}/OpenSC.wxs"
        @ONLY
    )

    add_custom_target(msi-installer
        COMMAND wix build -arch ${NUGET_ARCH}
                -ext WixToolset.UI.wixext
                -ext WixToolset.Util.wixext
                -d SOURCE_DIR=${CMAKE_SOURCE_DIR}
                -o "${CMAKE_BINARY_DIR}/OpenSC.msi"
                "${CMAKE_BINARY_DIR}/OpenSC.wxs"
        DEPENDS customactions opensc-minidriver opensc opensc-tool
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        COMMENT "Building MSI installer package using WiX Toolset (${NUGET_ARCH})"
        VERBATIM
    )
endif()