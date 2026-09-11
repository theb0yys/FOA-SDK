/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once
#include "ModPackagePlan.h"
#include <QByteArray>
#include <QMap>
#include <QStringList>
#include <atomic>
#include <functional>
#include <memory>
namespace TaintedGrailModdingSDK
{
    struct ModPackageContext
    {
        WorkspaceModel m_workspace;
        AZStd::string m_workspaceFile, m_root, m_selectedPack;
        AZStd::vector<PackManifest> m_packs;
        CatalogDatabase m_catalog;
        SourceEvidenceRegistry m_evidence;
    };
    struct ModPackageProgress
    {
        std::shared_ptr<std::atomic_bool> m_cancel;
        std::function<void(int, const QString&)> m_update;
        bool Cancelled() const { return m_cancel && m_cancel->load(); }
    };
    struct ModPackagePreview
    {
        QMap<QString, QByteArray> m_entries;
        QByteArray m_manifest;
        QString m_fingerprint, m_inputFingerprint;
        QStringList m_warnings, m_packLabels;
        QString m_selectedPack;
        qint64 m_totalBytes = 0, m_archiveBytes = 0;
    };
    struct ModPackageReceipt
    {
        QString m_path, m_fingerprint;
        QString m_provider = QStringLiteral("foa.local-authoring-package/1");
        qint64 m_bytes = 0;
    };
    //! Bounded local authoring package provider. V1 adapter previews remain inert.
    class ModPackageService final
    {
    public:
        static constexpr qint64 MaximumDecodedBytes = 64 * 1024 * 1024;
        static constexpr qint64 MaximumArchiveBytes = 96 * 1024 * 1024;
        static constexpr int MaximumEntries = 4096;
        static AZ::Outcome<ModPackagePreview, AZStd::string> Preview(
            const ModPackageContext&, const ModPackageProgress& = {});
        static AZ::Outcome<ModPackageReceipt, AZStd::string> Export(
            const ModPackageContext&, const ModPackagePreview&, const QString& output,
            const ModPackageProgress& = {});
        static AZ::Outcome<ModPackagePreview, AZStd::string> Inspect(
            const QString& archive, const ModPackageProgress& = {});
        static AZ::Outcome<ModPackageReceipt, AZStd::string> Import(
            const QString& archive, const QString& expectedFingerprint, const WorkspaceModel& localContext,
            const QString& destination, const ModPackageProgress& = {});
    };
}
