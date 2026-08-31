/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "PackManagerWidget.h"

#include "FoundationService.h"

#include <AzCore/std/algorithm.h>

#include <QByteArray>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileInfoList>
#include <QFont>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QStringList>
#include <QVBoxLayout>

#include <cstddef>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        constexpr int MaximumWorkspaceModDirectories = 512;
        constexpr qint64 MaximumManifestSummaryBytes = 1024 * 1024;

        AZStd::string ToAzString(const QString& value)
        {
            const QByteArray utf8 = value.trimmed().toUtf8();
            return AZStd::string(utf8.constData(), static_cast<size_t>(utf8.size()));
        }

        QString ToQString(const AZStd::string& value)
        {
            return QString::fromUtf8(value.c_str());
        }

        QString StableToken(const QString& value)
        {
            const QString lowered = value.trimmed().toLower();
            QString token;
            token.reserve(lowered.size());
            bool separatorPending = false;
            for (const QChar character : lowered)
            {
                const ushort code = character.unicode();
                const bool alphaNumeric =
                    (code >= 'a' && code <= 'z') || (code >= '0' && code <= '9');
                if (alphaNumeric)
                {
                    if (separatorPending && !token.isEmpty())
                    {
                        token += '-';
                    }
                    token += character;
                    separatorPending = false;
                }
                else if (!token.isEmpty())
                {
                    separatorPending = true;
                }
                if (token.size() >= 64)
                {
                    break;
                }
            }
            while (token.endsWith('-'))
            {
                token.chop(1);
            }
            return token;
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
                if (AZStd::find(values.begin(), values.end(), value) == values.end())
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

        QString WorkspaceModLabel(const QFileInfo& manifestInfo)
        {
            const QString fallback = manifestInfo.dir().dirName();
            QFile file(manifestInfo.absoluteFilePath());
            if (!file.open(QIODevice::ReadOnly))
            {
                return QObject::tr("%1 · needs repair").arg(fallback);
            }

            QJsonParseError parseError;
            const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
            if (parseError.error != QJsonParseError::NoError || !document.isObject())
            {
                return QObject::tr("%1 · needs repair").arg(fallback);
            }

            const QJsonObject root = document.object();
            const QString packId = root.value(QStringLiteral("PackId")).toString().trimmed();
            const QString displayName = root.value(QStringLiteral("DisplayName")).toString().trimmed();
            const QString version = root.value(QStringLiteral("Version")).toString().trimmed();

            QString label = displayName.isEmpty()
                ? (packId.isEmpty() ? fallback : packId)
                : displayName;
            if (!version.isEmpty())
            {
                label += QObject::tr(" · %1").arg(version);
            }
            return label;
        }
    } // namespace

    PackManagerWidget::PackManagerWidget(QWidget* parent)
        : QWidget(parent)
    {
        auto* rootLayout = new QVBoxLayout(this);

        auto* heading = new QLabel(tr("FOA-SDK Mods"), this);
        QFont headingFont = heading->font();
        headingFont.setPointSize(headingFont.pointSize() + 3);
        headingFont.setBold(true);
        heading->setFont(headingFont);
        rootLayout->addWidget(heading);

        auto* description = new QLabel(
            tr("Create or select a mod. FOA-SDK fills the current game compatibility and stores each manifest in the workspace automatically."),
            this);
        description->setWordWrap(true);
        rootLayout->addWidget(description);

        auto* summaryGroup = new QGroupBox(tr("Current mod"), this);
        auto* summaryLayout = new QFormLayout(summaryGroup);
        m_activePackValue = new QLabel(summaryGroup);
        summaryLayout->addRow(tr("Mod"), m_activePackValue);
        rootLayout->addWidget(summaryGroup);

        auto* workspaceModsGroup = new QGroupBox(tr("Saved mods"), this);
        auto* workspaceModsLayout = new QVBoxLayout(workspaceModsGroup);
        auto* workspaceModsRow = new QWidget(workspaceModsGroup);
        auto* workspaceModsRowLayout = new QHBoxLayout(workspaceModsRow);
        workspaceModsRowLayout->setContentsMargins(0, 0, 0, 0);
        m_workspaceModsCombo = new QComboBox(workspaceModsRow);
        m_workspaceModsCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        m_workspaceModsCombo->setMinimumContentsLength(28);
        m_openSelectedButton = new QPushButton(tr("Open selected"), workspaceModsRow);
        workspaceModsRowLayout->addWidget(m_workspaceModsCombo, 1);
        workspaceModsRowLayout->addWidget(m_openSelectedButton);
        workspaceModsLayout->addWidget(workspaceModsRow);
        m_workspaceModsHint = new QLabel(workspaceModsGroup);
        m_workspaceModsHint->setWordWrap(true);
        workspaceModsLayout->addWidget(m_workspaceModsHint);
        rootLayout->addWidget(workspaceModsGroup);

        auto* detailsGroup = new QGroupBox(tr("Mod details"), this);
        auto* detailsLayout = new QFormLayout(detailsGroup);
        m_displayNameEdit = new QLineEdit(detailsGroup);
        m_displayNameEdit->setPlaceholderText(tr("My Fall of Avalon mod"));
        m_ownerIdEdit = new QLineEdit(detailsGroup);
        m_ownerIdEdit->setPlaceholderText(tr("author or namespace"));
        detailsLayout->addRow(tr("Mod name"), m_displayNameEdit);
        detailsLayout->addRow(tr("Author / namespace"), m_ownerIdEdit);
        rootLayout->addWidget(detailsGroup);

        m_advancedToggleButton = new QPushButton(tr("Show advanced manifest"), this);
        rootLayout->addWidget(m_advancedToggleButton, 0, Qt::AlignRight);

        m_advancedGroup = new QGroupBox(tr("Advanced manifest"), this);
        auto* advancedOuterLayout = new QVBoxLayout(m_advancedGroup);
        auto* advancedDescription = new QLabel(
            tr("These fields are for deliberate compatibility, dependency, packaging, or release overrides. New mods receive safe defaults from the active FOA-SDK profile."),
            m_advancedGroup);
        advancedDescription->setWordWrap(true);
        advancedOuterLayout->addWidget(advancedDescription);

        auto* scrollArea = new QScrollArea(m_advancedGroup);
        scrollArea->setWidgetResizable(true);
        auto* content = new QWidget(scrollArea);
        auto* contentLayout = new QVBoxLayout(content);

        auto* identityGroup = new QGroupBox(tr("Identity"), content);
        auto* identityLayout = new QFormLayout(identityGroup);
        m_packIdEdit = new QLineEdit(identityGroup);
        m_packIdEdit->setReadOnly(true);
        m_versionEdit = new QLineEdit(identityGroup);
        m_versionEdit->setPlaceholderText(tr("0.1.0"));
        identityLayout->addRow(tr("Mod ID"), m_packIdEdit);
        identityLayout->addRow(tr("Version"), m_versionEdit);
        contentLayout->addWidget(identityGroup);

        auto* compatibilityGroup = new QGroupBox(tr("Compatibility"), content);
        auto* compatibilityLayout = new QFormLayout(compatibilityGroup);
        m_targetGameVersionEdit = new QLineEdit(compatibilityGroup);
        m_targetBranchEdit = new QLineEdit(compatibilityGroup);
        m_compatibleGameVersionsEdit = AddListField(
            compatibilityLayout,
            tr("Additional game versions"),
            compatibilityGroup);
        m_coreVersionEdit = new QLineEdit(compatibilityGroup);
        m_adapterVersionEdit = new QLineEdit(compatibilityGroup);
        m_dlcScopesEdit = AddListField(
            compatibilityLayout,
            tr("DLC / content scopes"),
            compatibilityGroup);
        compatibilityLayout->insertRow(0, tr("Primary game version"), m_targetGameVersionEdit);
        compatibilityLayout->insertRow(1, tr("Target branch"), m_targetBranchEdit);
        compatibilityLayout->insertRow(3, tr("Required core/framework"), m_coreVersionEdit);
        compatibilityLayout->insertRow(4, tr("Required FoA adapter"), m_adapterVersionEdit);
        contentLayout->addWidget(compatibilityGroup);

        auto* relationshipsGroup = new QGroupBox(tr("Dependencies"), content);
        auto* relationshipsLayout = new QFormLayout(relationshipsGroup);
        m_dependenciesEdit = AddListField(relationshipsLayout, tr("Pack dependencies"), relationshipsGroup);
        m_requiredModsEdit = AddListField(relationshipsLayout, tr("Required mods"), relationshipsGroup);
        m_incompatibilitiesEdit = AddListField(relationshipsLayout, tr("Incompatibilities"), relationshipsGroup);
        contentLayout->addWidget(relationshipsGroup);

        auto* contentGroup = new QGroupBox(tr("Content declarations"), content);
        auto* contentForm = new QFormLayout(contentGroup);
        m_saveImpactCombo = new QComboBox(contentGroup);
        m_saveImpactCombo->addItems({ "unknown", "none", "compatible", "migration", "destructive" });
        m_contentDefinitionsEdit = AddListField(contentForm, tr("Content definitions"), contentGroup);
        m_assetPathsEdit = AddListField(contentForm, tr("Assets"), contentGroup);
        m_localisationPathsEdit = AddListField(contentForm, tr("Localisation"), contentGroup);
        contentForm->insertRow(0, tr("Save impact"), m_saveImpactCombo);
        contentLayout->addWidget(contentGroup);

        auto* releaseGroup = new QGroupBox(tr("Build and release"), content);
        auto* releaseLayout = new QFormLayout(releaseGroup);
        m_buildConfigurationEdit = new QLineEdit(releaseGroup);
        m_buildConfigurationEdit->setPlaceholderText(tr("Profile"));
        m_releaseChannelCombo = new QComboBox(releaseGroup);
        m_releaseChannelCombo->addItems({ "development", "alpha", "beta", "release" });
        releaseLayout->addRow(tr("Build configuration"), m_buildConfigurationEdit);
        releaseLayout->addRow(tr("Release channel"), m_releaseChannelCombo);
        contentLayout->addWidget(releaseGroup);

        auto* locationGroup = new QGroupBox(tr("Storage"), content);
        auto* locationLayout = new QFormLayout(locationGroup);
        m_manifestPathValue = new QLabel(locationGroup);
        m_manifestPathValue->setTextInteractionFlags(Qt::TextSelectableByMouse);
        m_manifestPathValue->setWordWrap(true);
        locationLayout->addRow(tr("Manifest"), m_manifestPathValue);
        contentLayout->addWidget(locationGroup);
        contentLayout->addStretch(1);

        scrollArea->setWidget(content);
        advancedOuterLayout->addWidget(scrollArea, 1);
        m_advancedGroup->setVisible(false);
        rootLayout->addWidget(m_advancedGroup, 1);

        auto* buttonLayout = new QHBoxLayout();
        auto* newButton = new QPushButton(tr("New mod"), this);
        auto* saveButton = new QPushButton(tr("Save mod"), this);
        buttonLayout->addWidget(newButton);
        buttonLayout->addStretch(1);
        buttonLayout->addWidget(saveButton);
        rootLayout->addLayout(buttonLayout);

        m_statusLabel = new QLabel(this);
        m_statusLabel->setWordWrap(true);
        rootLayout->addWidget(m_statusLabel);

        connect(m_displayNameEdit, &QLineEdit::textChanged, this, [this]()
        {
            UpdateGeneratedIdentity();
        });
        connect(m_ownerIdEdit, &QLineEdit::textChanged, this, [this]()
        {
            UpdateGeneratedIdentity();
        });
        connect(m_advancedToggleButton, &QPushButton::clicked, this, [this]()
        {
            const bool show = !m_advancedGroup->isVisible();
            m_advancedGroup->setVisible(show);
            m_advancedToggleButton->setText(
                show ? tr("Hide advanced manifest") : tr("Show advanced manifest"));
        });
        connect(m_openSelectedButton, &QPushButton::clicked, this, [this]()
        {
            OpenSelectedPack();
        });
        connect(newButton, &QPushButton::clicked, this, [this]()
        {
            FoundationService::Get().ClearActivePack();
            ClearFormForNewPack();
            SetStatus(tr("New mod ready. Enter a name and author, then Save mod."));
        });
        connect(saveButton, &QPushButton::clicked, this, [this]()
        {
            SavePack();
        });

        FoundationNotificationBus::Handler::BusConnect();
        ClearFormForNewPack();
        if (const PackManifest* pack = FoundationService::Get().GetActivePack())
        {
            PopulateFromPack(*pack);
        }
        UpdateSummary();
        RefreshWorkspaceMods(ToQString(FoundationService::Get().GetActivePackFilePath()));
    }

    PackManagerWidget::~PackManagerWidget()
    {
        FoundationNotificationBus::Handler::BusDisconnect();
    }

    void PackManagerWidget::OnFoundationChanged()
    {
        UpdateSummary();
        RefreshWorkspaceMods(ToQString(FoundationService::Get().GetActivePackFilePath()));
    }

    PackManifest PackManagerWidget::BuildPackFromForm() const
    {
        PackManifest pack;
        pack.m_packId = ToAzString(m_packIdEdit->text());
        pack.m_displayName = ToAzString(m_displayNameEdit->text());
        pack.m_ownerId = ToAzString(StableToken(m_ownerIdEdit->text()));
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
        m_isNewPack = false;
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
        UpdateSummary();
    }

    void PackManagerWidget::ClearFormForNewPack()
    {
        m_isNewPack = true;
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
        UpdateGeneratedIdentity();
        UpdateSummary();
    }

    void PackManagerWidget::UpdateGeneratedIdentity()
    {
        if (!m_isNewPack)
        {
            return;
        }

        const QString owner = StableToken(m_ownerIdEdit->text());
        const QString name = StableToken(m_displayNameEdit->text());
        m_packIdEdit->setText(
            owner.isEmpty() || name.isEmpty()
                ? QString()
                : owner + QStringLiteral(".") + name);
    }

    void PackManagerWidget::UpdateSummary()
    {
        const FoundationSnapshot& snapshot = FoundationService::Get().GetSnapshot();
        if (snapshot.m_activePackId.empty())
        {
            m_activePackValue->setText(tr("None selected"));
            m_manifestPathValue->setText(tr("Not saved"));
            return;
        }

        QString text = snapshot.m_activePackName.empty()
            ? ToQString(snapshot.m_activePackId)
            : ToQString(snapshot.m_activePackName);
        if (!snapshot.m_activePackVersion.empty())
        {
            text += tr(" · %1").arg(ToQString(snapshot.m_activePackVersion));
        }
        m_activePackValue->setText(text);
        m_manifestPathValue->setText(
            snapshot.m_activePackFilePath.empty()
                ? tr("Not saved")
                : ToQString(snapshot.m_activePackFilePath));
    }

    void PackManagerWidget::RefreshWorkspaceMods(const QString& selectedPath)
    {
        QString selection = selectedPath.trimmed();
        if (selection.isEmpty() && m_workspaceModsCombo->currentIndex() >= 0)
        {
            selection = m_workspaceModsCombo->currentData().toString();
        }
        if (!selection.isEmpty())
        {
            selection = QDir::cleanPath(QFileInfo(selection).absoluteFilePath());
        }

        m_workspaceModsCombo->clear();
        int availableCount = 0;
        int ignoredCount = 0;
        bool truncated = false;

        const QString workspaceRoot = ToQString(FoundationService::Get().GetWorkspace().m_rootPath);
        if (!workspaceRoot.isEmpty())
        {
            QDir packsDirectory(QDir(workspaceRoot).filePath(QStringLiteral("Packs")));
            const QFileInfoList directories = packsDirectory.entryInfoList(
                QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks,
                QDir::Name | QDir::IgnoreCase);
            const int limit = directories.size() < MaximumWorkspaceModDirectories
                ? directories.size()
                : MaximumWorkspaceModDirectories;
            truncated = directories.size() > MaximumWorkspaceModDirectories;

            for (int index = 0; index < limit; ++index)
            {
                const QFileInfo manifestInfo(
                    QDir(directories[index].absoluteFilePath()).filePath(
                        QStringLiteral("pack.tgpack.json")));
                if (manifestInfo.isSymLink()
                    || !manifestInfo.isFile()
                    || manifestInfo.size() <= 0
                    || manifestInfo.size() > MaximumManifestSummaryBytes)
                {
                    ++ignoredCount;
                    continue;
                }

                const QString filePath = QDir::cleanPath(manifestInfo.absoluteFilePath());
                m_workspaceModsCombo->addItem(WorkspaceModLabel(manifestInfo), filePath);
                ++availableCount;
            }
        }

        if (availableCount == 0)
        {
            m_workspaceModsCombo->addItem(tr("No saved mods found"), QString());
            m_workspaceModsCombo->setEnabled(false);
            m_openSelectedButton->setEnabled(false);
            m_workspaceModsHint->setText(
                tr("Create a new mod below. It will appear here automatically after the first save."));
            return;
        }

        m_workspaceModsCombo->setEnabled(true);
        m_openSelectedButton->setEnabled(true);
        const int selectedIndex = selection.isEmpty()
            ? -1
            : m_workspaceModsCombo->findData(selection);
        m_workspaceModsCombo->setCurrentIndex(selectedIndex >= 0 ? selectedIndex : 0);

        QString hint = availableCount == 1
            ? tr("1 saved mod is available in this workspace.")
            : tr("%1 saved mods are available in this workspace.").arg(availableCount);
        if (ignoredCount > 0)
        {
            hint += tr(" %1 unsafe or unsupported manifest(s) were ignored.").arg(ignoredCount);
        }
        if (truncated)
        {
            hint += tr(" The list is limited to the first %1 mod folders.")
                .arg(MaximumWorkspaceModDirectories);
        }
        m_workspaceModsHint->setText(hint);
    }

    void PackManagerWidget::OpenSelectedPack()
    {
        const QString filePath = m_workspaceModsCombo->currentData().toString();
        if (filePath.isEmpty())
        {
            SetStatus(tr("No saved mod is selected."), true);
            return;
        }
        if (!IsInsideWorkspace(filePath))
        {
            SetStatus(tr("The selected mod is outside the FOA-SDK workspace."), true);
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
        RefreshWorkspaceMods(filePath);
        SetStatus(tr("Mod opened."));
    }

    void PackManagerWidget::SetStatus(const QString& message, bool error)
    {
        m_statusLabel->setText(message);
        m_statusLabel->setStyleSheet(error ? QStringLiteral("color: #d9534f;") : QString());
    }

    bool PackManagerWidget::ApplyPack()
    {
        UpdateGeneratedIdentity();
        if (m_displayNameEdit->text().trimmed().isEmpty())
        {
            SetStatus(tr("Enter a mod name."), true);
            return false;
        }
        if (StableToken(m_ownerIdEdit->text()).isEmpty())
        {
            SetStatus(tr("Enter an author or namespace."), true);
            return false;
        }

        AZStd::string error;
        if (!FoundationService::Get().SetActivePack(BuildPackFromForm(), &error))
        {
            SetStatus(ToQString(error), true);
            return false;
        }
        m_isNewPack = false;
        return true;
    }

    QString PackManagerWidget::CanonicalPackFilePath(const PackManifest& pack) const
    {
        const WorkspaceModel& workspace = FoundationService::Get().GetWorkspace();
        if (workspace.m_rootPath.empty() || pack.m_packId.empty())
        {
            return {};
        }
        return QDir(ToQString(workspace.m_rootPath)).filePath(
            QStringLiteral("Packs/%1/pack.tgpack.json").arg(ToQString(pack.m_packId)));
    }

    bool PackManagerWidget::SavePack()
    {
        if (!ApplyPack())
        {
            return false;
        }

        FoundationService& service = FoundationService::Get();
        const PackManifest* pack = service.GetActivePack();
        if (!pack)
        {
            SetStatus(tr("No mod is available to save."), true);
            return false;
        }

        QString filePath = service.GetActivePackFilePath().empty()
            ? CanonicalPackFilePath(*pack)
            : ToQString(service.GetActivePackFilePath());
        if (filePath.isEmpty())
        {
            SetStatus(tr("FOA-SDK could not resolve the mod location inside the workspace."), true);
            return false;
        }
        if (!IsInsideWorkspace(filePath))
        {
            SetStatus(tr("The mod manifest must stay inside the FOA-SDK workspace."), true);
            return false;
        }
        if (!QDir().mkpath(QFileInfo(filePath).absolutePath()))
        {
            SetStatus(tr("FOA-SDK could not create the mod folder."), true);
            return false;
        }

        AZStd::string error;
        if (!service.SaveActivePack(ToAzString(filePath), &error))
        {
            SetStatus(ToQString(error), true);
            return false;
        }
        SetStatus(tr("Mod saved. You can start authoring."));
        UpdateSummary();
        RefreshWorkspaceMods(filePath);
        return true;
    }

    bool PackManagerWidget::IsInsideWorkspace(const QString& filePath) const
    {
        const QString workspaceRoot = QDir::cleanPath(
            ToQString(FoundationService::Get().GetWorkspace().m_rootPath));
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