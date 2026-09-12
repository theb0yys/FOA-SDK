#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

if(NOT PAL_TRAIT_BUILD_HOST_TOOLS)
    return()
endif()

ly_add_target(
    NAME ${gem_name}.API INTERFACE
    NAMESPACE Gem
    FILES_CMAKE
        externaltoolchain_api_files.cmake
    INCLUDE_DIRECTORIES
        INTERFACE
            Include
    BUILD_DEPENDENCIES
        INTERFACE
            AZ::AzCore
)

ly_add_target(
    NAME ${gem_name}.Core.Static STATIC
    NAMESPACE Gem
    FILES_CMAKE
        externaltoolchain_core_files.cmake
    INCLUDE_DIRECTORIES
        PUBLIC
            Include
        PRIVATE
            Source
    BUILD_DEPENDENCIES
        PUBLIC
            Gem::${gem_name}.API
            AZ::AzCore
)

ly_add_target(
    NAME ${gem_name}.Editor.Private.Static STATIC
    NAMESPACE Gem
    FILES_CMAKE
        externaltoolchain_editor_private_files.cmake
    INCLUDE_DIRECTORIES
        PUBLIC
            Include
        PRIVATE
            Source
    BUILD_DEPENDENCIES
        PUBLIC
            Gem::${gem_name}.Core.Static
        PRIVATE
            AZ::AzToolsFramework
)

ly_add_target(
    NAME ${gem_name}.Editor GEM_MODULE
    NAMESPACE Gem
    FILES_CMAKE
        externaltoolchain_editor_shared_files.cmake
    INCLUDE_DIRECTORIES
        PRIVATE
            Include
            Source
    BUILD_DEPENDENCIES
        PRIVATE
            Gem::${gem_name}.Editor.Private.Static
            AZ::AzToolsFramework
)

ly_add_source_properties(
    SOURCES
        Source/Editor/ExternalToolchainEditorModule.cpp
    PROPERTY COMPILE_DEFINITIONS
        VALUES
            O3DE_GEM_NAME=${gem_name}
            O3DE_GEM_VERSION=${gem_version}
)

ly_create_alias(NAME ${gem_name}.Tools NAMESPACE Gem TARGETS Gem::${gem_name}.Editor)
ly_create_alias(NAME ${gem_name}.Builders NAMESPACE Gem TARGETS Gem::${gem_name}.Editor)
ly_create_alias(NAME ${gem_name}.Tools.API NAMESPACE Gem TARGETS Gem::${gem_name}.API)
ly_create_alias(NAME ${gem_name}.Builders.API NAMESPACE Gem TARGETS Gem::${gem_name}.API)

o3de_add_variant_dependencies_for_gem_dependencies(
    GEM_NAME ${gem_name}
    VARIANTS Tools Builders
)

if(PAL_TRAIT_BUILD_TESTS_SUPPORTED AND PAL_TRAIT_TEST_GOOGLE_TEST_SUPPORTED)
    ly_add_target(
        NAME ${gem_name}.Tests ${PAL_TRAIT_TEST_TARGET_TYPE}
        NAMESPACE Gem
        FILES_CMAKE
            externaltoolchain_tests_files.cmake
        INCLUDE_DIRECTORIES
            PRIVATE
                Include
                Source
                Tests
        BUILD_DEPENDENCIES
            PRIVATE
                Gem::${gem_name}.Core.Static
                AZ::AzTest
    )

    ly_add_googletest(
        NAME Gem::${gem_name}.Tests
        LABELS REQUIRES_tiaf
        TEST_COMMAND "${Python_EXECUTABLE}" -B -s
            "${CMAKE_CURRENT_LIST_DIR}/../../TaintedGrailModdingSDK/Tools/run_aztest_without_gtest_xml.py"
            $<TARGET_FILE:AZ::AzTestRunner> $<TARGET_FILE:${gem_name}.Tests> AzRunUnitTests
    )
endif()

# M2 contracts are independent of the discovery Core and product Framework.
# The private durable journal is implemented only by the Windows execution host.
set(foa_m2_private_journal 0)
if(PAL_PLATFORM_NAME STREQUAL "Windows")
    set(foa_m2_private_journal 1)
endif()
ly_add_target(
    NAME ${gem_name}.Execution.Core.Static STATIC
    NAMESPACE Gem
    NO_UNITY
    FILES_CMAKE externaltoolchain_execution_core_files.cmake
    INCLUDE_DIRECTORIES PUBLIC Include
    BUILD_DEPENDENCIES PUBLIC Gem::${gem_name}.API AZ::AzCore
)
ly_add_target(
    NAME ${gem_name}.Execution.Host.Static STATIC
    NAMESPACE Gem
    NO_UNITY
    FILES_CMAKE externaltoolchain_execution_host_files.cmake
    INCLUDE_DIRECTORIES PUBLIC Include PRIVATE Source
    COMPILE_DEFINITIONS PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN
        AZ_TRAIT_EXTERNAL_TOOLCHAIN_PRIVATE_JOURNAL=${foa_m2_private_journal}
    BUILD_DEPENDENCIES PUBLIC Gem::${gem_name}.Execution.Core.Static AZ::AzCore
)
if(PAL_PLATFORM_NAME STREQUAL "Windows")
    target_link_libraries(${gem_name}.Execution.Host.Static PRIVATE Userenv Advapi32 Ole32)
endif()
target_link_libraries(${gem_name}.Editor.Private.Static PRIVATE Gem::${gem_name}.Execution.Host.Static)

if(PAL_TRAIT_BUILD_TESTS_SUPPORTED AND PAL_TRAIT_TEST_GOOGLE_TEST_SUPPORTED)
    set(foa_m2_operational_tests OFF)
    ly_add_target(
        NAME ${gem_name}.Execution.Tests ${PAL_TRAIT_TEST_TARGET_TYPE}
        NAMESPACE Gem
        NO_UNITY
        FILES_CMAKE externaltoolchain_execution_tests_files.cmake
        INCLUDE_DIRECTORIES PRIVATE Include Source Tests
        COMPILE_DEFINITIONS PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN
        AZ_TRAIT_EXTERNAL_TOOLCHAIN_PRIVATE_JOURNAL=${foa_m2_private_journal}
        BUILD_DEPENDENCIES PRIVATE Gem::${gem_name}.Execution.Host.Static AZ::AzTest
    )
    set(foa_m2_test_wrapper "${CMAKE_CURRENT_LIST_DIR}/../../TaintedGrailModdingSDK/Tools/run_aztest_without_gtest_xml.py")
    ly_add_googletest(
        NAME Gem::${gem_name}.Execution.Tests
        LABELS REQUIRES_tiaf
        TEST_COMMAND "${Python_EXECUTABLE}" -B -s "${foa_m2_test_wrapper}"
            $<TARGET_FILE:AZ::AzTestRunner> $<TARGET_FILE:${gem_name}.Execution.Tests> AzRunUnitTests
    )
    ly_add_dependencies(${gem_name}.Execution.Tests AZ::AzTestRunner)
    if(PAL_PLATFORM_NAME STREQUAL "Windows")
        ly_add_target(
            NAME ${gem_name}.Execution.Fixture APPLICATION
            NAMESPACE Gem
            NO_UNITY
            FILES_CMAKE externaltoolchain_execution_fixture_files.cmake
            COMPILE_DEFINITIONS PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN
        AZ_TRAIT_EXTERNAL_TOOLCHAIN_PRIVATE_JOURNAL=${foa_m2_private_journal}
        )
        set_target_properties(${gem_name}.Execution.Fixture PROPERTIES WIN32_EXECUTABLE FALSE)
        target_link_libraries(${gem_name}.Execution.Fixture PRIVATE Bcrypt Ws2_32)
        set(foa_m2_operational_tests ON)
        ly_add_target(
            NAME ${gem_name}.Execution.Operational.Tests ${PAL_TRAIT_TEST_TARGET_TYPE}
            NAMESPACE Gem
            NO_UNITY
            FILES_CMAKE externaltoolchain_execution_tests_files.cmake
            INCLUDE_DIRECTORIES PRIVATE Include Source Tests
            COMPILE_DEFINITIONS PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN
        AZ_TRAIT_EXTERNAL_TOOLCHAIN_PRIVATE_JOURNAL=${foa_m2_private_journal}
            BUILD_DEPENDENCIES PRIVATE Gem::${gem_name}.Execution.Host.Static AZ::AzTest
        )
        ly_add_googletest(
            NAME Gem::${gem_name}.Execution.Operational.Tests
            LABELS REQUIRES_tiaf
            TEST_COMMAND "${CMAKE_COMMAND}" -E env "FOA_M2_FIXTURE=$<TARGET_FILE:${gem_name}.Execution.Fixture>"
                "${Python_EXECUTABLE}" -B -s "${foa_m2_test_wrapper}"
                $<TARGET_FILE:AZ::AzTestRunner> $<TARGET_FILE:${gem_name}.Execution.Operational.Tests> AzRunUnitTests
        )
        ly_add_dependencies(${gem_name}.Execution.Operational.Tests AZ::AzTestRunner ${gem_name}.Execution.Fixture)
    endif()
    unset(foa_m2_operational_tests)
endif()
