/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#pragma once
#include "../AdapterDeploymentWorkOrderService.h"
#include "../AdapterPostDeploymentVerificationService.h"
#include "../CapabilityExecutionValidation.h"
#include "../TerrainHeightmapDocument.h"

namespace TaintedGrailModdingSDK::ExecutionFramework
{
    namespace CE = CapabilityExecution;
    enum class PlannerSourceKind
    {
        Build,
        Package,
        Deployment,
        WorkOrder,
        TerrainBuild
    };

    // Original owner bytes stay in memory. Only the digest reference is an M1 value.
    struct PlannerSource
    {
        AZStd::string m_canonicalJson;
        CE::PhaseExtensionReferenceV1 m_reference;
        // Populated only for TerrainBuild. The owner JSON binds these exact bytes by digest.
        TerrainHeightmap::NativeTerrainBuildInputV1 m_terrainInput;
    };

    // Immutable, in-memory preview input. It is neither an execution plan nor an authorization.
    class PlannerSnapshot
    {
    public:
        const CE::CapabilityExecutionRequestV1& GetRequest() const
        {
            return m_request;
        }
        AZ::Outcome<const PlannerSource*, AZStd::string> ReadSource(
            PlannerSourceKind kind, CE::Phase phase, const CE::CapabilityExecutionRequestV1& request) const;

    private:
        friend class FrameworkPlannerService;
        struct Source
        {
            PlannerSourceKind m_kind;
            CE::Phase m_phase;
            PlannerSource m_value;
        };
        CE::CapabilityExecutionRequestV1 m_request;
        AZStd::vector<Source> m_sources;
    };

    class FrameworkPlannerService
    {
    public:
        AdapterWorkOrderPlanSet BuildPlans(
            const WorkspaceModel&,
            const AZStd::vector<PackManifest>&,
            const AdapterContractRegistry&,
            const SourceEvidenceRegistry&,
            const CatalogDatabase&,
            const AZStd::vector<BlockerRecord>&) const;
        AdapterBuildManifest BuildManifest(const AdapterBuildManifestRequest&) const;
        AdapterPackageAssemblyPreview BuildPreview(const AdapterPackageAssemblyPreviewRequest&) const;
        AdapterStagingDeploymentPreview BuildPreview(const AdapterStagingDeploymentPreviewRequest&) const;
        AdapterDeploymentWorkOrder BuildWorkOrder(const AdapterDeploymentWorkOrderRequest&) const;
        AdapterPostDeploymentVerificationReport BuildReport(
            const AdapterDeploymentWorkOrder&,
            const AdapterDeploymentExecutionResultEnvelope&,
            const AdapterDeploymentExecutionEvidenceReturn&) const;

        static constexpr const char* TerrainBuildCapabilityId = "capability.world.heightmap";
        // Captures a Core-validated revision for BUILD preview only. No provider or permission is created.
        AZ::Outcome<PlannerSnapshot, AZStd::string> BindTerrainBuild(
            const CE::CapabilityExecutionRequestV1&, const AZStd::string& workspaceRoot,
            const AZStd::string& manifestRelativePath, const TerrainHeightmap::ProfileBinding&,
            const AZStd::string& expectedDocumentFingerprint, const TerrainHeightmap::ImportControl* = nullptr) const;

        // Explicit worker-side operations. They never replan at execution time or authorize a provider.
        AZ::Outcome<PlannerSnapshot, AZStd::string> BindBuild(
            const CE::CapabilityExecutionRequestV1&, const AdapterBuildManifestRequest&) const;
        AZ::Outcome<PlannerSnapshot, AZStd::string> BindPackage(
            const PlannerSnapshot& build, const AdapterPackageAssemblyPreviewRequest&) const;
        AZ::Outcome<PlannerSnapshot, AZStd::string> BindDeployment(
            const PlannerSnapshot& package, const AdapterStagingDeploymentPreviewRequest&, const AdapterDeploymentWorkOrderRequest&) const;

    private:
        static AZ::Outcome<void, AZStd::string> Append(PlannerSnapshot&, PlannerSourceKind, CE::Phase, const AZStd::string&);
    };
} // namespace TaintedGrailModdingSDK::ExecutionFramework
