#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

set(FILES
    Source/ActorTroopEditorWidget.cpp
    Source/ActorTroopEditorWidget.h
    Source/AdapterBuildManifestWidget.cpp
    Source/AdapterBuildManifestWidget.h
    Source/AdapterCapabilityMatrixWidget.cpp
    Source/AdapterCapabilityMatrixWidget.h
    Source/AdapterDeploymentExecutionEvidenceWidget.cpp
    Source/AdapterDeploymentExecutionEvidenceWidget.h
    Source/AdapterDeploymentWorkOrderWidget.cpp
    Source/AdapterDeploymentWorkOrderWidget.h
    Source/AdapterPackageAssemblyPreviewWidget.cpp
    Source/AdapterPackageAssemblyPreviewWidget.h
    Source/AdapterPostDeploymentVerificationWidget.cpp
    Source/AdapterPostDeploymentVerificationWidget.h
    Source/AdapterPostDeploymentVerifierWidget.cpp
    Source/AdapterPostDeploymentVerifierWidget.h
    Source/AdapterReleaseArtifactPaneSystemComponent.cpp
    Source/AdapterReleaseArtifactPaneSystemComponent.h
    Source/AdapterReleaseArtifactWidget.cpp
    Source/AdapterReleaseArtifactWidget.h
    Source/AdapterReleaseAssemblyPaneSystemComponent.cpp
    Source/AdapterReleaseAssemblyPaneSystemComponent.h
    Source/AdapterReleaseAssemblyResultWidget.cpp
    Source/AdapterReleaseAssemblyResultWidget.h
    Source/AdapterReleaseSigningPaneSystemComponent.cpp
    Source/AdapterReleaseSigningPaneSystemComponent.h
    Source/AdapterReleaseSigningResultWidget.cpp
    Source/AdapterReleaseSigningResultWidget.h
    Source/AdapterRuntimeResultEvidenceWidget.cpp
    Source/AdapterRuntimeResultEvidenceWidget.h
    Source/AdapterStagingDeploymentPreviewWidget.cpp
    Source/AdapterStagingDeploymentPreviewWidget.h
    Source/AdapterVerifierEvidenceReconciliationWidget.cpp
    Source/AdapterVerifierEvidenceReconciliationWidget.h
    Source/AdapterWorkOrderPlanWidget.cpp
    Source/AdapterWorkOrderPlanWidget.h
    Source/CatalogBrowserWidget.cpp
    Source/CatalogBrowserWidget.h
    Source/CatalogGovernanceWidget.cpp
    Source/CatalogGovernanceWidget.h
    Source/DevelopmentHubWidget.cpp
    Source/DevelopmentHubWidget.h
    Source/EconomyCoverageDashboardWidget.cpp
    Source/EconomyCoverageDashboardWidget.h
    Source/EconomyDuplicateReportWidget.cpp
    Source/EconomyDuplicateReportWidget.h
    Source/FoundationStatusWidget.cpp
    Source/FoundationStatusWidget.h
    Source/ItemRecipeEditorWidget.cpp
    Source/ItemRecipeEditorWidget.h
    Source/PackManagerWidget.cpp
    Source/PackManagerWidget.h
    Source/SourceEvidenceIntakeWidget.cpp
    Source/SourceEvidenceIntakeWidget.h
    Source/TaintedGrailModdingSDKEditorModule.cpp
    Source/TaintedGrailModdingSDKSystemComponent.cpp
    Source/TaintedGrailModdingSDKSystemComponent.h
)

# Editor implementation files use repeated file-local presentation helpers.
# Keep each implementation in its own translation unit so unity concatenation
# cannot merge their anonymous namespaces and create duplicate definitions.
set(SKIP_UNITY_BUILD_INCLUSION_FILES)
foreach(source_file IN LISTS FILES)
    if(source_file MATCHES "\\.cpp$")
        list(APPEND SKIP_UNITY_BUILD_INCLUSION_FILES ${source_file})
    endif()
endforeach()
