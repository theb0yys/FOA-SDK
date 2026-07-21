#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

include_guard()

# FOA-SDK is the product checkout. Resolve the exact external O3DE checkout
# without depending on a machine-specific global O3DE registration.
cmake_path(GET CMAKE_CURRENT_LIST_DIR PARENT_PATH tg_editor_project_root)
cmake_path(GET tg_editor_project_root PARENT_PATH tg_editor_product_root)

if(DEFINED ENV{FOA_O3DE_ROOT} AND NOT "$ENV{FOA_O3DE_ROOT}" STREQUAL "")
    cmake_path(SET tg_editor_engine_root NORMALIZE "$ENV{FOA_O3DE_ROOT}")
else()
    set(tg_editor_engine_checkout_directory "o3de")
    set(tg_editor_engine_lock "${tg_editor_product_root}/o3de.lock.json")
    if(EXISTS "${tg_editor_engine_lock}")
        file(READ "${tg_editor_engine_lock}" tg_editor_engine_lock_json)
        string(JSON tg_editor_engine_checkout_directory
            ERROR_VARIABLE tg_editor_engine_lock_error
            GET "${tg_editor_engine_lock_json}" checkout_directory
        )
        if(tg_editor_engine_lock_error)
            message(FATAL_ERROR
                "Unable to read checkout_directory from ${tg_editor_engine_lock}: "
                "${tg_editor_engine_lock_error}"
            )
        endif()
    endif()

    cmake_path(GET tg_editor_product_root PARENT_PATH tg_editor_development_root)
    cmake_path(APPEND tg_editor_development_root
        "${tg_editor_engine_checkout_directory}"
        OUTPUT_VARIABLE tg_editor_engine_root
    )
    cmake_path(NORMAL_PATH tg_editor_engine_root)
endif()

set(tg_editor_engine_version_file
    "${tg_editor_engine_root}/cmake/o3deConfigVersion.cmake"
)
if(NOT EXISTS "${tg_editor_engine_version_file}")
    message(FATAL_ERROR
        "Unable to locate the pinned external O3DE engine for "
        "${tg_editor_project_root}. Expected ${tg_editor_engine_version_file}. "
        "Set FOA_O3DE_ROOT or place the checkout beside FOA-SDK."
    )
endif()

include("${tg_editor_engine_version_file}")
if(NOT PACKAGE_VERSION_COMPATIBLE)
    message(FATAL_ERROR
        "The external O3DE engine is not compatible with "
        "TaintedGrailModdingEditor."
    )
endif()

list(APPEND CMAKE_MODULE_PATH "${tg_editor_engine_root}/cmake")
