/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "PackManagerWidget.h"

#include "FoundationService.h"

#include <AzCore/std/algorithm.h>

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
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QStringList>
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
            tr("Create or open a mod. FOA-SDK fills the current game compatibility and stores the manifest inside the workspace automatically."),
            this);
        description->setWordWrap(true);
        rootLayout->addWidget(description);

        auto* summaryGroup = new QGroupBox(tr("Current mod"), this);
        auto* summaryLayout = new QFormLayout(summaryGroup);
        m_activePackValue = new QLabel(summaryGroup);
        summaryLayout->addRow(tr("Mod"), m_activePackValue);
        rootLayout->addWidget(summaryGroup);

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
        auto* openButton = new QPushButton(tr("Open existing..."), this);
        auto* saveButton = new QPushButton(tr("Save mod"), this);
        buttonLayout->addWidget(newButton);
        buttonLayout->addWidget(openButton);
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
        connect(newButton, &QPushButton::clicked, this, [this]()
        {
            FoundationService::Get().ClearActivePack();
            ClearFormForNewPack();
            SetStatus(tr("New mod ready. Enter a name and author, then Save mod."));
        });
        connect(openButton, &QPushButton::clicked, this, [this]()
        {
            const WorkspaceModel& workspace = FoundationService::Get().GetWorkspace();
            const QString workspaceRoot = ToQString(workspace.m_rootPath);
            const QString startDirectory = workspaceRoot.isEmpty()
                ? QDir::homePath()
                : QDir(workspaceRoot).filePath(QStringLiteral("Packs"));
            const QString filePath = QFileDialog::getOpenFileName(
                this,
                tr("Open FOA-SDK mod"),
                startDirectory,
                tr("FOA-SDK mod (*.tgpack.json);;JSON files (*.json)"));
            if (filePath.isEmpty())
            {
                return;
            }
            if (!IsInsideWorkspace(filePath))
            {
                SetStatus(
                    tr("Open a mod from this FOA-SDK workspace. External paths are not used for normal mod projects."),
                    true);
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
            SetStatus(tr("Mod opened."));
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
