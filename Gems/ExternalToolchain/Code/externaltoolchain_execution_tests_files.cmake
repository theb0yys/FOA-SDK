# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT

set(FILES Tests/Main.cpp)
if(foa_m2_operational_tests)
    list(APPEND FILES Tests/ToolExecutionOperationalTests.cpp)
else()
    list(APPEND FILES
        Tests/ToolExecutionContractTests.cpp
        Tests/ToolExecutionServiceTests.cpp
        Tests/ToolExecutionJournalTests.cpp
    )
endif()
