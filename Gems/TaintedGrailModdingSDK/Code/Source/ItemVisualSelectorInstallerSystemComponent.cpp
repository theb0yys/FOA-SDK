/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "ItemVisualSelectorInstallerSystemComponent.h"

#include "ItemVisualSelectorWidget.h"

#include <AzCore/Debug/Trace.h>
#include <AzCore/Math/Crc.h>
#include <AzCore/Serialization/SerializeContext.h>

#include <QApplication>
#include <QEvent>
#include <QLabel>
#include <QObject>
#include <QTabWidget>
#include <QWidget>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        constexpr const char* InstalledProperty = "TaintedGrail.ItemVisualSelectorInstalled";

        class ItemVisualSelectorEventFilter final
            : public QObject
        {
        public:
            bool eventFilter(QObject* watched, QEvent* event) override
            {
                if (event
                    && (event->type() == QEvent::Polish
                        || event->type() == QEvent::Show
                        || event->type() == QEvent::ChildAdded))
                {
                    TryInstall(qobject_cast<QWidget*>(watched));
                }
                return QObject::eventFilter(watched, event);
            }

            void TryInstall(QWidget* candidate) const
            {
                if (!candidate || candidate->property(InstalledProperty).toBool())
                {
                    return;
                }

                bool isItemRecipeEditor = false;
                const QList<QLabel*> directLabels = candidate->findChildren<QLabel*>(
                    QString(),
                    Qt::FindDirectChildrenOnly);
                for (const QLabel* label : directLabels)
                {
                    if (label && label->text().contains(
                            QStringLiteral("Item and Recipe Editor"),
                            Qt::CaseInsensitive))
                    {
                        isItemRecipeEditor = true;
                        break;
                    }
                }
                if (!isItemRecipeEditor)
                {
                    return;
                }

                QTabWidget* tabs = candidate->findChild<QTabWidget*>(
                    QString(),
                    Qt::FindDirectChildrenOnly);
                if (!tabs)
                {
                    AZ_Warning(
                        "TaintedGrailModdingSDK",
                        false,
                        "Item/Recipe Editor was identified but its direct tab host was unavailable.");
                    return;
                }

                candidate->setProperty(InstalledProperty, true);
                auto* selector = new ItemVisualSelectorWidget(tabs);
                selector->setProperty("TaintedGrail.VisualPreviewTab", true);
                tabs->addTab(selector, QObject::tr("Visual Preview"));
            }
        };
    } // namespace

    void ItemVisualSelectorInstallerSystemComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<ItemVisualSelectorInstallerSystemComponent, AZ::Component>()
                ->Version(1);
        }
    }

    void ItemVisualSelectorInstallerSystemComponent::GetProvidedServices(
        AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("TaintedGrailItemVisualSelectorService"));
    }

    void ItemVisualSelectorInstallerSystemComponent::GetIncompatibleServices(
        AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("TaintedGrailItemVisualSelectorService"));
    }

    void ItemVisualSelectorInstallerSystemComponent::GetRequiredServices(
        AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        required.push_back(AZ_CRC_CE("TaintedGrailModdingSDKService"));
    }

    void ItemVisualSelectorInstallerSystemComponent::Activate()
    {
        if (!qApp || m_eventFilter)
        {
            return;
        }
        auto* filter = new ItemVisualSelectorEventFilter();
        m_eventFilter = filter;
        qApp->installEventFilter(filter);
        for (QWidget* widget : QApplication::allWidgets())
        {
            filter->TryInstall(widget);
        }
    }

    void ItemVisualSelectorInstallerSystemComponent::Deactivate()
    {
        if (!m_eventFilter)
        {
            return;
        }
        if (qApp)
        {
            qApp->removeEventFilter(m_eventFilter);
        }
        delete m_eventFilter;
        m_eventFilter = nullptr;
    }
} // namespace TaintedGrailModdingSDK
