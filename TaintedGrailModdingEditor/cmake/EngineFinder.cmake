#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

include_guard()

# FOA-SDK is the product checkout. Resolve and verify the exact external O3DE
# checkout without depending on a machine-specific global O3DE registration.
cmake_path(GET CMAKE_CURRENT_LIST_DIR PARENT_PATH tg_editor_project_root)
cmake_path(GET tg_editor_project_root PARENT_PATH tg_editor_product_root)

set(tg_editor_engine_lock "${tg_editor_product_root}/o3de.lock.json")
if(NOT EXISTS "${tg_editor_engine_lock}")
    message(FATAL_ERROR
        "The FOA-SDK O3DE dependency lock is missing: ${tg_editor_engine_lock}."
    )
endif()

file(READ "${tg_editor_engine_lock}" tg_editor_engine_lock_json)
string(JSON tg_editor_engine_checkout_directory
    ERROR_VARIABLE tg_editor_checkout_directory_error
    GET "${tg_editor_engine_lock_json}" checkout_directory
)
if(tg_editor_checkout_directory_error)
    message(FATAL_ERROR
        "Unable to read checkout_directory from ${tg_editor_engine_lock}: "
        "${tg_editor_checkout_directory_error}"
    )
endif()
string(JSON tg_editor_expected_engine_commit
    ERROR_VARIABLE tg_editor_engine_commit_error
    GET "${tg_editor_engine_lock_json}" commit
)
if(tg_editor_engine_commit_error)
    message(FATAL_ERROR
        "Unable to read commit from ${tg_editor_engine_lock}: "
        "${tg_editor_engine_commit_error}"
    )
endif()

if(DEFINED ENV{FOA_O3DE_ROOT} AND NOT "$ENV{FOA_O3DE_ROOT}" STREQUAL "")
    cmake_path(SET tg_editor_engine_root NORMALIZE "$ENV{FOA_O3DE_ROOT}")
else()
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

find_program(tg_editor_git_executable NAMES git REQUIRED)
execute_process(
    COMMAND "${tg_editor_git_executable}" -C "${tg_editor_engine_root}" rev-parse HEAD
    RESULT_VARIABLE tg_editor_git_result
    OUTPUT_VARIABLE tg_editor_actual_engine_commit
    ERROR_VARIABLE tg_editor_git_error
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT tg_editor_git_result EQUAL 0)
    message(FATAL_ERROR
        "Unable to resolve the external O3DE checkout commit: ${tg_editor_git_error}"
    )
endif()
string(TOLOWER "${tg_editor_actual_engine_commit}" tg_editor_actual_engine_commit)
string(TOLOWER "${tg_editor_expected_engine_commit}" tg_editor_expected_engine_commit)
if(NOT tg_editor_actual_engine_commit STREQUAL tg_editor_expected_engine_commit)
    message(FATAL_ERROR
        "External O3DE commit mismatch. Expected ${tg_editor_expected_engine_commit}, "
        "found ${tg_editor_actual_engine_commit}. Checkout the commit recorded in "
        "${tg_editor_engine_lock}."
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
