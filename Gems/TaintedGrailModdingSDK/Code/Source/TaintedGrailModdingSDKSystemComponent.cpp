/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "TaintedGrailModdingSDKSystemComponent.h"

#include "AdapterBuildManifestWidget.h"
#include "AdapterCapabilityMatrixWidget.h"
#include "AdapterContractRegistry.h"
#include "AdapterDeploymentExecutionEvidenceWidget.h"
#include "AdapterDeploymentExecutionResultContracts.h"
#include "AdapterDeploymentWorkOrderService.h"
#include "AdapterDeploymentWorkOrderWidget.h"
#include "AdapterPackageAssemblyPreviewService.h"
#include "AdapterPackageAssemblyPreviewWidget.h"
#include "AdapterPostDeploymentVerificationWidget.h"
#include "AdapterPostDeploymentVerifierContracts.h"
#include "AdapterPostDeploymentVerifierWidget.h"
#include "AdapterRuntimeResultContracts.h"
#include "AdapterRuntimeResultEvidenceWidget.h"
#include "AdapterStagingDeploymentPreviewService.h"
#include "AdapterStagingDeploymentPreviewWidget.h"
#include "AdapterVerifierEvidenceReconciliationContracts.h"
#include "AdapterVerifierEvidenceReconciliationWidget.h"
#include "AdapterWorkOrderPlanWidget.h"
#include "ActorTroopEditorWidget.h"
#include "CatalogBrowserWidget.h"
#include "CatalogGovernanceWidget.h"
#include "DevelopmentHubWidget.h"
#include "EconomyCoverageDashboardWidget.h"
#include "EconomyDuplicateReportWidget.h"
#include "EconomyModels.h"
#include "FoundationModels.h"
#include "FoundationService.h"
#include "FoundationStatusWidget.h"
#include "ItemRecipeEditorWidget.h"
#include "PackManagerWidget.h"
#include "SourceEvidenceIntakeWidget.h"

#include <AzCore/Debug/Trace.h>
#include <AzCore/Math/Crc.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzToolsFramework/API/ToolsApplicationAPI.h>
#include <AzToolsFramework/API/ViewPaneOptions.h>
#include <QtCore/QRect>
#include <QtCore/QString>
#include <QtCore/QTimer>
#include <QtCore/qnamespace.h>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        constexpr const char* DevelopmentHubViewPaneName = "FOA Development Hub";
        constexpr const char* FoundationStatusViewPaneName = "Tainted Grail SDK Status";
        constexpr const char* PackManagerViewPaneName = "Tainted Grail Pack Manager";
        constexpr const char* SourceIntakeViewPaneName = "Tainted Grail Source Intake";
        constexpr const char* CatalogBrowserViewPaneName = "Tainted Grail Catalog Browser";
        constexpr const char* CatalogGovernanceViewPaneName = "Tainted Grail Catalog Governance";
        constexpr const char* ItemRecipeEditorViewPaneName = "Tainted Grail Item and Recipe Editor";
        constexpr const char* ActorTroopEditorViewPaneName = "Tainted Grail Actor and Troop Editor";
        constexpr const char* EconomyCoverageDashboardViewPaneName = "Tainted Grail Economy Acquisition Coverage";
        constexpr const char* EconomyDuplicateReportViewPaneName = "Tainted Grail Economy Cross-Pack Duplicates";
        constexpr const char* AdapterCapabilityMatrixViewPaneName = "Tainted Grail Adapter Capability Matrix";
        constexpr const char* AdapterWorkOrderPlanViewPaneName = "Tainted Grail Adapter Work-Order Plans";
        constexpr const char* AdapterRuntimeResultEvidenceViewPaneName =
            "Tainted Grail Adapter Runtime Result Evidence";
        constexpr const char* AdapterBuildManifestViewPaneName =
            "Tainted Grail Adapter Build Manifests";
        constexpr const char* AdapterPackageAssemblyPreviewViewPaneName =
            "Tainted Grail Package Assembly Preview";
        constexpr const char* AdapterStagingDeploymentPreviewViewPaneName =
            "Tainted Grail Staging and Deployment Preview";
        constexpr const char* AdapterDeploymentWorkOrderViewPaneName =
            "Tainted Grail Deployment Confirmation and Work Orders";
        constexpr const char* AdapterDeploymentExecutionEvidenceViewPaneName =
            "Tainted Grail Deployment Execution Result Evidence";
        constexpr const char* AdapterPostDeploymentVerificationViewPaneName =
            "Tainted Grail Post-Deployment Verification and Release Blockers";
        constexpr const char* AdapterPostDeploymentVerifierViewPaneName =
            "Tainted Grail Independent Post-Deployment Verifier Results";
        constexpr const char* AdapterVerifierReconciliationViewPaneName =
            "Tainted Grail Verifier Evidence Reconciliation and Release Decision";
    }

    void TaintedGrailModdingSDKSystemComponent::Reflect(AZ::ReflectContext* context)
    {
        GameProfile::Reflect(context);
        WorkspaceModel::Reflect(context);
        PackManifest::Reflect(context);
        SourceImporterContract::Reflect(context);
        SourceRecord::Reflect(context);
        EvidenceRecord::Reflect(context);
        ImportIssue::Reflect(context);
        SourceDocument::Reflect(context);
        EvidenceDocument::Reflect(context);
        CatalogRecord::Reflect(context);
        CatalogRelationship::Reflect(context);
        CatalogValidationEvent::Reflect(context);
        CatalogGovernanceEvent::Reflect(context);
        EconomyItemProfile::Reflect(context);
        EconomyRecipeProfile::Reflect(context);
        EconomyRecipeIngredient::Reflect(context);
        EconomyRecipeOutput::Reflect(context);
        CatalogDocument::Reflect(context);
        BlockerRecord::Reflect(context);
        DomainCoverage::Reflect(context);
        FoundationSnapshot::Reflect(context);

        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<TaintedGrailModdingSDKSystemComponent, AZ::Component>()
                ->Version(17);
        }
    }

    void TaintedGrailModdingSDKSystemComponent::GetProvidedServices(
        AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("TaintedGrailModdingSDKService"));
    }

    void TaintedGrailModdingSDKSystemComponent::GetIncompatibleServices(
        AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("TaintedGrailModdingSDKService"));
    }

    void TaintedGrailModdingSDKSystemComponent::Activate()
    {
        FoundationService::Get().Initialize();
        AzToolsFramework::EditorEvents::Bus::Handler::BusConnect();

        AZ_Printf(
            "TaintedGrailModdingSDK",
            "Editor foundation activated. FoA runtime execution remains disabled.\n");
    }

    void TaintedGrailModdingSDKSystemComponent::Deactivate()
    {
        if (m_viewRegistered)
        {
            AzToolsFramework::UnregisterViewPane(DevelopmentHubViewPaneName);
            AzToolsFramework::UnregisterViewPane(FoundationStatusViewPaneName);
            AzToolsFramework::UnregisterViewPane(PackManagerViewPaneName);
            AzToolsFramework::UnregisterViewPane(SourceIntakeViewPaneName);
            AzToolsFramework::UnregisterViewPane(CatalogBrowserViewPaneName);
            AzToolsFramework::UnregisterViewPane(CatalogGovernanceViewPaneName);
            AzToolsFramework::UnregisterViewPane(ItemRecipeEditorViewPaneName);
            AzToolsFramework::UnregisterViewPane(ActorTroopEditorViewPaneName);
            AzToolsFramework::UnregisterViewPane(EconomyCoverageDashboardViewPaneName);
            AzToolsFramework::UnregisterViewPane(EconomyDuplicateReportViewPaneName);
            AzToolsFramework::UnregisterViewPane(AdapterCapabilityMatrixViewPaneName);
            AzToolsFramework::UnregisterViewPane(AdapterWorkOrderPlanViewPaneName);
            AzToolsFramework::UnregisterViewPane(AdapterRuntimeResultEvidenceViewPaneName);
            AzToolsFramework::UnregisterViewPane(AdapterBuildManifestViewPaneName);
            AzToolsFramework::UnregisterViewPane(AdapterPackageAssemblyPreviewViewPaneName);
            AzToolsFramework::UnregisterViewPane(AdapterStagingDeploymentPreviewViewPaneName);
            AzToolsFramework::UnregisterViewPane(AdapterDeploymentWorkOrderViewPaneName);
            AzToolsFramework::UnregisterViewPane(AdapterDeploymentExecutionEvidenceViewPaneName);
            AzToolsFramework::UnregisterViewPane(AdapterPostDeploymentVerificationViewPaneName);
            AzToolsFramework::UnregisterViewPane(AdapterPostDeploymentVerifierViewPaneName);
            AzToolsFramework::UnregisterViewPane(AdapterVerifierReconciliationViewPaneName);
            m_viewRegistered = false;
        }

        AdapterVerifierEvidenceReconciliationRegistry::Get().Clear();
        AdapterPostDeploymentVerifierResultRegistry::Get().Clear();
        AdapterDeploymentExecutionResultRegistry::Get().Clear();
        AdapterDeploymentWorkOrderRegistry::Get().Clear();
        AdapterStagingDeploymentPreviewRegistry::Get().Clear();
        AdapterPackageAssemblyPreviewRegistry::Get().Clear();
        AdapterRuntimeResultRegistry::Get().Clear();
        AdapterContractRegistry::Get().Clear();
        AzToolsFramework::EditorEvents::Bus::Handler::BusDisconnect();
        FoundationService::Get().Shutdown();
        AZ_Printf("TaintedGrailModdingSDK", "Editor foundation deactivated.\n");
    }

    void TaintedGrailModdingSDKSystemComponent::NotifyRegisterViews()
    {
        if (m_viewRegistered)
        {
            return;
        }

        AzToolsFramework::ViewPaneOptions hubOptions;
        hubOptions.paneRect = QRect(80, 80, 1180, 980);
        hubOptions.preferedDockingArea = Qt::LeftDockWidgetArea;
        hubOptions.isDeletable = true;
        hubOptions.isPreview = true;
        hubOptions.saveKeyName = QStringLiteral("TaintedGrailModdingSDK.DevelopmentHub");
        AzToolsFramework::RegisterViewPane<DevelopmentHubWidget>(
            DevelopmentHubViewPaneName,
            "Tainted Grail SDK",
            hubOptions);

        AzToolsFramework::ViewPaneOptions statusOptions;
        statusOptions.paneRect = QRect(100, 100, 760, 900);
        statusOptions.preferedDockingArea = Qt::RightDockWidgetArea;
        statusOptions.isDeletable = true;
        statusOptions.isPreview = true;
        statusOptions.saveKeyName = QStringLiteral("TaintedGrailModdingSDK.FoundationStatus");
        AzToolsFramework::RegisterViewPane<FoundationStatusWidget>(
            FoundationStatusViewPaneName,
            "Tainted Grail SDK",
            statusOptions);

        AzToolsFramework::ViewPaneOptions packOptions;
        packOptions.paneRect = QRect(120, 120, 820, 920);
        packOptions.preferedDockingArea = Qt::LeftDockWidgetArea;
        packOptions.isDeletable = true;
        packOptions.isPreview = true;
        packOptions.saveKeyName = QStringLiteral("TaintedGrailModdingSDK.PackManager");
        AzToolsFramework::RegisterViewPane<PackManagerWidget>(
            PackManagerViewPaneName,
            "Tainted Grail SDK",
            packOptions);

        AzToolsFramework::ViewPaneOptions intakeOptions;
        intakeOptions.paneRect = QRect(140, 140, 1000, 900);
        intakeOptions.preferedDockingArea = Qt::BottomDockWidgetArea;
        intakeOptions.isDeletable = true;
        intakeOptions.isPreview = true;
        intakeOptions.saveKeyName = QStringLiteral("TaintedGrailModdingSDK.SourceIntake");
        AzToolsFramework::RegisterViewPane<SourceEvidenceIntakeWidget>(
            SourceIntakeViewPaneName,
            "Tainted Grail SDK",
            intakeOptions);

        AzToolsFramework::ViewPaneOptions catalogOptions;
        catalogOptions.paneRect = QRect(160, 160, 1280, 940);
        catalogOptions.preferedDockingArea = Qt::RightDockWidgetArea;
        catalogOptions.isDeletable = true;
        catalogOptions.isPreview = true;
        catalogOptions.saveKeyName = QStringLiteral("TaintedGrailModdingSDK.CatalogBrowser");
        AzToolsFramework::RegisterViewPane<CatalogBrowserWidget>(
            CatalogBrowserViewPaneName,
            "Tainted Grail SDK",
            catalogOptions);

        AzToolsFramework::ViewPaneOptions governanceOptions;
        governanceOptions.paneRect = QRect(180, 180, 980, 980);
        governanceOptions.preferedDockingArea = Qt::LeftDockWidgetArea;
        governanceOptions.isDeletable = true;
        governanceOptions.isPreview = true;
        governanceOptions.saveKeyName = QStringLiteral("TaintedGrailModdingSDK.CatalogGovernance");
        AzToolsFramework::RegisterViewPane<CatalogGovernanceWidget>(
            CatalogGovernanceViewPaneName,
            "Tainted Grail SDK",
            governanceOptions);

        AzToolsFramework::ViewPaneOptions itemRecipeOptions;
        itemRecipeOptions.paneRect = QRect(200, 200, 1220, 980);
        itemRecipeOptions.preferedDockingArea = Qt::RightDockWidgetArea;
        itemRecipeOptions.isDeletable = true;
        itemRecipeOptions.isPreview = true;
        itemRecipeOptions.saveKeyName = QStringLiteral("TaintedGrailModdingSDK.ItemRecipeEditor");
        AzToolsFramework::RegisterViewPane<ItemRecipeEditorWidget>(
            ItemRecipeEditorViewPaneName,
            "Tainted Grail SDK",
            itemRecipeOptions);

        AzToolsFramework::ViewPaneOptions actorTroopOptions;
        actorTroopOptions.paneRect = QRect(210, 210, 1280, 1000);
        actorTroopOptions.preferedDockingArea = Qt::RightDockWidgetArea;
        actorTroopOptions.isDeletable = true;
        actorTroopOptions.isPreview = true;
        actorTroopOptions.saveKeyName = QStringLiteral("TaintedGrailModdingSDK.ActorTroopEditor");
        AzToolsFramework::RegisterViewPane<ActorTroopEditorWidget>(
            ActorTroopEditorViewPaneName,
            "Tainted Grail SDK",
            actorTroopOptions);

        AzToolsFramework::ViewPaneOptions economyCoverageOptions;
        economyCoverageOptions.paneRect = QRect(220, 220, 1280, 900);
        economyCoverageOptions.preferedDockingArea = Qt::BottomDockWidgetArea;
        economyCoverageOptions.isDeletable = true;
        economyCoverageOptions.isPreview = true;
        economyCoverageOptions.saveKeyName = QStringLiteral("TaintedGrailModdingSDK.EconomyCoverageDashboard");
        AzToolsFramework::RegisterViewPane<EconomyCoverageDashboardWidget>(
            EconomyCoverageDashboardViewPaneName,
            "Tainted Grail SDK",
            economyCoverageOptions);

        AzToolsFramework::ViewPaneOptions economyDuplicateOptions;
        economyDuplicateOptions.paneRect = QRect(240, 240, 1280, 900);
        economyDuplicateOptions.preferedDockingArea = Qt::BottomDockWidgetArea;
        economyDuplicateOptions.isDeletable = true;
        economyDuplicateOptions.isPreview = true;
        economyDuplicateOptions.saveKeyName = QStringLiteral("TaintedGrailModdingSDK.EconomyDuplicateReport");
        AzToolsFramework::RegisterViewPane<EconomyDuplicateReportWidget>(
            EconomyDuplicateReportViewPaneName,
            "Tainted Grail SDK",
            economyDuplicateOptions);

        AzToolsFramework::ViewPaneOptions adapterMatrixOptions;
        adapterMatrixOptions.paneRect = QRect(260, 260, 1320, 920);
        adapterMatrixOptions.preferedDockingArea = Qt::BottomDockWidgetArea;
        adapterMatrixOptions.isDeletable = true;
        adapterMatrixOptions.isPreview = true;
        adapterMatrixOptions.saveKeyName = QStringLiteral("TaintedGrailModdingSDK.AdapterCapabilityMatrix");
        AzToolsFramework::RegisterViewPane<AdapterCapabilityMatrixWidget>(
            AdapterCapabilityMatrixViewPaneName,
            "Tainted Grail SDK",
            adapterMatrixOptions);

        AzToolsFramework::ViewPaneOptions workOrderPlanOptions;
        workOrderPlanOptions.paneRect = QRect(280, 280, 1360, 940);
        workOrderPlanOptions.preferedDockingArea = Qt::BottomDockWidgetArea;
        workOrderPlanOptions.isDeletable = true;
        workOrderPlanOptions.isPreview = true;
        workOrderPlanOptions.saveKeyName = QStringLiteral("TaintedGrailModdingSDK.AdapterWorkOrderPlans");
        AzToolsFramework::RegisterViewPane<AdapterWorkOrderPlanWidget>(
            AdapterWorkOrderPlanViewPaneName,
            "Tainted Grail SDK",
            workOrderPlanOptions);

        AzToolsFramework::ViewPaneOptions runtimeResultOptions;
        runtimeResultOptions.paneRect = QRect(300, 300, 1380, 960);
        runtimeResultOptions.preferedDockingArea = Qt::BottomDockWidgetArea;
        runtimeResultOptions.isDeletable = true;
        runtimeResultOptions.isPreview = true;
        runtimeResultOptions.saveKeyName =
            QStringLiteral("TaintedGrailModdingSDK.AdapterRuntimeResultEvidence");
        AzToolsFramework::RegisterViewPane<AdapterRuntimeResultEvidenceWidget>(
            AdapterRuntimeResultEvidenceViewPaneName,
            "Tainted Grail SDK",
            runtimeResultOptions);

        AzToolsFramework::ViewPaneOptions buildManifestOptions;
        buildManifestOptions.paneRect = QRect(320, 320, 1400, 980);
        buildManifestOptions.preferedDockingArea = Qt::BottomDockWidgetArea;
        buildManifestOptions.isDeletable = true;
        buildManifestOptions.isPreview = true;
        buildManifestOptions.saveKeyName =
            QStringLiteral("TaintedGrailModdingSDK.AdapterBuildManifests");
        AzToolsFramework::RegisterViewPane<AdapterBuildManifestWidget>(
            AdapterBuildManifestViewPaneName,
            "Tainted Grail SDK",
            buildManifestOptions);

        AzToolsFramework::ViewPaneOptions packagePreviewOptions;
        packagePreviewOptions.paneRect = QRect(340, 340, 1420, 1000);
        packagePreviewOptions.preferedDockingArea = Qt::BottomDockWidgetArea;
        packagePreviewOptions.isDeletable = true;
        packagePreviewOptions.isPreview = true;
        packagePreviewOptions.saveKeyName =
            QStringLiteral("TaintedGrailModdingSDK.PackageAssemblyPreview");
        AzToolsFramework::RegisterViewPane<AdapterPackageAssemblyPreviewWidget>(
            AdapterPackageAssemblyPreviewViewPaneName,
            "Tainted Grail SDK",
            packagePreviewOptions);

        AzToolsFramework::ViewPaneOptions deploymentPreviewOptions;
        deploymentPreviewOptions.paneRect = QRect(360, 360, 1440, 1020);
        deploymentPreviewOptions.preferedDockingArea = Qt::BottomDockWidgetArea;
        deploymentPreviewOptions.isDeletable = true;
        deploymentPreviewOptions.isPreview = true;
        deploymentPreviewOptions.saveKeyName =
            QStringLiteral("TaintedGrailModdingSDK.StagingDeploymentPreview");
        AzToolsFramework::RegisterViewPane<AdapterStagingDeploymentPreviewWidget>(
            AdapterStagingDeploymentPreviewViewPaneName,
            "Tainted Grail SDK",
            deploymentPreviewOptions);

        AzToolsFramework::ViewPaneOptions deploymentWorkOrderOptions;
        deploymentWorkOrderOptions.paneRect = QRect(380, 380, 1460, 1040);
        deploymentWorkOrderOptions.preferedDockingArea = Qt::BottomDockWidgetArea;
        deploymentWorkOrderOptions.isDeletable = true;
        deploymentWorkOrderOptions.isPreview = true;
        deploymentWorkOrderOptions.saveKeyName =
            QStringLiteral("TaintedGrailModdingSDK.DeploymentWorkOrders");
        AzToolsFramework::RegisterViewPane<AdapterDeploymentWorkOrderWidget>(
            AdapterDeploymentWorkOrderViewPaneName,
            "Tainted Grail SDK",
            deploymentWorkOrderOptions);

        AzToolsFramework::ViewPaneOptions deploymentExecutionEvidenceOptions;
        deploymentExecutionEvidenceOptions.paneRect = QRect(400, 400, 1480, 1060);
        deploymentExecutionEvidenceOptions.preferedDockingArea = Qt::BottomDockWidgetArea;
        deploymentExecutionEvidenceOptions.isDeletable = true;
        deploymentExecutionEvidenceOptions.isPreview = true;
        deploymentExecutionEvidenceOptions.saveKeyName =
            QStringLiteral("TaintedGrailModdingSDK.DeploymentExecutionEvidence");
        AzToolsFramework::RegisterViewPane<AdapterDeploymentExecutionEvidenceWidget>(
            AdapterDeploymentExecutionEvidenceViewPaneName,
            "Tainted Grail SDK",
            deploymentExecutionEvidenceOptions);

        AzToolsFramework::ViewPaneOptions postDeploymentVerificationOptions;
        postDeploymentVerificationOptions.paneRect = QRect(420, 420, 1500, 1080);
        postDeploymentVerificationOptions.preferedDockingArea =
            Qt::BottomDockWidgetArea;
        postDeploymentVerificationOptions.isDeletable = true;
        postDeploymentVerificationOptions.isPreview = true;
        postDeploymentVerificationOptions.saveKeyName =
            QStringLiteral("TaintedGrailModdingSDK.PostDeploymentVerification");
        AzToolsFramework::RegisterViewPane<AdapterPostDeploymentVerificationWidget>(
            AdapterPostDeploymentVerificationViewPaneName,
            "Tainted Grail SDK",
            postDeploymentVerificationOptions);

        AzToolsFramework::ViewPaneOptions postDeploymentVerifierOptions;
        postDeploymentVerifierOptions.paneRect = QRect(440, 440, 1520, 1100);
        postDeploymentVerifierOptions.preferedDockingArea =
            Qt::BottomDockWidgetArea;
        postDeploymentVerifierOptions.isDeletable = true;
        postDeploymentVerifierOptions.isPreview = true;
        postDeploymentVerifierOptions.saveKeyName =
            QStringLiteral("TaintedGrailModdingSDK.PostDeploymentVerifier");
        AzToolsFramework::RegisterViewPane<AdapterPostDeploymentVerifierWidget>(
            AdapterPostDeploymentVerifierViewPaneName,
            "Tainted Grail SDK",
            postDeploymentVerifierOptions);

        AzToolsFramework::ViewPaneOptions verifierReconciliationOptions;
        verifierReconciliationOptions.paneRect = QRect(460, 460, 1540, 1120);
        verifierReconciliationOptions.preferedDockingArea =
            Qt::BottomDockWidgetArea;
        verifierReconciliationOptions.isDeletable = true;
        verifierReconciliationOptions.isPreview = true;
        verifierReconciliationOptions.saveKeyName =
            QStringLiteral("TaintedGrailModdingSDK.VerifierReconciliation");
        AzToolsFramework::RegisterViewPane<
            AdapterVerifierEvidenceReconciliationWidget>(
            AdapterVerifierReconciliationViewPaneName,
            "Tainted Grail SDK",
            verifierReconciliationOptions);

        m_viewRegistered = true;
    }

    void TaintedGrailModdingSDKSystemComponent::NotifyEditorInitialized()
    {
        if (m_viewRegistered)
        {
            QTimer::singleShot(
                0,
                []()
                {
                    AzToolsFramework::OpenViewPane(DevelopmentHubViewPaneName);
                });
        }
    }
} // namespace TaintedGrailModdingSDK
