/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "DevelopmentHubWidget.h"

#include "FoundationModels.h"
#include "FoundationService.h"
#include "LocalSetupDetectionService.h"

#include <AzToolsFramework/API/ToolsApplicationAPI.h>

#include <QFont>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QString>
#include <QTimer>
#include <QVBoxLayout>

#include <cstring>
#include <initializer_list>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        constexpr const char* DevelopmentHubPane = "FOA Development Hub";
        constexpr const char* FoundationStatusPane = "Tainted Grail SDK Status";
        constexpr const char* PackManagerPane = "Tainted Grail Pack Manager";
        constexpr const char* SourceIntakePane = "Tainted Grail Source Intake";
        constexpr const char* AssetBrowserPreviewPane = "Tainted Grail Asset Browser Preview";
        constexpr const char* CatalogBrowserPane = "Tainted Grail Catalog Browser";
        constexpr const char* CatalogGovernancePane = "Tainted Grail Catalog Governance";
        constexpr const char* ItemRecipeEditorPane = "Tainted Grail Item and Recipe Editor";
        constexpr const char* QuestStateInspectorPane = "Tainted Grail Quest and State Inspector";
        constexpr const char* ActorTroopEditorPane = "Tainted Grail Actor and Troop Editor";
        constexpr const char* RoadAtlasEditorPane = "Tainted Grail Map Editor (Road Atlas)";
        constexpr const char* AvalonAIEditorPane = "Tainted Grail Avalon AI Editor";
        constexpr const char* EconomyCoveragePane = "Tainted Grail Economy Acquisition Coverage";
        constexpr const char* EconomyDuplicatesPane = "Tainted Grail Economy Cross-Pack Duplicates";
        constexpr const char* AdapterCapabilityPane = "Tainted Grail Adapter Capability Matrix";
        constexpr const char* AdapterPlanPane = "Tainted Grail Adapter Work-Order Plans";
        constexpr const char* RuntimeEvidencePane = "Tainted Grail Adapter Runtime Result Evidence";
        constexpr const char* BuildManifestPane = "Tainted Grail Adapter Build Manifests";
        constexpr const char* PackagePreviewPane = "Tainted Grail Package Assembly Preview";
        constexpr const char* StagingPreviewPane = "Tainted Grail Staging and Deployment Preview";
        constexpr const char* DeploymentWorkOrderPane =
            "Tainted Grail Deployment Confirmation and Work Orders";
        constexpr const char* DeploymentEvidencePane =
            "Tainted Grail Deployment Execution Result Evidence";
        constexpr const char* PostDeploymentPane =
            "Tainted Grail Post-Deployment Verification and Release Blockers";
        constexpr const char* IndependentVerifierPane =
            "Tainted Grail Independent Post-Deployment Verifier Results";
        constexpr const char* ReconciliationPane =
            "Tainted Grail Verifier Evidence Reconciliation and Release Decision";
        constexpr const char* ReleaseArtifactPane =
            "Tainted Grail Release Artifact Provenance and Signing Intent";
        constexpr const char* ReleaseAssemblyPane =
            "Tainted Grail Release Assembly and Checksum Results";
        constexpr const char* ReleaseSigningPane =
            "Tainted Grail Release Signing Results";

        struct HubRoute
        {
            QString m_label;
            QString m_description;
            const char* m_paneName;
        };

        QString ToQString(const AZStd::string& value)
        {
            return QString::fromUtf8(value.c_str());
        }

        QString DisplayValue(const AZStd::string& value, const QString& fallback)
        {
            return value.empty() ? fallback : ToQString(value);
        }

        bool IsAdvancedOnlyUsage(const AZStd::string& usage)
        {
            return usage == "build"
                || usage == "package"
                || usage == "deploy"
                || usage == "release"
                || usage == "runtime_handoff"
                || usage == "all_runtime_actions";
        }

        bool IsNormalAuthoringIssue(const BlockerRecord& blocker)
        {
            if (blocker.m_severity != "error")
            {
                return false;
            }

            if (blocker.m_blockerId.find("foundation.pack.profile-mismatch.") == 0
                || blocker.m_blockerId.find("foundation.pack.game-target.") == 0)
            {
                return true;
            }

            if (blocker.m_affectedUsages.empty())
            {
                return true;
            }

            for (const AZStd::string& usage : blocker.m_affectedUsages)
            {
                if (!IsAdvancedOnlyUsage(usage))
                {
                    return true;
                }
            }
            return false;
        }

        QPushButton* CreateRouteButton(
            QWidget* parent,
            const QString& label,
            const QString& description,
            const char* paneName)
        {
            auto* button = new QPushButton(label, parent);
            button->setMinimumWidth(190);
            button->setAccessibleName(label);
            button->setAccessibleDescription(description);
            button->setToolTip(description);
            QObject::connect(button, &QPushButton::clicked, parent, [paneName]()
            {
                AzToolsFramework::OpenViewPane(paneName);
                if (std::strcmp(paneName, AssetBrowserPreviewPane) == 0)
                {
                    QTimer::singleShot(0, []()
                    {
                        AzToolsFramework::CloseViewPane(DevelopmentHubPane);
                    });
                }
            });
            return button;
        }

        QGroupBox* CreateRouteGroup(
            QWidget* parent,
            const QString& title,
            const QString& introduction,
            std::initializer_list<HubRoute> routes)
        {
            auto* group = new QGroupBox(title, parent);
            auto* layout = new QVBoxLayout(group);

            if (!introduction.isEmpty())
            {
                auto* introductionLabel = new QLabel(introduction, group);
                introductionLabel->setWordWrap(true);
                layout->addWidget(introductionLabel);
            }

            for (const HubRoute& route : routes)
            {
                auto* row = new QWidget(group);
                auto* rowLayout = new QHBoxLayout(row);
                rowLayout->setContentsMargins(0, 0, 0, 0);

                rowLayout->addWidget(CreateRouteButton(
                    row,
                    route.m_label,
                    route.m_description,
                    route.m_paneName));

                auto* description = new QLabel(route.m_description, row);
                description->setWordWrap(true);
                rowLayout->addWidget(description, 1);
                layout->addWidget(row);
            }

            return group;
        }
    } // namespace

    DevelopmentHubWidget::DevelopmentHubWidget(QWidget* parent)
        : QWidget(parent)
    {
        FoundationNotificationBus::Handler::BusConnect();

        setMinimumWidth(420);
        setMaximumWidth(760);

        auto* rootLayout = new QVBoxLayout(this);
        auto* scrollArea = new QScrollArea(this);
        scrollArea->setWidgetResizable(true);
        auto* content = new QWidget(scrollArea);
        auto* contentLayout = new QVBoxLayout(content);
        scrollArea->setWidget(content);
        rootLayout->addWidget(scrollArea);

        auto* heading = new QLabel(tr("FOA-SDK Home"), content);
        QFont headingFont = heading->font();
        headingFont.setPointSize(headingFont.pointSize() + 5);
        headingFont.setBold(true);
        heading->setFont(headingFont);
        contentLayout->addWidget(heading);

        auto* description = new QLabel(
            tr("FOA-SDK handles the local game and workspace setup automatically. "
               "Open or create a mod, then choose what you want to edit."),
            content);
        description->setWordWrap(true);
        contentLayout->addWidget(description);

        m_statusHeadline = new QLabel(content);
        QFont statusFont = m_statusHeadline->font();
        statusFont.setPointSize(statusFont.pointSize() + 2);
        statusFont.setBold(true);
        m_statusHeadline->setFont(statusFont);
        m_statusHeadline->setWordWrap(true);
        contentLayout->addWidget(m_statusHeadline);

        auto* contextGroup = new QGroupBox(tr("Current project"), content);
        auto* contextLayout = new QFormLayout(contextGroup);
        m_setupValue = new QLabel(contextGroup);
        m_gameValue = new QLabel(contextGroup);
        m_packValue = new QLabel(contextGroup);
        m_blockersValue = new QLabel(contextGroup);
        for (QLabel* label : {
                 m_setupValue,
                 m_gameValue,
                 m_packValue,
                 m_blockersValue })
        {
            label->setWordWrap(true);
            label->setTextInteractionFlags(Qt::TextSelectableByKeyboard | Qt::TextSelectableByMouse);
        }
        contextLayout->addRow(tr("System"), m_setupValue);
        contextLayout->addRow(tr("Fall of Avalon"), m_gameValue);
        contextLayout->addRow(tr("Current mod"), m_packValue);
        contextLayout->addRow(tr("Authoring issues"), m_blockersValue);
        contentLayout->addWidget(contextGroup);

        auto* startGroup = new QGroupBox(tr("Start"), content);
        auto* startLayout = new QVBoxLayout(startGroup);

        auto* actionRow = new QWidget(startGroup);
        auto* actionLayout = new QHBoxLayout(actionRow);
        actionLayout->setContentsMargins(0, 0, 0, 0);
        m_setupButton = CreateRouteButton(
            actionRow,
            tr("System details"),
            tr("Check automatic game detection, versions, workspace paths, and setup diagnostics."),
            FoundationStatusPane);
        m_packButton = CreateRouteButton(
            actionRow,
            tr("Create or open a mod"),
            tr("Create, select, or manage the current mod project."),
            PackManagerPane);
        actionLayout->addWidget(m_setupButton);
        actionLayout->addWidget(m_packButton);
        startLayout->addWidget(actionRow);

        m_primaryHint = new QLabel(startGroup);
        m_primaryHint->setWordWrap(true);
        startLayout->addWidget(m_primaryHint);
        contentLayout->addWidget(startGroup);

        m_authoringGroup = CreateRouteGroup(
            content,
            tr("Create and edit"),
            tr("These are the main authoring tools. FOA-SDK supplies the detected game/profile context automatically."),
            {
                { tr("Game assets"), tr("Browse imported game assets and previews."), AssetBrowserPreviewPane },
                { tr("Map editor"), tr("Build and edit map and road content."), RoadAtlasEditorPane },
                { tr("Items and recipes"), tr("Create and edit items, recipes, and economy data."), ItemRecipeEditorPane },
                { tr("Actors and troops"), tr("Create and edit actors and troop composition."), ActorTroopEditorPane },
                { tr("Quests and state"), tr("Inspect and work with quest/state definitions."), QuestStateInspectorPane },
                { tr("Avalon AI"), tr("Create and edit Avalon AI packages and plans."), AvalonAIEditorPane },
            });
        contentLayout->addWidget(m_authoringGroup);

        auto* advancedToggleRow = new QWidget(content);
        auto* advancedToggleLayout = new QHBoxLayout(advancedToggleRow);
        advancedToggleLayout->setContentsMargins(0, 0, 0, 0);
        advancedToggleLayout->addStretch(1);
        m_advancedToggleButton = new QPushButton(tr("Show advanced tools"), advancedToggleRow);
        advancedToggleLayout->addWidget(m_advancedToggleButton);
        contentLayout->addWidget(advancedToggleRow);

        m_advancedGroup = new QGroupBox(tr("Advanced tools"), content);
        auto* advancedLayout = new QVBoxLayout(m_advancedGroup);
        auto* advancedDescription = new QLabel(
            tr("Diagnostics, evidence, packaging, adapter, and release surfaces are kept here so they do not clutter normal authoring."),
            m_advancedGroup);
        advancedDescription->setWordWrap(true);
        advancedLayout->addWidget(advancedDescription);

        advancedLayout->addWidget(CreateRouteGroup(
            m_advancedGroup,
            tr("Data and diagnostics"),
            QString(),
            {
                { tr("Source and evidence intake"), tr("Import and inspect local source/evidence material."), SourceIntakePane },
                { tr("Catalog browser"), tr("Inspect canonical records and relationships."), CatalogBrowserPane },
                { tr("Catalog governance"), tr("Review validation and usage decisions."), CatalogGovernancePane },
                { tr("Economy coverage"), tr("Review acquisition-path coverage and blockers."), EconomyCoveragePane },
                { tr("Cross-pack duplicates"), tr("Review exact duplicate signals across mods."), EconomyDuplicatesPane },
            }));

        advancedLayout->addWidget(CreateRouteGroup(
            m_advancedGroup,
            tr("Adapters and packaging"),
            QString(),
            {
                { tr("Adapter capability matrix"), tr("Inspect adapter compatibility and readiness."), AdapterCapabilityPane },
                { tr("Work-order plans"), tr("Inspect generated non-executable adapter plans."), AdapterPlanPane },
                { tr("Runtime result evidence"), tr("Inspect supplied adapter result evidence."), RuntimeEvidencePane },
                { tr("Build manifests"), tr("Inspect reproducible adapter build definitions."), BuildManifestPane },
                { tr("Package assembly preview"), tr("Inspect deterministic package layout."), PackagePreviewPane },
                { tr("Staging and deployment preview"), tr("Inspect intended staging/deployment changes and rollback steps."), StagingPreviewPane },
                { tr("Deployment work orders"), tr("Inspect confirmation and operator work orders."), DeploymentWorkOrderPane },
                { tr("Deployment result evidence"), tr("Inspect supplied deployment execution evidence."), DeploymentEvidencePane },
            }));

        advancedLayout->addWidget(CreateRouteGroup(
            m_advancedGroup,
            tr("Verification and release"),
            QString(),
            {
                { tr("Post-deployment verification"), tr("Inspect compatibility and release blockers."), PostDeploymentPane },
                { tr("Independent verifier results"), tr("Inspect supplied independent verifier observations."), IndependentVerifierPane },
                { tr("Evidence reconciliation"), tr("Review evidence dispositions and release decisions."), ReconciliationPane },
                { tr("Release artifact provenance"), tr("Inspect provenance and signing intent metadata."), ReleaseArtifactPane },
                { tr("Release assembly results"), tr("Inspect supplied archive/checksum result evidence."), ReleaseAssemblyPane },
                { tr("Release signing results"), tr("Inspect supplied signing-result metadata."), ReleaseSigningPane },
            }));

        m_advancedGroup->setVisible(false);
        contentLayout->addWidget(m_advancedGroup);

        connect(m_advancedToggleButton, &QPushButton::clicked, this, [this]()
        {
            const bool show = !m_advancedGroup->isVisible();
            m_advancedGroup->setVisible(show);
            m_advancedToggleButton->setText(
                show ? tr("Hide advanced tools") : tr("Show advanced tools"));
        });

        contentLayout->addStretch();
        FoundationService::Get().RefreshLocalSetup();
        Refresh();
    }

    DevelopmentHubWidget::~DevelopmentHubWidget()
    {
        FoundationNotificationBus::Handler::BusDisconnect();
    }

    void DevelopmentHubWidget::OnFoundationChanged()
    {
        Refresh();
    }

    void DevelopmentHubWidget::Refresh()
    {
        FoundationService& service = FoundationService::Get();
        const FoundationSnapshot snapshot = service.GetSnapshot();
        const WorkspaceModel& workspace = service.GetWorkspace();
        const GameProfile* profile = workspace.FindActiveGameProfile();

        const bool workspaceReady =
            !snapshot.m_workspaceFilePath.empty()
            && !workspace.m_rootPath.empty()
            && !workspace.m_outputPath.empty()
            && !workspace.m_stagingPath.empty()
            && !workspace.m_deploymentPath.empty();
        const bool gameFound = profile
            && LocalSetupDetectionService::LooksLikeTaintedGrailInstall(profile->m_installPath);
        const bool profileReady = profile && profile->IsConfigured();
        const bool setupReady = workspaceReady && gameFound && profileReady;
        const bool hasPack = !snapshot.m_activePackId.empty();

        AZ::u64 authoringIssueCount = 0;
        if (hasPack)
        {
            for (const BlockerRecord& blocker : snapshot.m_blockers)
            {
                if (IsNormalAuthoringIssue(blocker))
                {
                    ++authoringIssueCount;
                }
            }
        }
        const bool hasAuthoringIssues = authoringIssueCount > 0;

        m_setupValue->setText(setupReady ? tr("Ready") : tr("Needs attention"));
        m_setupValue->setToolTip(
            DisplayValue(snapshot.m_workspaceFilePath, tr("FOA-SDK manages the workspace automatically.")));

        if (setupReady)
        {
            m_gameValue->setText(
                tr("%1 · %2")
                    .arg(DisplayValue(snapshot.m_gameVersion, tr("version unknown")))
                    .arg(DisplayValue(snapshot.m_runtimeTarget, tr("runtime unknown"))));
            m_gameValue->setToolTip(
                tr("Profile: %1\nBranch: %2")
                    .arg(DisplayValue(snapshot.m_activeGameProfile, tr("unknown")))
                    .arg(DisplayValue(snapshot.m_branch, tr("unknown"))));
        }
        else if (gameFound)
        {
            m_gameValue->setText(tr("Found · setup incomplete"));
            m_gameValue->setToolTip(tr("Open System details to complete the automatic workspace/profile check."));
        }
        else
        {
            m_gameValue->setText(tr("Not found"));
            m_gameValue->setToolTip(tr("Open System details to locate Fall of Avalon."));
        }

        if (hasPack)
        {
            QString packText = DisplayValue(snapshot.m_activePackName, tr("Current mod"));
            if (!snapshot.m_activePackVersion.empty())
            {
                packText += tr(" · %1").arg(ToQString(snapshot.m_activePackVersion));
            }
            m_packValue->setText(packText);
        }
        else
        {
            m_packValue->setText(tr("None selected"));
        }

        m_blockersValue->setText(
            hasAuthoringIssues
                ? tr("%1 need review").arg(authoringIssueCount)
                : tr("None"));

        if (!setupReady)
        {
            m_statusHeadline->setText(tr("Finish setup to start authoring"));
            m_setupButton->setText(tr("Fix setup"));
            m_setupButton->setAccessibleName(tr("Fix setup"));
            m_packButton->setText(tr("Create or open a mod"));
            m_packButton->setAccessibleName(tr("Create or open a mod"));
            m_packButton->setEnabled(false);
            m_primaryHint->setText(
                gameFound
                    ? tr("FOA-SDK found the game but still needs to finish the automatic workspace/profile check. Open System details for the exact item.")
                    : tr("FOA-SDK could not find Fall of Avalon. Open System details and locate the game once; all dependent paths will be derived automatically."));
        }
        else if (!hasPack)
        {
            m_statusHeadline->setText(tr("Create or open a mod"));
            m_setupButton->setText(tr("System details"));
            m_setupButton->setAccessibleName(tr("System details"));
            m_packButton->setText(tr("Create or open a mod"));
            m_packButton->setAccessibleName(tr("Create or open a mod"));
            m_packButton->setEnabled(true);
            m_primaryHint->setText(
                tr("Fall of Avalon and the workspace are ready. Create or select a mod to enable the authoring tools."));
        }
        else
        {
            m_statusHeadline->setText(
                hasAuthoringIssues
                    ? tr("Ready to author · %1 issue(s) need review").arg(authoringIssueCount)
                    : tr("Ready to author"));
            m_setupButton->setText(tr("System details"));
            m_setupButton->setAccessibleName(tr("System details"));
            m_packButton->setText(tr("Manage mod"));
            m_packButton->setAccessibleName(tr("Manage mod"));
            m_packButton->setEnabled(true);
            m_primaryHint->setText(
                tr("Continue %1 or choose an editor below.")
                    .arg(DisplayValue(snapshot.m_activePackName, tr("the current mod"))));
        }

        m_authoringGroup->setEnabled(setupReady && hasPack);
    }
} // namespace TaintedGrailModdingSDK