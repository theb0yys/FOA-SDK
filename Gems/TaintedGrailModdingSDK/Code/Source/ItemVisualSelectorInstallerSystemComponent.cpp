/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "ItemVisualSelectorInstallerSystemComponent.h"

#include "ItemVisualLifecycleEnhancer.h"
#include "ItemVisualSelectionRestoreBridge.h"
#include "ItemVisualSelectorWidget.h"

#include <AzCore/Debug/Trace.h>
#include <AzCore/Math/Crc.h>
#include <AzCore/Serialization/SerializeContext.h>

#include <QApplication>
#include <QComboBox>
#include <QEvent>
#include <QLabel>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QTabWidget>
#include <QTimer>
#include <QWidget>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        constexpr const char* InstalledProperty = "TaintedGrail.ItemVisualSelectorInstalled";

        struct InstalledVisualSelectorTab
        {
            QPointer<QWidget> m_host;
            QPointer<QTabWidget> m_tabs;
            QPointer<ItemVisualSelectorWidget> m_selector;
        };

        class ItemVisualSelectorEventFilter final
            : public QObject
        {
        public:
            bool eventFilter(QObject* watched, QEvent* event) override
            {
                if (event && (event->type() == QEvent::Polish || event->type() == QEvent::Show))
                {
                    TryInstall(qobject_cast<QWidget*>(watched));
                }
                return QObject::eventFilter(watched, event);
            }

            void TryInstall(QWidget* candidate)
            {
                if (!candidate || candidate->property(InstalledProperty).toBool())
                {
                    return;
                }

                bool isItemRecipeEditor = false;
                const QList<QLabel*> directLabels = candidate->findChildren<QLabel*>(QString(), Qt::FindDirectChildrenOnly);
                for (const QLabel* label : directLabels)
                {
                    if (label && label->text().contains(QStringLiteral("Item and Recipe Editor"), Qt::CaseInsensitive))
                    {
                        isItemRecipeEditor = true;
                        break;
                    }
                }
                if (!isItemRecipeEditor)
                {
                    return;
                }

                QTabWidget* tabs = candidate->findChild<QTabWidget*>(QString(), Qt::FindDirectChildrenOnly);
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

                new ItemVisualLifecycleEnhancer(selector);
                new ItemVisualSelectionRestoreBridge(selector);

                // Follow the authoring selection by canonical ID. Opening the preview
                // must not restore an unrelated item from the old asset-browser state.
                QPointer<QComboBox> items = candidate->findChild<QComboBox*>(QStringLiteral("economyItemChoice"));
                QPointer<QComboBox> recipes = candidate->findChild<QComboBox*>(QStringLiteral("economyRecipeChoice"));
                selector->setProperty("authoringTab", tabs->currentIndex() == 1 ? 1 : 0);
                const auto syncTarget = [selector, items, recipes]()
                {
                    const auto choice = selector->property("authoringTab").toInt() == 1 ? recipes : items;
                    selector->SetTargetRecord(choice ? choice->currentData().toString() : QString());
                };
                connect(tabs, &QTabWidget::currentChanged, selector, [selector, tabs, syncTarget](int index)
                {
                    if (index == 0 || index == 1)
                    {
                        selector->setProperty("authoringTab", index);
                    }
                    if (tabs->currentWidget() == selector)
                    {
                        syncTarget();
                    }
                });
                for (const auto& choice : {items, recipes})
                {
                    if (choice)
                    {
                        connect(choice, qOverload<int>(&QComboBox::currentIndexChanged), selector, syncTarget);
                    }
                }
                QTimer::singleShot(0, selector, syncTarget);

                InstalledVisualSelectorTab installed;
                installed.m_host = candidate;
                installed.m_tabs = tabs;
                installed.m_selector = selector;
                m_installedTabs.push_back(installed);
            }

            void RemoveInstalled()
            {
                for (const InstalledVisualSelectorTab& installed : m_installedTabs)
                {
                    if (installed.m_host)
                    {
                        installed.m_host->setProperty(InstalledProperty, false);
                    }
                    if (installed.m_tabs && installed.m_selector)
                    {
                        const int index = installed.m_tabs->indexOf(installed.m_selector.data());
                        if (index >= 0)
                        {
                            installed.m_tabs->removeTab(index);
                        }
                        delete installed.m_selector.data();
                    }
                }
                m_installedTabs.clear();
            }

        private:
            QList<InstalledVisualSelectorTab> m_installedTabs;
        };
    }

    void ItemVisualSelectorInstallerSystemComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<ItemVisualSelectorInstallerSystemComponent, AZ::Component>()->Version(1);
        }
    }

    void ItemVisualSelectorInstallerSystemComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("TaintedGrailItemVisualSelectorService"));
    }

    void ItemVisualSelectorInstallerSystemComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("TaintedGrailItemVisualSelectorService"));
    }

    void ItemVisualSelectorInstallerSystemComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
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

        auto* filter = static_cast<ItemVisualSelectorEventFilter*>(m_eventFilter);
        if (qApp)
        {
            qApp->removeEventFilter(filter);
        }
        filter->RemoveInstalled();
        delete filter;
        m_eventFilter = nullptr;
    }
} // namespace TaintedGrailModdingSDK
