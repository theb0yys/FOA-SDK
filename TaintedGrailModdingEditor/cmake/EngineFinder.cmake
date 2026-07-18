#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

include_guard()

# This project is owned by this engine fork and intentionally lives directly beneath
# the engine root. Resolve the sibling engine deterministically instead of depending
# on a machine-specific global O3DE registration.
cmake_path(GET CMAKE_CURRENT_LIST_DIR PARENT_PATH tg_editor_project_root)
cmake_path(GET tg_editor_project_root PARENT_PATH tg_editor_engine_root)

set(tg_editor_engine_version_file
    "${tg_editor_engine_root}/cmake/o3deConfigVersion.cmake"
)
if(NOT EXISTS "${tg_editor_engine_version_file}")
    message(FATAL_ERROR
        "Unable to locate the owning O3DE engine beside "
        "${tg_editor_project_root}. Expected ${tg_editor_engine_version_file}."
    )
endif()

include("${tg_editor_engine_version_file}")
if(NOT PACKAGE_VERSION_COMPATIBLE)
    message(FATAL_ERROR
        "The sibling O3DE engine is not compatible with "
        "TaintedGrailModdingEditor."
    )
endif()

list(APPEND CMAKE_MODULE_PATH "${tg_editor_engine_root}/cmake")
