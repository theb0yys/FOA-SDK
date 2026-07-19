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
    }

    ExternalToolchainDiagnosticsWidget::ExternalToolchainDiagnosticsWidget(QWidget* parent)
        : QWidget(parent)
    {
        auto* layout = new QVBoxLayout(this);
        m_statusLabel = new QLabel(this);
        m_statusLabel->setWordWrap(true);
        layout->addWidget(m_statusLabel);

        m_providerTable = new QTableWidget(this);
        m_providerTable->setColumnCount(5);
        m_providerTable->setHorizontalHeaderLabels(
            QStringList{ "Provider ID", "Name", "Version", "Family", "Commands" });
        m_providerTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_providerTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_providerTable->verticalHeader()->setVisible(false);
        m_providerTable->horizontalHeader()->setStretchLastSection(true);
        m_providerTable->horizontalHeader()->setSectionResizeMode(
            QHeaderView::ResizeToContents);
        layout->addWidget(m_providerTable);

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

    void ExternalToolchainDiagnosticsWidget::Refresh()
    {
        ExternalToolchainRequests* host = ExternalToolchainInterface::Get();
        if (!host)
        {
            m_statusLabel->setText(
                "External Toolchain host service is not available.");
            m_providerTable->setRowCount(0);
            return;
        }

        const AZStd::vector<ExternalToolProviderDescriptor> providers =
            host->EnumerateProviders();
        const QString phase = host->IsProviderRegistrationFinalized()
            ? QStringLiteral("finalized")
            : QStringLiteral("open");
        m_statusLabel->setText(
            QStringLiteral(
                "Provider registration is %1. %2 provider(s) are registered. "
                "This foundation slice does not launch external processes or write assets.")
                .arg(phase)
                .arg(static_cast<qulonglong>(providers.size())));

        m_providerTable->setRowCount(static_cast<int>(providers.size()));
        for (int row = 0; row < static_cast<int>(providers.size()); ++row)
        {
            const ExternalToolProviderDescriptor& provider = providers[row];
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
                    QString::number(static_cast<qulonglong>(provider.m_commands.size()))));
        }
    }
} // namespace ExternalToolchain
