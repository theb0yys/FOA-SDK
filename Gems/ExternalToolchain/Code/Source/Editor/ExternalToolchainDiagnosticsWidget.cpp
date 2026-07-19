/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "ExternalToolchainDiagnosticsWidget.h"

#include <QAbstractItemView>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace ExternalToolchain
{
    namespace
    {
        QString ToQString(const AZStd::string& value)
        {
            return QString::fromUtf8(value.c_str());
        }

        QString ConfigurationState(
            const ExternalToolResolvedConfigurationValue& value)
        {
            if (!value.m_valueValid)
            {
                return QStringLiteral("invalid");
            }
            if (value.m_configured)
            {
                return QStringLiteral("configured");
            }
            return value.m_required
                ? QStringLiteral("missing required")
                : QStringLiteral("unset");
        }

        void ConfigureTable(QTableWidget* table)
        {
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->setSelectionBehavior(QAbstractItemView::SelectRows);
            table->verticalHeader()->setVisible(false);
            table->horizontalHeader()->setStretchLastSection(true);
            table->horizontalHeader()->setSectionResizeMode(
                QHeaderView::ResizeToContents);
        }
    } // namespace

    ExternalToolchainDiagnosticsWidget::ExternalToolchainDiagnosticsWidget(
        QWidget* parent)
        : QWidget(parent)
    {
        auto* layout = new QVBoxLayout(this);
        m_statusLabel = new QLabel(this);
        m_statusLabel->setWordWrap(true);
        layout->addWidget(m_statusLabel);

        m_refreshButton = new QPushButton(
            QStringLiteral("Refresh local discovery"),
            this);
        m_refreshButton->setToolTip(
            QStringLiteral(
                "Inspect configured local files and directories. No external process is launched."));
        connect(
            m_refreshButton,
            &QPushButton::clicked,
            this,
            [this]()
            {
                RefreshDiscovery();
            });
        layout->addWidget(m_refreshButton);

        auto* providersLabel = new QLabel(
            QStringLiteral("Providers and discovery results"),
            this);
        layout->addWidget(providersLabel);

        m_providerTable = new QTableWidget(this);
        m_providerTable->setColumnCount(8);
        m_providerTable->setHorizontalHeaderLabels(
            QStringList{
                "Provider ID",
                "Name",
                "Provider Version",
                "Family",
                "Discovery",
                "Selected Path",
                "Tool Version",
                "Candidates" });
        ConfigureTable(m_providerTable);
        layout->addWidget(m_providerTable);

        auto* configurationLabel = new QLabel(
            QStringLiteral("Resolved configuration (session > user > project > provider default)"),
            this);
        layout->addWidget(configurationLabel);

        m_configurationTable = new QTableWidget(this);
        m_configurationTable->setColumnCount(7);
        m_configurationTable->setHorizontalHeaderLabels(
            QStringList{
                "Provider ID",
                "Key",
                "Name",
                "Kind",
                "Layer",
                "Value",
                "State" });
        ConfigureTable(m_configurationTable);
        layout->addWidget(m_configurationTable);

        ExternalToolchainNotificationBus::Handler::BusConnect();
        Refresh();
    }

    ExternalToolchainDiagnosticsWidget::~ExternalToolchainDiagnosticsWidget()
    {
        ExternalToolchainNotificationBus::Handler::BusDisconnect();
    }

    void ExternalToolchainDiagnosticsWidget::OnExternalToolProviderRegistered(
        const ExternalToolProviderDescriptor&)
    {
        Refresh();
    }

    void ExternalToolchainDiagnosticsWidget::OnExternalToolProviderRegistrationFinalized(
        AZ::u64)
    {
        Refresh();
    }

    void ExternalToolchainDiagnosticsWidget::OnExternalToolConfigurationChanged(
        const AZStd::string&)
    {
        Refresh();
    }

    void ExternalToolchainDiagnosticsWidget::OnExternalToolDiscoveryRefreshed(
        AZ::u64)
    {
        Refresh();
    }

    void ExternalToolchainDiagnosticsWidget::RefreshDiscovery()
    {
        if (ExternalToolchainRequests* host = ExternalToolchainInterface::Get())
        {
            const ProviderOperationResult result = host->RefreshProviderDiscovery();
            if (!result.m_success)
            {
                m_statusLabel->setText(ToQString(result.m_message));
            }
        }
        Refresh();
    }

    void ExternalToolchainDiagnosticsWidget::Refresh()
    {
        ExternalToolchainRequests* host = ExternalToolchainInterface::Get();
        if (!host)
        {
            m_statusLabel->setText(
                "External Toolchain host service is not available.");
            m_refreshButton->setEnabled(false);
            m_providerTable->setRowCount(0);
            m_configurationTable->setRowCount(0);
            return;
        }

        const AZStd::vector<ExternalToolProviderDescriptor> providers =
            host->EnumerateProviders();
        const AZStd::vector<ExternalToolDiscoveryResult> discoveryResults =
            host->EnumerateDiscoveryResults();
        const bool finalized = host->IsProviderRegistrationFinalized();
        m_refreshButton->setEnabled(finalized);

        const QString phase = finalized
            ? QStringLiteral("finalized")
            : QStringLiteral("open");
        m_statusLabel->setText(
            QStringLiteral(
                "Provider registration is %1. %2 provider(s) and %3 discovery result(s) are present. "
                "Discovery only inspects configured local paths; it does not launch tools, run shell commands, or write assets.")
                .arg(phase)
                .arg(static_cast<qulonglong>(providers.size()))
                .arg(static_cast<qulonglong>(discoveryResults.size())));

        m_providerTable->setRowCount(static_cast<int>(providers.size()));
        int configurationRowCount = 0;
        for (const ExternalToolProviderDescriptor& provider : providers)
        {
            configurationRowCount += static_cast<int>(
                host->EnumerateResolvedConfiguration(provider.m_providerId).size());
        }
        m_configurationTable->setRowCount(configurationRowCount);

        int configurationRow = 0;
        for (int row = 0; row < static_cast<int>(providers.size()); ++row)
        {
            const ExternalToolProviderDescriptor& provider = providers[row];
            ExternalToolDiscoveryResult discovery;
            const bool hasDiscovery =
                host->GetDiscoveryResult(provider.m_providerId, discovery);

            m_providerTable->setItem(
                row, 0, new QTableWidgetItem(ToQString(provider.m_providerId)));
            m_providerTable->setItem(
                row, 1, new QTableWidgetItem(ToQString(provider.m_displayName)));
            m_providerTable->setItem(
                row, 2, new QTableWidgetItem(ToQString(provider.m_providerVersion)));
            m_providerTable->setItem(
                row, 3, new QTableWidgetItem(ToQString(ToString(provider.m_toolFamily))));
            m_providerTable->setItem(
                row,
                4,
                new QTableWidgetItem(
                    hasDiscovery
                        ? ToQString(ToString(discovery.m_status))
                        : QStringLiteral("not_run")));
            m_providerTable->setItem(
                row,
                5,
                new QTableWidgetItem(
                    hasDiscovery ? ToQString(discovery.m_selectedPath) : QString()));
            m_providerTable->setItem(
                row,
                6,
                new QTableWidgetItem(
                    hasDiscovery ? ToQString(discovery.m_selectedVersion) : QString()));
            m_providerTable->setItem(
                row,
                7,
                new QTableWidgetItem(
                    hasDiscovery
                        ? QString::number(
                            static_cast<qulonglong>(discovery.m_candidates.size()))
                        : QStringLiteral("0")));

            const AZStd::vector<ExternalToolResolvedConfigurationValue> values =
                host->EnumerateResolvedConfiguration(provider.m_providerId);
            for (const ExternalToolResolvedConfigurationValue& value : values)
            {
                const QString displayedValue = value.m_sensitive && value.m_configured
                    ? QStringLiteral("<hidden>")
                    : ToQString(value.m_value);
                m_configurationTable->setItem(
                    configurationRow,
                    0,
                    new QTableWidgetItem(ToQString(provider.m_providerId)));
                m_configurationTable->setItem(
                    configurationRow,
                    1,
                    new QTableWidgetItem(ToQString(value.m_key)));
                m_configurationTable->setItem(
                    configurationRow,
                    2,
                    new QTableWidgetItem(ToQString(value.m_displayName)));
                m_configurationTable->setItem(
                    configurationRow,
                    3,
                    new QTableWidgetItem(ToQString(ToString(value.m_kind))));
                m_configurationTable->setItem(
                    configurationRow,
                    4,
                    new QTableWidgetItem(ToQString(ToString(value.m_layer))));
                m_configurationTable->setItem(
                    configurationRow,
                    5,
                    new QTableWidgetItem(displayedValue));
                m_configurationTable->setItem(
                    configurationRow,
                    6,
                    new QTableWidgetItem(ConfigurationState(value)));
                ++configurationRow;
            }
        }
    }
} // namespace ExternalToolchain
