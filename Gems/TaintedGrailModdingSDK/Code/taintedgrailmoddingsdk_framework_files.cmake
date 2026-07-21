#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

set(FILES
    Source/CatalogGovernanceService.cpp
    Source/CatalogGovernanceService.h
    Source/CatalogPersistenceService.cpp
    Source/CatalogPersistenceService.h
    Source/CatalogPromotionService.cpp
    Source/CatalogPromotionService.h
    Source/EconomyAuthoringService.cpp
    Source/EconomyAuthoringService.h
    Source/ExtensionAPI.cpp
    Source/ExtensionAPI.h
    Source/FoundationCatalogService.cpp
    Source/FoundationEconomyService.cpp
    Source/FoundationExtensionAPI.cpp
    Source/FoundationGovernanceService.cpp
    Source/FoundationNotificationBus.h
    Source/FoundationPersistenceBoundary.cpp
    Source/FoundationPersistenceBoundary.h
    Source/FoundationPopulationService.cpp
    Source/FoundationService.cpp
    Source/FoundationService.h
    Source/FoundationServiceConstruction.cpp
    Source/FoundationValidationService.cpp
    Source/FoundationValidationService.h
    Source/FoundationWorkspaceLoadService.cpp
    Source/FoundationWorkspaceLoadService.h
    Source/PackPersistenceService.cpp
    Source/PackPersistenceService.h
    Source/PathPolicyService.cpp
    Source/PathPolicyService.h
    Source/PathPolicyWorkspaceValidation.cpp
    Source/PersistenceJsonUtils.h
    Source/PopulationAuthoringService.cpp
    Source/PopulationAuthoringService.h
    Source/SourceEvidencePersistenceService.cpp
    Source/SourceEvidencePersistenceService.h
    Source/SourceImportService.cpp
    Source/SourceImportService.h
    Source/WorkspacePersistenceService.cpp
    Source/WorkspacePersistenceService.h
    Source/WorkspaceSchemaService.cpp
    Source/WorkspaceSchemaService.h
)

# Framework implementation files use file-local helpers with intentionally
# repeated names. Compile them as separate translation units so those helpers
# retain normal internal linkage instead of colliding in generated unity files.
set(SKIP_UNITY_BUILD_INCLUSION_FILES)
foreach(source_file IN LISTS FILES)
    if(source_file MATCHES "\\.cpp$")
        list(APPEND SKIP_UNITY_BUILD_INCLUSION_FILES ${source_file})
    endif()
endforeach()
