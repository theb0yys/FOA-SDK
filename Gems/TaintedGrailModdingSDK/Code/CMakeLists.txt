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
            Gem::ExternalToolchain.API
            AZ::AzToolsFramework
            AZ::SceneCore
            AZ::SceneData
            AZ::AssetBuilderSDK
            Gem::Atom_RPI.Public
            Gem::Atom_RHI.Reflect
)

# The private source-program route currently targets the qualified Windows DX12 host.
if(PAL_PLATFORM_NAME STREQUAL "Windows")
    ly_target_link_libraries(${gem_name}.Framework.Static PRIVATE Gem::Atom_RHI_DX12.Reflect Gem::Atom_RPI.Edit)
    target_compile_definitions(${gem_name}.Framework.Static PRIVATE TG_SOURCE_SHADER_DX12)
endif()
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
            AZ::SceneCore
            AZ::SceneData
            AZ::AssetBuilderSDK
            Gem::Atom_RPI.Public
            Gem::Atom_RHI.Reflect
)

file(TO_CMAKE_PATH "${CMAKE_CURRENT_LIST_DIR}/../Tools/foa_terrain_native_editor.py" tg_native_terrain_tool_source)
ly_add_source_properties(
    SOURCES Source/TerrainNativeHandoff.cpp
    PROPERTY COMPILE_DEFINITIONS
    VALUES TG_SDK_NATIVE_TERRAIN_TOOL_SOURCE="${tg_native_terrain_tool_source}"
)
ly_install_files(FILES ../Tools/foa_terrain_native_editor.py DESTINATION scripts/foa-sdk)

file(TO_CMAKE_PATH "${CMAKE_CURRENT_LIST_DIR}/../Tools/foa_campaign_heightmap_export.py" tg_campaign_export_tool_source)
ly_add_source_properties(
    SOURCES Source/TerrainCampaignExportHost.cpp
    PROPERTY COMPILE_DEFINITIONS
    VALUES TG_SDK_CAMPAIGN_EXPORT_TOOL_SOURCE="${tg_campaign_export_tool_source}"
)
ly_install_files(
    FILES ../Tools/foa_campaign_heightmap_export.py ../Tools/foa_heightmap_importer.py
    DESTINATION scripts/foa-sdk
)

file(TO_CMAKE_PATH "${CMAKE_CURRENT_LIST_DIR}/../Tools/foa_asset_browser_pane_refresh.py" tg_asset_browser_pane_refresh_tool_source)
file(TO_CMAKE_PATH "${CMAKE_CURRENT_LIST_DIR}/../Tools/foa_native_item_preview.py" tg_native_item_preview_source)
file(TO_CMAKE_PATH "${CMAKE_CURRENT_LIST_DIR}/../Tools/tge_editor_connection.py" tg_game_connection_source)
ly_add_source_properties(
    SOURCES Source/GameConnectionWidget.cpp
    PROPERTY COMPILE_DEFINITIONS
    VALUES TG_SDK_GAME_CONNECTION_SOURCE="${tg_game_connection_source}"
)
set(tg_native_item_vendor "${CMAKE_CURRENT_BINARY_DIR}/item-preview-vendor-icons")
set(tg_native_item_requirements "${CMAKE_CURRENT_LIST_DIR}/../Tools/item_preview_requirements.txt")
add_custom_command(
    OUTPUT "${tg_native_item_vendor}/.ready"
    COMMAND "${Python_EXECUTABLE}" -s -m pip --isolated install
        --disable-pip-version-check --only-binary=:all: --no-compile --no-deps --upgrade
        --target "${tg_native_item_vendor}" -r "${tg_native_item_requirements}"
    COMMAND "${CMAKE_COMMAND}" -E touch "${tg_native_item_vendor}/.ready"
    DEPENDS "${tg_native_item_requirements}"
    COMMENT "Preparing the isolated native item preview reader"
    VERBATIM
)
add_custom_target(TaintedGrailNativeItemPreviewReader DEPENDS "${tg_native_item_vendor}/.ready")
add_dependencies(${gem_name}.Editor TaintedGrailNativeItemPreviewReader)
ly_add_source_properties(
    SOURCES Source/NativeItemPreviewService.cpp
    PROPERTY COMPILE_DEFINITIONS
    VALUES
        TG_SDK_NATIVE_ITEM_PREVIEW_SOURCE="${tg_native_item_preview_source}"
        TG_SDK_NATIVE_ITEM_PREVIEW_VENDOR="${tg_native_item_vendor}"
)
ly_add_source_properties(
    SOURCES
        Source/AssetBrowserPreviewRefreshService.cpp
    PROPERTY COMPILE_DEFINITIONS
        VALUES
            TG_SDK_ASSET_BROWSER_PANE_REFRESH_TOOL_SOURCE="${tg_asset_browser_pane_refresh_tool_source}"
)

# Keep both the thin embedded invocation adapter and the single pane-model
# generator owner available in the installed self-contained Editor. Runtime C++
# invokes the adapter through O3DE's embedded Python runner; system Python is not required.
ly_install_files(
    FILES
        ../Tools/foa_asset_browser_pane_refresh.py
        ../Tools/foa_asset_browser_pane_model.py
        ../Tools/foa_native_item_preview.py
        ../Tools/foa_native_economy.py
        ../Tools/foa_native_population.py
        ../Tools/foa_addressables_catalog.py
        ../Tools/item_preview_requirements.txt
        ../Tools/tge_editor_connection.py
        ../Tools/tge_player_position.py
        ../Tools/tge_player_vitals.py
        ../Tools/tge_encounters.py
        ../Tools/tge_sdk_client.py
        ../Tools/tge_sdk_transport.py
    DESTINATION
        scripts/foa-sdk
)
install(DIRECTORY "${tg_native_item_vendor}/" DESTINATION scripts/foa-sdk/item-preview-vendor
    PATTERN ".ready" EXCLUDE PATTERN "__pycache__" EXCLUDE)

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
            Tests/GameConnectionServiceTests.cpp
        PROPERTY COMPILE_DEFINITIONS
            VALUES TG_SDK_TEST_PYTHON="${Python_EXECUTABLE}"
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

    set(tg_canonical_interchange_ctest_command
        "${Python_EXECUTABLE}"
        -B
        -s
        "${CMAKE_CURRENT_LIST_DIR}/../Tools/run_aztest_without_gtest_xml.py"
        $<TARGET_FILE_DIR:${gem_name}.CanonicalInterchange.Tests>/$<TARGET_FILE_NAME:AZ::AzTestRunner>
        $<TARGET_FILE:${gem_name}.CanonicalInterchange.Tests>
        AzRunUnitTests
    )

    ly_add_googletest(
        NAME Gem::${gem_name}.CanonicalInterchange.Tests
        LABELS REQUIRES_tiaf
        TEST_COMMAND ${tg_canonical_interchange_ctest_command}
    )

    ly_add_dependencies(${gem_name}.CanonicalInterchange.Tests AZ::AzTestRunner)

    # M1 execution contracts have no Framework or Editor dependency.
    ly_add_target(
        NAME ${gem_name}.CapabilityExecution.Tests ${PAL_TRAIT_TEST_TARGET_TYPE}
        NAMESPACE Gem
        NO_UNITY
        FILES_CMAKE
            taintedgrailmoddingsdk_capability_execution_tests_files.cmake
        INCLUDE_DIRECTORIES
            PRIVATE
                Source
                Tests
        BUILD_DEPENDENCIES
            PRIVATE
                Gem::${gem_name}.Core.Static
                AZ::AzTest
    )

    set(tg_capability_execution_ctest_command
        "${Python_EXECUTABLE}"
        -B
        -s
        "${CMAKE_CURRENT_LIST_DIR}/../Tools/run_aztest_without_gtest_xml.py"
        $<TARGET_FILE_DIR:${gem_name}.CapabilityExecution.Tests>/$<TARGET_FILE_NAME:AZ::AzTestRunner>
        $<TARGET_FILE:${gem_name}.CapabilityExecution.Tests>
        AzRunUnitTests
    )

    ly_add_googletest(
        NAME Gem::${gem_name}.CapabilityExecution.Tests
        LABELS REQUIRES_tiaf
        TEST_COMMAND ${tg_capability_execution_ctest_command}
    )

    ly_add_dependencies(${gem_name}.CapabilityExecution.Tests AZ::AzTestRunner)
endif()

# Tool Gems are resolved for both Editor/Project Manager tools and builder hosts.
# Core and Framework remain internal implementation targets; only the Editor module is aliased.
ly_create_alias(NAME ${gem_name}.Tools NAMESPACE Gem TARGETS Gem::${gem_name}.Editor)
ly_create_alias(NAME ${gem_name}.Builders NAMESPACE Gem TARGETS Gem::${gem_name}.Editor)

o3de_add_variant_dependencies_for_gem_dependencies(
    GEM_NAME ${gem_name}
    VARIANTS Tools Builders
)
