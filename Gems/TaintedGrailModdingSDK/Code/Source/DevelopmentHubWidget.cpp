/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "DevelopmentHubWidget.h"

#include "FoundationModels.h"
#include "FoundationService.h"

#include <AzToolsFramework/API/ToolsApplicationAPI.h>

#include <QFont>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QString>
#include <QVBoxLayout>

#include <initializer_list>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        constexpr const char* FoundationStatusPane = "Tainted Grail SDK Status";
        constexpr const char* PackManagerPane = "Tainted Grail Pack Manager";
        constexpr const char* SourceIntakePane = "Tainted Grail Source Intake";
        constexpr const char* CatalogBrowserPane = "Tainted Grail Catalog Browser";
        constexpr const char* CatalogGovernancePane = "Tainted Grail Catalog Governance";
        constexpr const char* ItemRecipeEditorPane = "Tainted Grail Item and Recipe Editor";
        constexpr const char* ActorTroopEditorPane = "Tainted Grail Actor and Troop Editor";
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

        QPushButton* CreateRouteButton(
            QWidget* parent,
            const QString& label,
            const QString& description,
            const char* paneName)
        {
            auto* button = new QPushButton(label, parent);
            button->setMinimumWidth(260);
            button->setAccessibleName(label);
            button->setAccessibleDescription(description);
            button->setToolTip(description);
            QObject::connect(button, &QPushButton::clicked, parent, [paneName]()
            {
                AzToolsFramework::OpenViewPane(paneName);
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

            auto* introductionLabel = new QLabel(introduction, group);
            introductionLabel->setWordWrap(true);
            layout->addWidget(introductionLabel);

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

        auto* rootLayout = new QVBoxLayout(this);
        auto* scrollArea = new QScrollArea(this);
        scrollArea->setWidgetResizable(true);
        auto* content = new QWidget(scrollArea);
        auto* contentLayout = new QVBoxLayout(content);
        scrollArea->setWidget(content);
        rootLayout->addWidget(scrollArea);

        auto* heading = new QLabel(tr("FOA Development Hub"), content);
        QFont headingFont = heading->font();
        headingFont.setPointSize(headingFont.pointSize() + 5);
        headingFont.setBold(true);
        heading->setFont(headingFont);
        contentLayout->addWidget(heading);

        auto* description = new QLabel(
            tr("Resume the current workspace and move through the governed authoring flow. "
               "This Hub presents shared Foundation state and opens existing specialist panes; "
               "it does not duplicate validation, grant permission, execute adapters, deploy files, "
               "or launch FoA."),
            content);
        description->setWordWrap(true);
        contentLayout->addWidget(description);

        auto* contextGroup = new QGroupBox(tr("Current context"), content);
        auto* contextLayout = new QFormLayout(contextGroup);
        m_workspaceValue = new QLabel(contextGroup);
        m_profileValue = new QLabel(contextGroup);
        m_packValue = new QLabel(contextGroup);
        m_validationValue = new QLabel(contextGroup);
        m_blockersValue = new QLabel(contextGroup);
        m_dirtyValue = new QLabel(contextGroup);
        for (QLabel* label : {
                 m_workspaceValue,
                 m_profileValue,
                 m_packValue,
                 m_validationValue,
                 m_blockersValue,
                 m_dirtyValue })
        {
            label->setWordWrap(true);
            label->setTextInteractionFlags(Qt::TextSelectableByKeyboard | Qt::TextSelectableByMouse);
        }
        contextLayout->addRow(tr("Workspace"), m_workspaceValue);
        contextLayout->addRow(tr("Game profile"), m_profileValue);
        contextLayout->addRow(tr("Active pack"), m_packValue);
        contextLayout->addRow(tr("Validation state"), m_validationValue);
        contextLayout->addRow(tr("Open blockers"), m_blockersValue);
        contextLayout->addRow(tr("Dirty state"), m_dirtyValue);
        contentLayout->addWidget(contextGroup);

        auto* continueGroup = new QGroupBox(tr("Continue"), content);
        auto* continueLayout = new QVBoxLayout(continueGroup);

        auto* continueWorkspaceRow = new QWidget(continueGroup);
        auto* continueWorkspaceLayout = new QHBoxLayout(continueWorkspaceRow);
        continueWorkspaceLayout->setContentsMargins(0, 0, 0, 0);
        continueWorkspaceLayout->addWidget(CreateRouteButton(
            continueWorkspaceRow,
            tr("Continue workspace setup"),
            tr("Open the existing Foundation Status pane for workspace and exact game-profile configuration."),
            FoundationStatusPane));
        m_continueWorkspaceHint = new QLabel(continueWorkspaceRow);
        m_continueWorkspaceHint->setWordWrap(true);
        continueWorkspaceLayout->addWidget(m_continueWorkspaceHint, 1);
        continueLayout->addWidget(continueWorkspaceRow);

        auto* continuePackRow = new QWidget(continueGroup);
        auto* continuePackLayout = new QHBoxLayout(continuePackRow);
        continuePackLayout->setContentsMargins(0, 0, 0, 0);
        m_continuePackButton = CreateRouteButton(
            continuePackRow,
            tr("Continue active pack"),
            tr("Open the existing Pack Manager for the active pack or select a pack when none is active."),
            PackManagerPane);
        continuePackLayout->addWidget(m_continuePackButton);
        m_continuePackHint = new QLabel(continuePackRow);
        m_continuePackHint->setWordWrap(true);
        continuePackLayout->addWidget(m_continuePackHint, 1);
        continueLayout->addWidget(continuePackRow);
        contentLayout->addWidget(continueGroup);

        contentLayout->addWidget(CreateRouteGroup(
            content,
            tr("Setup and readiness"),
            tr("Establish exact workspace, game-build, pack, and adapter capability context before downstream work."),
            {
                { tr("Workspace and game profile"), tr("Configure and persist the exact authoring context."), FoundationStatusPane },
                { tr("Pack manager"), tr("Create, select, inspect, and persist governed pack manifests."), PackManagerPane },
                { tr("Adapter capability matrix"), tr("Review fail-closed capability, version, proof, and permission readiness."), AdapterCapabilityPane },
            }));

        contentLayout->addWidget(CreateRouteGroup(
            content,
            tr("Research and author"),
            tr("Move from fingerprinted source material to reviewed canonical records and typed economy authoring."),
            {
                { tr("Source and evidence intake"), tr("Register sources and evidence without inferring facts."), SourceIntakePane },
                { tr("Catalog browser"), tr("Inspect canonical identities, relationships, evidence, and blockers."), CatalogBrowserPane },
                { tr("Catalog governance"), tr("Record reviewed validation, permission, prohibition, and staleness decisions."), CatalogGovernancePane },
                { tr("Item and recipe editor"), tr("Author typed economy profiles against existing canonical identities."), ItemRecipeEditorPane },
                { tr("Actor and troop editor"), tr("Author evidence-bound population profiles and troop composition without runtime spawn authority."), ActorTroopEditorPane },
            }));

        contentLayout->addWidget(CreateRouteGroup(
            content,
            tr("Package and verify"),
            tr("Review deterministic handoffs and externally supplied evidence. These routes never authorise execution."),
            {
                { tr("Work-order plans"), tr("Review generated or refused non-executable adapter plans."), AdapterPlanPane },
                { tr("Runtime result evidence"), tr("Inspect externally supplied adapter results as candidate evidence only."), RuntimeEvidencePane },
                { tr("Build manifests"), tr("Review reproducible definitions with build permission fixed false."), BuildManifestPane },
                { tr("Package assembly preview"), tr("Inspect deterministic package layout without assembling files."), PackagePreviewPane },
                { tr("Staging and deployment preview"), tr("Inspect declared target changes, backups, and rollback steps."), StagingPreviewPane },
                { tr("Deployment work orders"), tr("Review named confirmations and non-executable operator checklists."), DeploymentWorkOrderPane },
                { tr("Deployment result evidence"), tr("Inspect separately supplied execution evidence without promotion."), DeploymentEvidencePane },
            }));

        contentLayout->addWidget(CreateRouteGroup(
            content,
            tr("Diagnostics"),
            tr("Inspect coverage, ambiguity, and post-deployment evidence without clearing blockers automatically."),
            {
                { tr("Economy acquisition coverage"), tr("Review acquisition-path coverage and associated blockers."), EconomyCoveragePane },
                { tr("Cross-pack duplicates"), tr("Review exact duplicate signals without fuzzy identity matching."), EconomyDuplicatesPane },
                { tr("Post-deployment verification"), tr("Review deterministic compatibility and release blockers."), PostDeploymentPane },
                { tr("Independent verifier results"), tr("Inspect supplied verifier observations as adverse-capable evidence."), IndependentVerifierPane },
                { tr("Evidence reconciliation"), tr("Review preserved blockers, dispositions, and human release decisions."), ReconciliationPane },
            }));

        contentLayout->addWidget(CreateRouteGroup(
            content,
            tr("Advanced"),
            tr("Inspect release metadata and result envelopes. Declared checksums, signing intent, and publication targets are not performed actions."),
            {
                { tr("Release artifact provenance"), tr("Review provenance, legal disposition, signing intent, and publication targets."), ReleaseArtifactPane },
                { tr("Release assembly results"), tr("Inspect externally supplied archive and checksum-result evidence."), ReleaseAssemblyPane },
                { tr("Release signing results"), tr("Inspect separately supplied signing-result metadata without loading keys, signing, or verification."), ReleaseSigningPane },
            }));

        contentLayout->addStretch();
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
        const FoundationSnapshot snapshot = FoundationService::Get().GetSnapshot();

        m_workspaceValue->setText(DisplayValue(snapshot.m_workspaceName, tr("Not configured")));
        m_workspaceValue->setToolTip(
            DisplayValue(snapshot.m_workspaceFilePath, tr("No persisted workspace path")));

        QString profile = DisplayValue(snapshot.m_activeGameProfile, tr("Not configured"));
        if (!snapshot.m_gameVersion.empty() || !snapshot.m_branch.empty())
        {
            profile += tr(" \u2014 version %1, branch %2")
                .arg(DisplayValue(snapshot.m_gameVersion, tr("unknown")))
                .arg(DisplayValue(snapshot.m_branch, tr("unknown")));
        }
        m_profileValue->setText(profile);
        m_packValue->setText(DisplayValue(snapshot.m_activePackName, tr("No active pack")));

        if (snapshot.m_openBlockerCount > 0)
        {
            m_validationValue->setText(
                tr("Blocked \u2014 resolve or explicitly review open blockers"));
        }
        else
        {
            m_validationValue->setText(
                tr("No open Foundation blockers; runtime and release permission remain separate"));
        }
        m_blockersValue->setText(tr("%1").arg(snapshot.m_openBlockerCount));

        if (snapshot.m_workspaceFilePath.empty())
        {
            m_dirtyValue->setText(
                tr("Workspace has no persisted file; exact unsaved-change tracking is not exposed"));
            m_continueWorkspaceHint->setText(
                tr("Configure the workspace and exact game profile, then save before downstream work."));
        }
        else
        {
            m_dirtyValue->setText(
                tr("Persisted workspace path is known; exact unsaved-change tracking is not exposed"));
            m_continueWorkspaceHint->setText(
                tr("Resume %1. Review blockers before continuing downstream.")
                    .arg(DisplayValue(snapshot.m_workspaceName, tr("the current workspace"))));
        }

        if (snapshot.m_activePackId.empty())
        {
            m_continuePackButton->setText(tr("Choose a pack"));
            m_continuePackButton->setAccessibleName(tr("Choose a pack"));
            m_continuePackHint->setText(
                tr("No active pack is selected. The Pack Manager explains the required identity and compatibility fields."));
        }
        else
        {
            m_continuePackButton->setText(tr("Continue active pack"));
            m_continuePackButton->setAccessibleName(tr("Continue active pack"));
            m_continuePackHint->setText(
                tr("Resume %1 (%2).")
                    .arg(DisplayValue(snapshot.m_activePackName, tr("the active pack")))
                    .arg(DisplayValue(snapshot.m_activePackVersion, tr("version not set"))));
        }
    }
} // namespace TaintedGrailModdingSDK
