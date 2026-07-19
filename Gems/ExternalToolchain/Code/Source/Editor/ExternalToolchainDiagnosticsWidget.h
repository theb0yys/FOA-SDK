/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <ExternalToolchain/ExternalToolchainBus.h>

#include <QWidget>

class QLabel;
class QTableWidget;

namespace ExternalToolchain
{
    class ExternalToolchainDiagnosticsWidget final
        : public QWidget
        , protected ExternalToolchainNotificationBus::Handler
    {
    public:
        explicit ExternalToolchainDiagnosticsWidget(QWidget* parent = nullptr);
        ~ExternalToolchainDiagnosticsWidget() override;

    private:
        void OnExternalToolProviderRegistered(
            const ExternalToolProviderDescriptor&) override;
        void OnExternalToolProviderRegistrationFinalized(AZ::u64) override;
        void Refresh();

        QLabel* m_statusLabel = nullptr;
        QTableWidget* m_providerTable = nullptr;
    };
} // namespace ExternalToolchain
