/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "PackManagerWidget.h"

#include "FoundationService.h"

#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        AZStd::string ToAzString(const QString& value)
        {
            const QByteArray utf8 = value.trimmed().toUtf8();
            return AZStd::string(utf8.constData(), static_cast<size_t>(utf8.size()));
        }

        QString ToQString(const AZStd::string& value)
        {
            return QString::fromUtf8(value.c_str());
        }

        AZStd::vector<AZStd::string> ParseLines(const QPlainTextEdit* editor)
        {
            AZStd::vector<AZStd::string> values;
            const QStringList lines = editor->toPlainText().split('\n');
            for (const QString& line : lines)
            {
                const QString trimmed = line.trimmed();
                if (trimmed.isEmpty())
                {
                    continue;
                }

                const AZStd::string value = ToAzString(trimmed);
                bool duplicate = false;
                for (const AZStd::string& existing : values)
                {
                    if (existing == value)
                    {
                        duplicate = true;
                        break;
                    }
                }
                if (!duplicate)
                {
                    values.push_back(value);
                }
            }
            return values;
        }

        QString JoinLines(const AZStd::vector<AZStd::string>& values)
        {
            QStringList lines;
            for (const AZStd::string& value : values)
            {
                lines.push_back(ToQString(value));
            }
            return lines.join('\n');
        }

        QPlainTextEdit* AddListField(QFormLayout* layout, const QString& label, QWidget* parent)
        {
            auto* editor = new QPlainTextEdit(parent);
            editor->setMaximumHeight(76);
            editor->setPlaceholderText(QObject::tr("One value or relative path per line"));
            layout->addRow(label, editor);
            return editor;
        }
    } // namespace

    PackManagerWidget::PackManagerWidget(QWidget* parent)
        : QWidget(parent)
    {
        auto* rootLayout = new QVBoxLayout(this);

        auto* heading = new QLabel(tr("Tainted Grail Mod and Content-Pack Manager"), this);
        QFont headingFont = heading->font();
        headingFont.setPointSize(headingFont.pointSize() + 3);
        headingFont.setBold(true);
        heading->setFont(headingFont);
        rootLayout->addWidget(heading);

        auto* boundary = new QLabel(
            tr("Creates editor-owned pack manifests. Runtime actions remain disabled and are never written as enabled."),
            this);
        boundary->setWordWrap(true);
        rootLayout->addWidget(boundary);

        auto* summaryGroup = new QGroupBox(tr("Active Pack"), this);
        auto* summaryLayout = new QFormLayout(summaryGroup);
        m_activePackValue = new QLabel(summaryGroup);
        m_manifestPathValue = new QLabel(summaryGroup);
        m_manifestPathValue->setTextInteractionFlags(Qt::TextSelectableByMouse);
        m_manifestPathValue->setWordWrap(true);
        summaryLayout->addRow(tr("Pack"), m_activePackValue);
        summaryLayout->addRow(tr("Manifest"), m_manifestPathValue);
        rootLayout->addWidget(summaryGroup);

        auto* scrollArea = new QScrollArea(this);
        scrollArea->setWidgetResizable(true);
        auto* content = new QWidget(scrollArea);
        auto* contentLayout = new QVBoxLayout(content);

        auto* identityGroup = new QGroupBox(tr("Identity and Ownership"), content);
        auto* identityLayout = new QFormLayout(identityGroup);
        m_packIdEdit = new QLineEdit(identityGroup);
        m_packIdEdit->setPlaceholderText(tr("owner.pack-name"));
        m_displayNameEdit = new QLineEdit(identityGroup);
        m_ownerIdEdit = new QLineEdit(identityGroup);
        m_versionEdit = new QLineEdit(identityGroup);
        m_versionEdit->setPlaceholderText(tr("0.1.0"));
        identityLayout->addRow(tr("Stable pack ID"), m_packIdEdit);
        identityLayout->addRow(tr("Display name"), m_displayNameEdit);
        identityLayout->addRow(tr("Owner ID"), m_ownerIdEdit);
        identityLayout->addRow(tr("Semantic version"), m_versionEdit);
        contentLayout->addWidget(identityGroup);

        auto* compatibilityGroup = new QGroupBox(tr("Compatibility"), content);
        auto* compatibilityLayout = new QFormLayout(compatibilityGroup);
        m_targetGameVersionEdit = new QLineEdit(compatibilityGroup);
        m_targetBranchEdit = new QLineEdit(compatibilityGroup);
        m_compatibleGameVersionsEdit = AddListField(compatibilityLayout, tr("Additional game versions"), compatibilityGroup);
        m_coreVersionEdit = new QLineEdit(compatibilityGroup);
        m_adapterVersionEdit = new QLineEdit(compatibilityGroup);
        m_dlcScopesEdit = AddListField(compatibilityLayout, tr("DLC / content scopes"), compatibilityGroup);
        compatibilityLayout->insertRow(0, tr("Primary game version"), m_targetGameVersionEdit);
        compatibilityLayout->insertRow(1, tr("Target branch"), m_targetBranchEdit);
        compatibilityLayout->insertRow(3, tr("Required Avalon Core"), m_coreVersionEdit);
        compatibilityLayout->insertRow(4, tr("Required FoA adapter"), m_adapterVersionEdit);
        contentLayout->addWidget(compatibilityGroup);

        auto* relationshipsGroup = new QGroupBox(tr("Dependencies and Compatibility Relationships"), content);
        auto* relationshipsLayout = new QFormLayout(relationshipsGroup);
        m_dependenciesEdit = AddListField(relationshipsLayout, tr("Pack dependencies"), relationshipsGroup);
        m_requiredModsEdit = AddListField(relationshipsLayout, tr("Required mods"), relationshipsGroup);
        m_incompatibilitiesEdit = AddListField(relationshipsLayout, tr("Incompatibilities"), relationshipsGroup);
        contentLayout->addWidget(relationshipsGroup);

        auto* contentGroup = new QGroupBox(tr("Content and Resources"), content);
        auto* contentForm = new QFormLayout(contentGroup);
        m_saveImpactCombo = new QComboBox(contentGroup);
        m_saveImpactCombo->addItems({ "unknown", "none", "compatible", "migration", "destructive" });
        m_contentDefinitionsEdit = AddListField(contentForm, tr("Content definitions"), contentGroup);
        m_assetPathsEdit = AddListField(contentForm, tr("Asset declarations"), contentGroup);
        m_localisationPathsEdit = AddListField(contentForm, tr("Localisation declarations"), contentGroup);
        contentForm->insertRow(0, tr("Save impact"), m_saveImpactCombo);
        contentLayout->addWidget(contentGroup);

        auto* releaseGroup = new QGroupBox(tr("Build and Release"), content);
        auto* releaseLayout = new QFormLayout(releaseGroup);
        m_buildConfigurationEdit = new QLineEdit(releaseGroup);
        m_buildConfigurationEdit->setPlaceholderText(tr("Profile"));
        m_releaseChannelCombo = new QComboBox(releaseGroup);
        m_releaseChannelCombo->addItems({ "development", "alpha", "beta", "release" });
        releaseLayout->addRow(tr("Build configuration"), m_buildConfigurationEdit);
        releaseLayout->addRow(tr("Release channel"), m_releaseChannelCombo);
        contentLayout->addWidget(releaseGroup);
        contentLayout->addStretch(1);

        scrollArea->setWidget(content);
        rootLayout->addWidget(scrollArea, 1);

        auto* buttonLayout = new QHBoxLayout();
        auto* newButton = new QPushButton(tr("New Pack"), this);
        auto* applyButton = new QPushButton(tr("Apply"), this);
        auto* openButton = new QPushButton(tr("Open…"), this);
        auto* saveButton = new QPushButton(tr("Save"), this);
        auto* saveAsButton = new QPushButton(tr("Save As…"), this);
        buttonLayout->addWidget(newButton);
        buttonLayout->addWidget(applyButton);
        buttonLayout->addStretch(1);
        buttonLayout->addWidget(openButton);
        buttonLayout->addWidget(saveButton);
        buttonLayout->addWidget(saveAsButton);
        rootLayout->addLayout(buttonLayout);

        m_statusLabel = new QLabel(this);
        m_statusLabel->setWordWrap(true);
        rootLayout->addWidget(m_statusLabel);

        connect(newButton, &QPushButton::clicked, this, [this]()
        {
            FoundationService::Get().ClearActivePack();
            ClearFormForNewPack();
            SetStatus(tr("New pack draft created. Apply it before saving."));
        });
        connect(applyButton, &QPushButton::clicked, this, [this]()
        {
            ApplyPack();
        });
        connect(openButton, &QPushButton::clicked, this, [this]()
        {
            const WorkspaceModel& workspace = FoundationService::Get().GetWorkspace();
            const QString startDirectory = workspace.m_rootPath.empty()
                ? QDir::homePath()
                : ToQString(workspace.m_rootPath);
            const QString filePath = QFileDialog::getOpenFileName(
                this,
                tr("Open TG Pack Manifest"),
                startDirectory,
                tr("TG Pack Manifest (*.tgpack.json);;JSON Files (*.json)"));
            if (filePath.isEmpty())
            {
                return;
            }

            AZStd::string error;
            if (!FoundationService::Get().LoadPack(ToAzString(filePath), &error))
            {
                SetStatus(ToQString(error), true);
                return;
            }
            if (const PackManifest* pack = FoundationService::Get().GetActivePack())
            {
                PopulateFromPack(*pack);
            }
            SetStatus(tr("Pack manifest opened."));
        });
        connect(saveButton, &QPushButton::clicked, this, [this]()
        {
            if (!ApplyPack())
            {
                return;
            }
            if (FoundationService::Get().GetActivePackFilePath().empty())
            {
                SavePackAs();
                return;
            }

            AZStd::string error;
            if (!FoundationService::Get().SaveActivePack(&error))
            {
                SetStatus(ToQString(error), true);
                return;
            }
            SetStatus(tr("Pack manifest saved."));
        });
        connect(saveAsButton, &QPushButton::clicked, this, [this]()
        {
            SavePackAs();
        });

        FoundationNotificationBus::Handler::BusConnect();
        ClearFormForNewPack();
        if (const PackManifest* pack = FoundationService::Get().GetActivePack())
        {
            PopulateFromPack(*pack);
        }
        UpdateSummary();
    }

    PackManagerWidget::~PackManagerWidget()
    {
        FoundationNotificationBus::Handler::BusDisconnect();
    }

    void PackManagerWidget::OnFoundationChanged()
    {
        UpdateSummary();
    }

    PackManifest PackManagerWidget::BuildPackFromForm() const
    {
        PackManifest pack;
        pack.m_packId = ToAzString(m_packIdEdit->text());
        pack.m_displayName = ToAzString(m_displayNameEdit->text());
        pack.m_ownerId = ToAzString(m_ownerIdEdit->text());
        pack.m_version = ToAzString(m_versionEdit->text());
        pack.m_targetGameVersion = ToAzString(m_targetGameVersionEdit->text());
        pack.m_targetBranch = ToAzString(m_targetBranchEdit->text());
        pack.m_compatibleGameVersions = ParseLines(m_compatibleGameVersionsEdit);
        pack.m_requiredCoreVersion = ToAzString(m_coreVersionEdit->text());
        pack.m_requiredAdapterVersion = ToAzString(m_adapterVersionEdit->text());
        pack.m_saveImpact = ToAzString(m_saveImpactCombo->currentText());
        pack.m_dlcScopes = ParseLines(m_dlcScopesEdit);
        pack.m_dependencies = ParseLines(m_dependenciesEdit);
        pack.m_requiredMods = ParseLines(m_requiredModsEdit);
        pack.m_incompatibilities = ParseLines(m_incompatibilitiesEdit);
        pack.m_contentDefinitionPaths = ParseLines(m_contentDefinitionsEdit);
        pack.m_assetPaths = ParseLines(m_assetPathsEdit);
        pack.m_localisationPaths = ParseLines(m_localisationPathsEdit);
        pack.m_buildConfiguration = ToAzString(m_buildConfigurationEdit->text());
        pack.m_releaseChannel = ToAzString(m_releaseChannelCombo->currentText());
        pack.m_runtimeActionsEnabled = false;
        return pack;
    }

    void PackManagerWidget::PopulateFromPack(const PackManifest& pack)
    {
        m_packIdEdit->setText(ToQString(pack.m_packId));
        m_displayNameEdit->setText(ToQString(pack.m_displayName));
        m_ownerIdEdit->setText(ToQString(pack.m_ownerId));
        m_versionEdit->setText(ToQString(pack.m_version));
        m_targetGameVersionEdit->setText(ToQString(pack.m_targetGameVersion));
        m_targetBranchEdit->setText(ToQString(pack.m_targetBranch));
        m_compatibleGameVersionsEdit->setPlainText(JoinLines(pack.m_compatibleGameVersions));
        m_coreVersionEdit->setText(ToQString(pack.m_requiredCoreVersion));
        m_adapterVersionEdit->setText(ToQString(pack.m_requiredAdapterVersion));
        m_dlcScopesEdit->setPlainText(JoinLines(pack.m_dlcScopes));
        m_dependenciesEdit->setPlainText(JoinLines(pack.m_dependencies));
        m_requiredModsEdit->setPlainText(JoinLines(pack.m_requiredMods));
        m_incompatibilitiesEdit->setPlainText(JoinLines(pack.m_incompatibilities));
        m_saveImpactCombo->setCurrentText(ToQString(pack.m_saveImpact));
        m_contentDefinitionsEdit->setPlainText(JoinLines(pack.m_contentDefinitionPaths));
        m_assetPathsEdit->setPlainText(JoinLines(pack.m_assetPaths));
        m_localisationPathsEdit->setPlainText(JoinLines(pack.m_localisationPaths));
        m_buildConfigurationEdit->setText(ToQString(pack.m_buildConfiguration));
        m_releaseChannelCombo->setCurrentText(ToQString(pack.m_releaseChannel));
    }

    void PackManagerWidget::ClearFormForNewPack()
    {
        m_packIdEdit->clear();
        m_displayNameEdit->clear();
        m_ownerIdEdit->clear();
        m_versionEdit->setText(QStringLiteral("0.1.0"));
        m_compatibleGameVersionsEdit->clear();
        m_coreVersionEdit->clear();
        m_adapterVersionEdit->clear();
        m_dlcScopesEdit->clear();
        m_dependenciesEdit->clear();
        m_requiredModsEdit->clear();
        m_incompatibilitiesEdit->clear();
        m_saveImpactCombo->setCurrentText(QStringLiteral("unknown"));
        m_contentDefinitionsEdit->clear();
        m_assetPathsEdit->clear();
        m_localisationPathsEdit->clear();
        m_buildConfigurationEdit->setText(QStringLiteral("Profile"));
        m_releaseChannelCombo->setCurrentText(QStringLiteral("development"));

        if (const GameProfile* profile = FoundationService::Get().GetWorkspace().FindActiveGameProfile())
        {
            m_targetGameVersionEdit->setText(ToQString(profile->m_gameVersion));
            m_targetBranchEdit->setText(ToQString(profile->m_branch));
            m_dlcScopesEdit->setPlainText(JoinLines(profile->m_dlcScopes));
        }
        else
        {
            m_targetGameVersionEdit->clear();
            m_targetBranchEdit->clear();
        }
    }

    void PackManagerWidget::UpdateSummary()
    {
        const FoundationSnapshot& snapshot = FoundationService::Get().GetSnapshot();
        m_activePackValue->setText(
            tr("%1 — %2 (%3)")
                .arg(ToQString(snapshot.m_activePackId), ToQString(snapshot.m_activePackName), ToQString(snapshot.m_activePackVersion)));
        m_manifestPathValue->setText(ToQString(snapshot.m_activePackFilePath));
    }

    void PackManagerWidget::SetStatus(const QString& message, bool error)
    {
        m_statusLabel->setText(message);
        m_statusLabel->setStyleSheet(error ? QStringLiteral("color: #d9534f;") : QString());
    }

    bool PackManagerWidget::ApplyPack()
    {
        AZStd::string error;
        if (!FoundationService::Get().SetActivePack(BuildPackFromForm(), &error))
        {
            SetStatus(ToQString(error), true);
            return false;
        }
        SetStatus(tr("Pack applied. Review the Foundation Status window for compatibility blockers."));
        return true;
    }

    bool PackManagerWidget::SavePackAs()
    {
        if (!ApplyPack())
        {
            return false;
        }

        const WorkspaceModel& workspace = FoundationService::Get().GetWorkspace();
        if (workspace.m_rootPath.empty())
        {
            SetStatus(tr("Configure and apply a workspace root before saving pack manifests."), true);
            return false;
        }

        const PackManifest* pack = FoundationService::Get().GetActivePack();
        if (!pack)
        {
            SetStatus(tr("No active pack is available to save."), true);
            return false;
        }

        const QString packDirectory = QDir(ToQString(workspace.m_rootPath)).filePath(
            QStringLiteral("Packs/%1").arg(ToQString(pack->m_packId)));
        QDir().mkpath(packDirectory);
        QString filePath = QFileDialog::getSaveFileName(
            this,
            tr("Save TG Pack Manifest"),
            QDir(packDirectory).filePath(QStringLiteral("pack.tgpack.json")),
            tr("TG Pack Manifest (*.tgpack.json)"));
        if (filePath.isEmpty())
        {
            return false;
        }
        if (!filePath.endsWith(QStringLiteral(".tgpack.json"), Qt::CaseInsensitive))
        {
            filePath += QStringLiteral(".tgpack.json");
        }
        if (!IsInsideWorkspace(filePath))
        {
            SetStatus(tr("Pack manifests must be stored inside the active workspace root."), true);
            return false;
        }

        QDir().mkpath(QFileInfo(filePath).absolutePath());
        AZStd::string error;
        if (!FoundationService::Get().SaveActivePack(ToAzString(filePath), &error))
        {
            SetStatus(ToQString(error), true);
            return false;
        }
        SetStatus(tr("Pack manifest saved inside the active workspace."));
        return true;
    }

    bool PackManagerWidget::IsInsideWorkspace(const QString& filePath) const
    {
        const QString workspaceRoot = QDir::cleanPath(ToQString(FoundationService::Get().GetWorkspace().m_rootPath));
        const QString absoluteFilePath = QDir::cleanPath(QFileInfo(filePath).absoluteFilePath());
        if (workspaceRoot.isEmpty())
        {
            return false;
        }

#ifdef Q_OS_WIN
        const Qt::CaseSensitivity sensitivity = Qt::CaseInsensitive;
#else
        const Qt::CaseSensitivity sensitivity = Qt::CaseSensitive;
#endif
        return absoluteFilePath.compare(workspaceRoot, sensitivity) == 0
            || absoluteFilePath.startsWith(workspaceRoot + QDir::separator(), sensitivity);
    }
} // namespace TaintedGrailModdingSDK
