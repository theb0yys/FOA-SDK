/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AdapterBuildManifestWidget.h"

#include "AdapterContractRegistry.h"
#include "FoundationService.h"

#include <AzCore/std/utility/move.h>

#include <QAbstractItemView>
#include <QFont>
#include <QHeaderView>
#include <QLabel>
#include <QObject>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        QString ToQString(const AZStd::string& value)
        {
            return QString::fromUtf8(value.c_str());
        }

        QString JoinValues(const AZStd::vector<AZStd::string>& values)
        {
            QStringList result;
            for (const AZStd::string& value : values)
            {
                result.push_back(ToQString(value));
            }
            return result.join(QStringLiteral(" | "));
        }

        void SetCell(
            QTableWidget* table,
            int row,
            int column,
            const QString& value)
        {
            table->setItem(row, column, new QTableWidgetItem(value));
        }

        const PackManifest* FindPack(
            const AZStd::vector<PackManifest>& packs,
            const AZStd::string& packId)
        {
            for (const PackManifest& pack : packs)
            {
                if (pack.m_packId == packId)
                {
                    return &pack;
                }
            }
            return nullptr;
        }

        AdapterBuildMaterial MakeMaterial(
            AZStd::string id,
            AZStd::string role,
            AZStd::string locator,
            AZStd::string mediaType,
            bool includeInPackage,
            bool redistributable)
        {
            AdapterBuildMaterial material;
            material.m_materialId = AZStd::move(id);
            material.m_role = AZStd::move(role);
            material.m_locator = AZStd::move(locator);
            material.m_mediaType = AZStd::move(mediaType);
            material.m_required = true;
            material.m_includeInPackage = includeInPackage;
            material.m_redistributable = redistributable;
            return material;
        }

        AdapterBuildExpectedOutput MakeOutput(
            const AZStd::string& packageRoot,
            AZStd::string relativeName,
            AZStd::string role,
            AZStd::string mediaType)
        {
            AdapterBuildExpectedOutput output;
            output.m_relativePath = packageRoot + "/" + relativeName;
            output.m_role = AZStd::move(role);
            output.m_mediaType = AZStd::move(mediaType);
            output.m_redistributable = true;
            return output;
        }

        AdapterBuildManifestRequest BuildPreviewRequest(
            const AdapterWorkOrderPlan& plan,
            const PackManifest& pack,
            const GameProfile& profile,
            const AdapterDeclaration& declaration)
        {
            AdapterBuildManifestRequest request;
            request.m_plan = plan;
            request.m_pack = pack;
            request.m_profile = profile;
            request.m_declaration = declaration;
            request.m_environment.m_configuration = pack.m_buildConfiguration;
            request.m_pluginGuid = pack.m_packId;
            request.m_pluginName = pack.m_displayName;
            request.m_pluginVersion = pack.m_version;
            request.m_packageRoot = AZStd::string("BepInEx/plugins/") + pack.m_packId;

            request.m_materials = {
                MakeMaterial(
                    "material.work-order-plan",
                    "work_order_plan",
                    AZStd::string("Reports/WorkOrders/") + plan.m_planId + ".json",
                    "application/json",
                    false,
                    false),
                MakeMaterial(
                    "material.source-tree",
                    "source_tree",
                    "Source",
                    "application/vnd.git-tree",
                    false,
                    false),
                MakeMaterial(
                    "material.dependency-lock",
                    "dependency_lock",
                    "Build/dependencies.lock.json",
                    "application/json",
                    false,
                    true),
                MakeMaterial(
                    "material.toolchain-lock",
                    "toolchain_lock",
                    "Build/toolchain.lock.json",
                    "application/json",
                    false,
                    true),
                MakeMaterial(
                    "material.license",
                    "license",
                    "LICENSE",
                    "text/plain",
                    true,
                    true),
            };

            request.m_expectedOutputs = {
                MakeOutput(
                    request.m_packageRoot,
                    pack.m_packId + ".dll",
                    "plugin_binary",
                    "application/vnd.microsoft.portable-executable"),
                MakeOutput(request.m_packageRoot, "README.md", "readme", "text/markdown"),
                MakeOutput(request.m_packageRoot, "CHANGELOG.md", "changelog", "text/markdown"),
                MakeOutput(request.m_packageRoot, "MANIFEST.md", "manifest", "text/markdown"),
                MakeOutput(request.m_packageRoot, "LICENSE", "license", "text/plain"),
            };
            return request;
        }

        QString EnvironmentSummary(const AdapterBuildEnvironment& environment)
        {
            return QObject::tr("builder %1 @ %2 | %3 | %4 | compiler %5 @ %6")
                .arg(
                    ToQString(environment.m_builderId),
                    ToQString(environment.m_builderVersion),
                    ToQString(environment.m_configuration),
                    ToQString(environment.m_targetFramework),
                    ToQString(environment.m_compilerId),
                    ToQString(environment.m_compilerVersion));
        }

        QString MaterialSummary(const AZStd::vector<AdapterBuildMaterial>& materials)
        {
            QStringList values;
            for (const AdapterBuildMaterial& material : materials)
            {
                values.push_back(
                    QObject::tr("%1=%2 [%3]")
                        .arg(
                            ToQString(material.m_role),
                            ToQString(material.m_locator),
                            ToQString(material.m_fingerprint)));
            }
            return values.join(QStringLiteral(" | "));
        }

        QString OutputSummary(const AZStd::vector<AdapterBuildExpectedOutput>& outputs)
        {
            QStringList values;
            for (const AdapterBuildExpectedOutput& output : outputs)
            {
                values.push_back(
                    QObject::tr("%1=%2")
                        .arg(ToQString(output.m_role), ToQString(output.m_relativePath)));
            }
            return values.join(QStringLiteral(" | "));
        }
    } // namespace

    AdapterBuildManifestWidget::AdapterBuildManifestWidget(QWidget* parent)
        : QWidget(parent)
    {
        auto* layout = new QVBoxLayout(this);
        auto* heading = new QLabel(tr("Tainted Grail Adapter Build Manifests"), this);
        QFont headingFont = heading->font();
        headingFont.setPointSize(headingFont.pointSize() + 3);
        headingFont.setBold(true);
        heading->setFont(headingFont);
        layout->addWidget(heading);

        auto* description = new QLabel(
            tr(
                "Read-only Phase 8 build-definition research. Manifests bind an exact canonical plan, pack, "
                "adapter, profile, toolchain, resolved input fingerprints, BepInEx plugin metadata, dependencies, "
                "and legally redistributable expected outputs. BuildAllowed is always false. Nothing is compiled, "
                "packaged, copied, deployed, launched, or executed by this pane."),
            this);
        description->setWordWrap(true);
        layout->addWidget(description);

        m_summary = new QLabel(this);
        m_summary->setWordWrap(true);
        layout->addWidget(m_summary);

        m_table = new QTableWidget(0, 11, this);
        m_table->setHorizontalHeaderLabels({
            tr("Manifest"),
            tr("Pack"),
            tr("Adapter"),
            tr("Plan"),
            tr("Profile / runtime"),
            tr("Status"),
            tr("Builder / toolchain"),
            tr("Plugin metadata"),
            tr("Resolved materials"),
            tr("Expected package outputs"),
            tr("Canonical JSON / reasons") });
        m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_table->setSelectionMode(QAbstractItemView::SingleSelection);
        m_table->verticalHeader()->setVisible(false);
        m_table->horizontalHeader()->setStretchLastSection(true);
        layout->addWidget(m_table, 1);

        FoundationNotificationBus::Handler::BusConnect();
        Refresh();
    }

    AdapterBuildManifestWidget::~AdapterBuildManifestWidget()
    {
        FoundationNotificationBus::Handler::BusDisconnect();
    }

    void AdapterBuildManifestWidget::OnFoundationChanged()
    {
        Refresh();
    }

    void AdapterBuildManifestWidget::Refresh()
    {
        const FoundationService& foundation = FoundationService::Get();
        const WorkspaceModel& workspace = foundation.GetWorkspace();
        const GameProfile* profile = workspace.FindActiveGameProfile();
        const AdapterWorkOrderPlanSet planSet = FoundationService::Get().GetFrameworkPlanners().BuildPlans(
            workspace,
            foundation.GetPacks(),
            AdapterContractRegistry::Get(),
            foundation.GetSourceRegistry(),
            foundation.GetCatalog(),
            foundation.GetSnapshot().m_blockers);

        AZStd::vector<AdapterBuildManifest> manifests;
        if (profile)
        {
            for (const AdapterWorkOrderPlan& plan : planSet.m_plans)
            {
                const PackManifest* pack = FindPack(foundation.GetPacks(), plan.m_packId);
                const AdapterDeclaration* declaration =
                    AdapterContractRegistry::Get().FindByAdapterId(plan.m_adapterId);
                if (pack && declaration)
                {
                    manifests.push_back(FoundationService::Get().GetFrameworkPlanners().BuildManifest(
                        BuildPreviewRequest(plan, *pack, *profile, *declaration)));
                }
            }
        }

        AZ::u64 readyCount = 0;
        for (const AdapterBuildManifest& manifest : manifests)
        {
            if (manifest.m_status == AdapterBuildManifestStatus::Ready)
            {
                ++readyCount;
            }
        }
        m_summary->setText(
            tr(
                "Generated plans: %1 | plan refusals: %2 | manifest candidates: %3 | ready definitions: %4 | build: prohibited")
                .arg(QString::number(static_cast<qulonglong>(planSet.m_generatedPlanCount)))
                .arg(QString::number(static_cast<qulonglong>(planSet.m_refusedPlanCount)))
                .arg(QString::number(static_cast<qulonglong>(manifests.size())))
                .arg(QString::number(static_cast<qulonglong>(readyCount))));

        const int rowCount = static_cast<int>(manifests.size() + planSet.m_refusals.size());
        m_table->setRowCount(rowCount);
        int row = 0;
        for (const AdapterBuildManifest& manifest : manifests)
        {
            SetCell(m_table, row, 0, ToQString(manifest.m_manifestId));
            SetCell(
                m_table,
                row,
                1,
                tr("%1 @ %2").arg(ToQString(manifest.m_packId), ToQString(manifest.m_packVersion)));
            SetCell(
                m_table,
                row,
                2,
                tr("%1 @ %2").arg(ToQString(manifest.m_adapterId), ToQString(manifest.m_adapterVersion)));
            SetCell(
                m_table,
                row,
                3,
                tr("%1 | %2").arg(ToQString(manifest.m_planId), ToQString(manifest.m_planFingerprint)));
            SetCell(
                m_table,
                row,
                4,
                tr("%1 | %2 | %3 | %4")
                    .arg(
                        ToQString(manifest.m_profileId),
                        ToQString(manifest.m_gameVersion),
                        ToQString(manifest.m_branch),
                        ToQString(manifest.m_runtimeTarget)));
            SetCell(
                m_table,
                row,
                5,
                tr("%1; BuildAllowed=false").arg(ToQString(ToString(manifest.m_status))));
            SetCell(m_table, row, 6, EnvironmentSummary(manifest.m_environment));
            SetCell(
                m_table,
                row,
                7,
                tr("%1 | %2 @ %3 | root %4")
                    .arg(
                        ToQString(manifest.m_pluginGuid),
                        ToQString(manifest.m_pluginName),
                        ToQString(manifest.m_pluginVersion),
                        ToQString(manifest.m_packageRoot)));
            SetCell(m_table, row, 8, MaterialSummary(manifest.m_materials));
            SetCell(m_table, row, 9, OutputSummary(manifest.m_expectedOutputs));
            SetCell(
                m_table,
                row,
                10,
                tr("%1 | %2")
                    .arg(JoinValues(manifest.m_reasons), ToQString(manifest.m_canonicalJson)));
            ++row;
        }

        for (const AdapterWorkOrderRefusal& refusal : planSet.m_refusals)
        {
            SetCell(m_table, row, 0, tr("no manifest"));
            SetCell(m_table, row, 1, ToQString(refusal.m_packId));
            SetCell(
                m_table,
                row,
                2,
                tr("%1 @ %2")
                    .arg(ToQString(refusal.m_adapterId), ToQString(refusal.m_adapterVersion)));
            SetCell(m_table, row, 3, ToQString(refusal.m_planId));
            SetCell(m_table, row, 4, ToQString(refusal.m_runtimeTarget));
            SetCell(m_table, row, 5, tr("plan_refused; build prohibited"));
            SetCell(
                m_table,
                row,
                10,
                tr("failed capabilities: %1 | statuses: %2 | reasons: %3")
                    .arg(
                        JoinValues(refusal.m_failedCapabilities),
                        JoinValues(refusal.m_compatibilityStatuses),
                        JoinValues(refusal.m_reasons)));
            ++row;
        }
    }
} // namespace TaintedGrailModdingSDK
