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

install(DIRECTORY "${TG_INSTALLER_PAYLOAD_ROOT}/" DESTINATION ".")

set(CPACK_GENERATOR "WIX")
set(CPACK_PACKAGE_NAME "Tainted Grail FoA Modding SDK")
set(CPACK_PACKAGE_VENDOR "FOA-SDK Contributors")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Prebuilt Tainted Grail: The Fall of Avalon modding Editor and SDK")
set(CPACK_PACKAGE_VERSION "${TG_INSTALLER_VERSION}")
set(CPACK_PACKAGE_FILE_NAME
    "Tainted-Grail-FoA-SDK-${TG_INSTALLER_VERSION}-windows-x64")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Tainted Grail FoA SDK")
set(CPACK_PACKAGE_CHECKSUM "SHA256")
set(CPACK_RESOURCE_FILE_LICENSE "${TG_INSTALLER_PAYLOAD_ROOT}/LICENSE.txt")
set(CPACK_PACKAGE_EXECUTABLES
    "TaintedGrailModdingEditorLauncher"
    "Tainted Grail Modding Editor")

set(CPACK_WIX_VERSION 4)
set(CPACK_WIX_ARCHITECTURE x64)
set(CPACK_WIX_INSTALL_SCOPE perUser)
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
    "Prebuilt development-channel modding Editor and SDK for Tainted Grail: The Fall of Avalon")
set(CPACK_WIX_PROPERTY_ARPHELPLINK "https://github.com/theb0yys/FOA-SDK")
set(CPACK_WIX_PROPERTY_ARPURLINFOABOUT "https://github.com/theb0yys/FOA-SDK")

include(CPack)
