#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

cmake_minimum_required(VERSION 4.3)

set(TG_INSTALLER_VERSION "" CACHE STRING "Three-component SDK installer version")
set(TG_INSTALLER_PAYLOAD_ROOT "" CACHE PATH "Verified SDK staging root")

if(NOT WIN32)
    message(FATAL_ERROR "The Tainted Grail SDK MSI project is Windows-only.")
endif()
if(NOT TG_INSTALLER_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "TG_INSTALLER_VERSION must be MAJOR.MINOR.PATCH.")
endif()
if(NOT IS_DIRECTORY "${TG_INSTALLER_PAYLOAD_ROOT}")
    message(FATAL_ERROR "TG_INSTALLER_PAYLOAD_ROOT must identify a verified staging directory.")
endif()

foreach(required_file IN ITEMS
    INSTALL_MANIFEST.json
    SHA256SUMS
    LICENSE.txt
    NOTICES.txt
    bin/Windows/profile/Default/Editor.exe
    bin/Windows/profile/Default/TaintedGrailModdingEditorLauncher.exe
    bin/Windows/profile/Default/FOA-SDK.exe
    TaintedGrailModdingEditor/project.json
    TaintedGrailModdingEditor/TaintedGrailModdingEditor.ico
)
    if(NOT EXISTS "${TG_INSTALLER_PAYLOAD_ROOT}/${required_file}")
        message(FATAL_ERROR "Verified staging payload is missing ${required_file}.")
    endif()
endforeach()

file(READ "${TG_INSTALLER_PAYLOAD_ROOT}/INSTALL_MANIFEST.json" tg_installer_manifest)
string(JSON tg_manifest_version ERROR_VARIABLE tg_manifest_error
    GET "${tg_installer_manifest}" product_version)
if(tg_manifest_error OR NOT tg_manifest_version STREQUAL TG_INSTALLER_VERSION)
    message(FATAL_ERROR
        "TG_INSTALLER_VERSION must equal the verified staging manifest product_version.")
endif()

project(TaintedGrailFoASDKInstaller VERSION "${TG_INSTALLER_VERSION}" LANGUAGES NONE)

file(GLOB tg_payload_entries
    LIST_DIRECTORIES true
    RELATIVE "${TG_INSTALLER_PAYLOAD_ROOT}"
    "${TG_INSTALLER_PAYLOAD_ROOT}/*")
foreach(tg_payload_entry IN LISTS tg_payload_entries)
    set(tg_payload_source "${TG_INSTALLER_PAYLOAD_ROOT}/${tg_payload_entry}")
    if(IS_DIRECTORY "${tg_payload_source}")
        if(tg_payload_entry STREQUAL "lib")
            set(tg_default_lib_root
                "${TG_INSTALLER_PAYLOAD_ROOT}/lib/Windows/profile/Default")
            install(
                DIRECTORY "${TG_INSTALLER_PAYLOAD_ROOT}/lib/"
                DESTINATION "lib"
                COMPONENT p_lib_base
                PATTERN "Default" EXCLUDE)
            if(IS_DIRECTORY "${tg_default_lib_root}")
                file(GLOB tg_default_lib_entries
                    LIST_DIRECTORIES true
                    RELATIVE "${tg_default_lib_root}"
                    "${tg_default_lib_root}/*")
                foreach(tg_default_lib_entry IN LISTS tg_default_lib_entries)
                    string(SUBSTRING "${tg_default_lib_entry}" 0 1 tg_lib_bucket)
                    string(TOUPPER "${tg_lib_bucket}" tg_lib_bucket)
                    string(REGEX REPLACE "[^A-Z0-9]" "_" tg_lib_bucket "${tg_lib_bucket}")
                    if(tg_lib_bucket STREQUAL "")
                        set(tg_lib_bucket "OTHER")
                    endif()
                    set(tg_lib_component "p_lib_${tg_lib_bucket}")
                    set(tg_lib_source "${tg_default_lib_root}/${tg_default_lib_entry}")
                    if(IS_DIRECTORY "${tg_lib_source}")
                        install(
                            DIRECTORY "${tg_lib_source}/"
                            DESTINATION "lib/Windows/profile/Default/${tg_default_lib_entry}"
                            COMPONENT "${tg_lib_component}")
                    else()
                        install(
                            FILES "${tg_lib_source}"
                            DESTINATION "lib/Windows/profile/Default"
                            COMPONENT "${tg_lib_component}")
                    endif()
                endforeach()
            endif()
        else()
            string(MAKE_C_IDENTIFIER "${tg_payload_entry}" tg_payload_component_suffix)
            install(
                DIRECTORY "${tg_payload_source}/"
                DESTINATION "${tg_payload_entry}"
                COMPONENT "p_${tg_payload_component_suffix}")
        endif()
    else()
        install(
            FILES "${tg_payload_source}"
            DESTINATION "."
            COMPONENT p_root)
    endif()
endforeach()

set(CPACK_GENERATOR "WIX")
set(CPACK_PACKAGE_NAME "Tainted Grail FoA SDK MSI Payload")
set(CPACK_PACKAGE_VENDOR "FOA-SDK Contributors")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Windows Installer payload for the prebuilt Tainted Grail: The Fall of Avalon modding Editor and SDK")
set(CPACK_PACKAGE_VERSION "${TG_INSTALLER_VERSION}")
set(CPACK_PACKAGE_FILE_NAME
    "Tainted-Grail-FoA-SDK-${TG_INSTALLER_VERSION}-windows-x64")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Tainted Grail FoA SDK")
set(CPACK_PACKAGE_CHECKSUM "SHA256")
set(CPACK_RESOURCE_FILE_LICENSE "${TG_INSTALLER_PAYLOAD_ROOT}/LICENSE.txt")
set(CPACK_PACKAGE_EXECUTABLES
    "FOA-SDK"
    "FOA-SDK")

set(CPACK_WIX_VERSION 4)
set(CPACK_WIX_ARCHITECTURE x64)
set(CPACK_WIX_INSTALL_SCOPE perUser)
set(CPACK_WIX_CAB_PER_COMPONENT ON)
set(CPACK_WIX_BUILD_EXTRA_FLAGS "-sw1026")
set(CPACK_WIX_COMPONENT_KEYS_REGISTRY_PATH
    "Software/FOA-SDK/TaintedGrailFoASDK/Components")
set(CPACK_WIX_UPGRADE_GUID "EF05F481-EEED-4CE5-A623-0915EC28F92D")
string(
    UUID CPACK_WIX_PRODUCT_GUID
    NAMESPACE "EF05F481-EEED-4CE5-A623-0915EC28F92D"
    NAME "TaintedGrailFoASDK-${TG_INSTALLER_VERSION}"
    TYPE SHA1
    UPPER
)
set(CPACK_WIX_PRODUCT_ICON
    "${TG_INSTALLER_PAYLOAD_ROOT}/TaintedGrailModdingEditor/TaintedGrailModdingEditor.ico")
set(CPACK_WIX_PROGRAM_MENU_FOLDER "Tainted Grail FoA SDK")
set(CPACK_WIX_PROPERTY_ARPCOMMENTS
    "Windows Installer payload for the prebuilt development-channel modding Editor and SDK for Tainted Grail: The Fall of Avalon")
set(CPACK_WIX_PROPERTY_ARPHELPLINK "https://github.com/theb0yys/FOA-SDK")
set(CPACK_WIX_PROPERTY_ARPURLINFOABOUT "https://github.com/theb0yys/FOA-SDK")

include(CPack)
