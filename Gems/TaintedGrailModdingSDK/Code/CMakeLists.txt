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
    NAME ${gem_name}.Core.Static STATIC
    NAMESPACE Gem
    FILES_CMAKE
        taintedgrailmoddingsdk_core_files.cmake
    INCLUDE_DIRECTORIES
        PUBLIC
            Source
    BUILD_DEPENDENCIES
        PUBLIC
            AZ::AzCore
)

# Preserve the reviewed Schema-1 field writer under explicit legacy names so a
# small public boundary can enforce the accepted omit-when-empty display_name
# rule and bind its dependent manifest/package digests without duplicating the
# canonical implementation.
ly_add_source_properties(
    SOURCES
        Source/CanonicalInterchangeCanonical.cpp
    PROPERTY COMPILE_DEFINITIONS
        VALUES
            SerializeCanonicalManifestV1=SerializeCanonicalManifestIncludingEmptyDisplayNameV1
            SerializeDeclaredPackageFingerprintInputV1=SerializeDeclaredPackageFingerprintInputIncludingEmptyDisplayNameV1
            CalculateCanonicalManifestDigestV1=CalculateCanonicalManifestDigestIncludingEmptyDisplayNameV1
            CalculateDeclaredPackageFingerprintV1=CalculateDeclaredPackageFingerprintIncludingEmptyDisplayNameV1
            CanonicalManifestBytesMatchV1=CanonicalManifestBytesMatchIncludingEmptyDisplayNameV1
)

ly_add_target(
    NAME ${gem_name}.Framework.Static STATIC
    NAMESPACE Gem
    FILES_CMAKE
        taintedgrailmoddingsdk_framework_files.cmake
    INCLUDE_DIRECTORIES
        PUBLIC
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
        taintedgrailmoddingsdk_editor_files.cmake
    INCLUDE_DIRECTORIES
        PRIVATE
            Source
    BUILD_DEPENDENCIES
        PRIVATE
            Gem::${gem_name}.Framework.Static
            AZ::AzToolsFramework
)

ly_add_source_properties(
    SOURCES
        Source/TaintedGrailModdingSDKEditorModule.cpp
    PROPERTY COMPILE_DEFINITIONS
        VALUES
            O3DE_GEM_NAME=${gem_name}
            O3DE_GEM_VERSION=${gem_version}
)

if(PAL_PLATFORM_NAME STREQUAL "Windows")
    ly_add_target(
        NAME TaintedGrailModdingEditorLauncher APPLICATION
        NAMESPACE Gem
        FILES_CMAKE
            taintedgrailmoddingsdk_installed_launcher_files.cmake
    )
endif()

if(PAL_TRAIT_BUILD_TESTS_SUPPORTED AND PAL_TRAIT_TEST_GOOGLE_TEST_SUPPORTED)
    ly_add_target(
        NAME ${gem_name}.Catalog.Tests ${PAL_TRAIT_TEST_TARGET_TYPE}
        NAMESPACE Gem
        NO_UNITY
        FILES_CMAKE
            taintedgrailmoddingsdk_catalog_tests_files.cmake
            taintedgrailmoddingsdk_population_hardening_tests_files.cmake
            taintedgrailmoddingsdk_extension_api_tests_files.cmake
            taintedgrailmoddingsdk_tainted_framework_tests_files.cmake
            taintedgrailmoddingsdk_path_policy_tests_files.cmake
            taintedgrailmoddingsdk_work_order_tests_files.cmake
            taintedgrailmoddingsdk_runtime_result_tests_files.cmake
            taintedgrailmoddingsdk_build_manifest_tests_files.cmake
            taintedgrailmoddingsdk_package_preview_tests_files.cmake
            taintedgrailmoddingsdk_deployment_preview_tests_files.cmake
            taintedgrailmoddingsdk_deployment_work_order_tests_files.cmake
            taintedgrailmoddingsdk_deployment_execution_result_tests_files.cmake
            taintedgrailmoddingsdk_post_deployment_verification_tests_files.cmake
            taintedgrailmoddingsdk_post_deployment_verifier_tests_files.cmake
            taintedgrailmoddingsdk_verifier_reconciliation_tests_files.cmake
            taintedgrailmoddingsdk_release_artifact_tests_files.cmake
            taintedgrailmoddingsdk_release_assembly_result_tests_files.cmake
            taintedgrailmoddingsdk_release_signing_result_tests_files.cmake
            taintedgrailmoddingsdk_external_tool_interchange_tests_files.cmake
        INCLUDE_DIRECTORIES
            PRIVATE
                Source
                Tests
        BUILD_DEPENDENCIES
            PRIVATE
                Gem::${gem_name}.Framework.Static
                AZ::AzTest
                AZ::AzToolsFramework
    )

    ly_add_source_properties(
        SOURCES
            Tests/CatalogSchemaMigrationPersistenceTests.cpp
            Tests/DeveloperPreviewSmokeTests.cpp
            Tests/WorkspaceSchemaServiceTests.cpp
        PROPERTY COMPILE_DEFINITIONS
            VALUES
                TG_SDK_PREVIEW_TEMPLATE_ROOT="${CMAKE_CURRENT_LIST_DIR}/../Preview/Template"
    )

    ly_add_googletest(
        NAME Gem::${gem_name}.Catalog.Tests
        LABELS REQUIRES_tiaf
    )

    # Gate 5 canonical interchange tests deliberately link Core only. This
    # prevents Framework, Editor, provider, runtime, Qt, or AzToolsFramework
    # dependencies from silently entering the inert canonical contract.
    ly_add_target(
        NAME ${gem_name}.CanonicalInterchange.Tests ${PAL_TRAIT_TEST_TARGET_TYPE}
        NAMESPACE Gem
        NO_UNITY
        FILES_CMAKE
            taintedgrailmoddingsdk_canonical_interchange_tests_files.cmake
        INCLUDE_DIRECTORIES
            PRIVATE
                Source
                Tests
        BUILD_DEPENDENCIES
            PRIVATE
                Gem::${gem_name}.Core.Static
                AZ::AzTest
    )

    ly_add_googletest(
        NAME Gem::${gem_name}.CanonicalInterchange.Tests
        LABELS REQUIRES_tiaf
    )
endif()

# Tool Gems are resolved for both Editor/Project Manager tools and builder hosts.
# Core and Framework remain internal implementation targets; only the Editor module is aliased.
ly_create_alias(NAME ${gem_name}.Tools NAMESPACE Gem TARGETS Gem::${gem_name}.Editor)
ly_create_alias(NAME ${gem_name}.Builders NAMESPACE Gem TARGETS Gem::${gem_name}.Editor)

o3de_add_variant_dependencies_for_gem_dependencies(
    GEM_NAME ${gem_name}
    VARIANTS Tools Builders
)
