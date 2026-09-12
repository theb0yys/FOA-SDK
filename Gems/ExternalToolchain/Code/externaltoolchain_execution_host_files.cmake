# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT

set(FILES
    Source/Execution/ToolExecutionAdmission.h
    Source/Execution/ToolExecutionService.h
    Source/Execution/ToolExecutionService.cpp
    Source/Execution/ToolProcessBackend.h
    Source/Execution/ToolExecutionJournal.h
    Source/Execution/ToolExecutionJournal.cpp
    Source/Execution/ToolExecutionRedactor.h
    Source/Execution/ToolExecutionRedactor.cpp
)
if(PAL_PLATFORM_NAME STREQUAL "Windows")
    list(APPEND FILES
        Source/Execution/Platform/Windows/ToolSandbox_Windows.h
        Source/Execution/Platform/Windows/ToolSandbox_Windows.cpp
        Source/Execution/Platform/Windows/ToolProcessBackend_Windows.cpp
    )
else()
    list(APPEND FILES Source/Execution/Platform/Common/ToolProcessBackend_Unsupported.cpp)
endif()
