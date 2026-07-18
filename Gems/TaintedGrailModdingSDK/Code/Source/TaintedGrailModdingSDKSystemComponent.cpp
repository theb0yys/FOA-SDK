/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "TaintedGrailModdingSDKSystemComponent.h"

#include "AdapterCapabilityMatrixWidget.h"
#include "AdapterContractRegistry.h"
#include "CatalogBrowserWidget.h"
#include "CatalogGovernanceWidget.h"
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
#include <AzToolsFramework/API/ViewPaneOptions.h>
#include <QtCore/QRect>
#include <QtCore/QString>
#include <QtCore/qnamespace.h>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        constexpr const char* FoundationStatusViewPaneName = "Tainted Grail SDK Status";
        constexpr const char* PackManagerViewPaneName = "Tainted Grail Pack Manager";
        constexpr const char* SourceIntakeViewPaneName = "Tainted Grail Source Intake";
        constexpr const char* CatalogBrowserViewPaneName = "Tainted Grail Catalog Browser";
        constexpr const char* CatalogGovernanceViewPaneName = "Tainted Grail Catalog Governance";
        constexpr const char* ItemRecipeEditorViewPaneName = "Tainted Grail Item and Recipe Editor";
        constexpr const char* EconomyCoverageDashboardViewPaneName = "Tainted Grail Economy Acquisition Coverage";
        constexpr const char* EconomyDuplicateReportViewPaneName = "Tainted Grail Economy Cross-Pack Duplicates";
        constexpr const char* AdapterCapabilityMatrixViewPaneName = "Tainted Grail Adapter Capability Matrix";
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
                ->Version(7);
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
            AzToolsFramework::UnregisterViewPane(FoundationStatusViewPaneName);
            AzToolsFramework::UnregisterViewPane(PackManagerViewPaneName);
            AzToolsFramework::UnregisterViewPane(SourceIntakeViewPaneName);
            AzToolsFramework::UnregisterViewPane(CatalogBrowserViewPaneName);
            AzToolsFramework::UnregisterViewPane(CatalogGovernanceViewPaneName);
            AzToolsFramework::UnregisterViewPane(ItemRecipeEditorViewPaneName);
            AzToolsFramework::UnregisterViewPane(EconomyCoverageDashboardViewPaneName);
            AzToolsFramework::UnregisterViewPane(EconomyDuplicateReportViewPaneName);
            AzToolsFramework::UnregisterViewPane(AdapterCapabilityMatrixViewPaneName);
            m_viewRegistered = false;
        }

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

        m_viewRegistered = true;
    }
} // namespace TaintedGrailModdingSDK
