/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AvalonAIEditorWidget.h"

#include <AvalonAiExtension.h>
#include <ExtensionRequestBus.h>

#include <AzCore/std/utility/move.h>

#include <QFont>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLabel>
#include <QPushButton>
#include <QStringList>
#include <QTextEdit>
#include <QVBoxLayout>

#include <cmath>
#include <limits>

namespace AvalonAIAuthoring
{
    namespace
    {
        using namespace TaintedGrailModdingSDK;
        using namespace TaintedGrailModdingSDK::AvalonAiExtension;

        constexpr const char* ExtensionId = "extension.avalon-ai-authoring";
        constexpr const char* DocumentPath = "avalon-ai.package.json";

        AZStd::string ToAzString(const QString& value)
        {
            const QByteArray utf8 = value.toUtf8();
            return AZStd::string(utf8.constData(), static_cast<size_t>(utf8.size()));
        }

        QString ToQString(const AZStd::string& value)
        {
            return QString::fromUtf8(value.c_str());
        }

        bool ReadString(
            const QJsonObject& object,
            const char* key,
            AZStd::string& target,
            QStringList& errors,
            bool required = false)
        {
            const QJsonValue value = object.value(QString::fromUtf8(key));
            if (value.isUndefined())
            {
                if (required)
                {
                    errors.push_back(QStringLiteral("Missing string field: %1").arg(key));
                    return false;
                }
                target.clear();
                return true;
            }
            if (!value.isString())
            {
                errors.push_back(QStringLiteral("Field must be a string: %1").arg(key));
                return false;
            }
            target = ToAzString(value.toString());
            return true;
        }

        bool ReadBool(
            const QJsonObject& object,
            const char* key,
            bool& target,
            QStringList& errors,
            bool defaultValue)
        {
            const QJsonValue value = object.value(QString::fromUtf8(key));
            if (value.isUndefined())
            {
                target = defaultValue;
                return true;
            }
            if (!value.isBool())
            {
                errors.push_back(QStringLiteral("Field must be boolean: %1").arg(key));
                return false;
            }
            target = value.toBool();
            return true;
        }

        bool ReadInteger(
            const QJsonObject& object,
            const char* key,
            qint64& target,
            QStringList& errors,
            bool required = true)
        {
            const QJsonValue value = object.value(QString::fromUtf8(key));
            if (value.isUndefined() && !required)
            {
                target = 0;
                return true;
            }
            if (!value.isDouble() || !std::isfinite(value.toDouble())
                || std::floor(value.toDouble()) != value.toDouble()
                || value.toDouble() < 0.0
                || value.toDouble() > 9007199254740991.0)
            {
                errors.push_back(QStringLiteral("Field must be a non-negative safe integer: %1").arg(key));
                return false;
            }
            target = static_cast<qint64>(value.toDouble());
            return true;
        }

        bool ReadSignedInteger(
            const QJsonObject& object,
            const char* key,
            int& target,
            QStringList& errors)
        {
            const QJsonValue value = object.value(QString::fromUtf8(key));
            if (!value.isDouble() || !std::isfinite(value.toDouble())
                || std::floor(value.toDouble()) != value.toDouble()
                || value.toDouble() < static_cast<double>(std::numeric_limits<int>::min())
                || value.toDouble() > static_cast<double>(std::numeric_limits<int>::max()))
            {
                errors.push_back(QStringLiteral("Field must be an integer: %1").arg(key));
                return false;
            }
            target = value.toInt();
            return true;
        }

        bool ReadStrings(
            const QJsonObject& object,
            const char* key,
            AZStd::vector<AZStd::string>& target,
            QStringList& errors)
        {
            target.clear();
            const QJsonValue value = object.value(QString::fromUtf8(key));
            if (!value.isArray())
            {
                errors.push_back(QStringLiteral("Field must be a string array: %1").arg(key));
                return false;
            }
            for (const QJsonValue& entry : value.toArray())
            {
                if (!entry.isString())
                {
                    errors.push_back(QStringLiteral("Array contains non-string value: %1").arg(key));
                    return false;
                }
                target.push_back(ToAzString(entry.toString()));
            }
            return true;
        }

        PlanningComparison ReadComparison(const QString& value, bool& known)
        {
            known = true;
            if (value == QStringLiteral("equal")) return PlanningComparison::Equal;
            if (value == QStringLiteral("not_equal")) return PlanningComparison::NotEqual;
            if (value == QStringLiteral("greater_than")) return PlanningComparison::GreaterThan;
            if (value == QStringLiteral("greater_than_or_equal")) return PlanningComparison::GreaterThanOrEqual;
            if (value == QStringLiteral("less_than")) return PlanningComparison::LessThan;
            if (value == QStringLiteral("less_than_or_equal")) return PlanningComparison::LessThanOrEqual;
            known = false;
            return PlanningComparison::Equal;
        }

        InterruptPolicy ReadInterruptPolicy(const QString& value, bool& known)
        {
            known = true;
            if (value == QStringLiteral("never")) return InterruptPolicy::Never;
            if (value == QStringLiteral("on_invalid_target")) return InterruptPolicy::OnInvalidTarget;
            if (value == QStringLiteral("on_threat_increase")) return InterruptPolicy::OnThreatIncrease;
            if (value == QStringLiteral("on_damage")) return InterruptPolicy::OnDamage;
            if (value == QStringLiteral("on_goal_change")) return InterruptPolicy::OnGoalChange;
            if (value == QStringLiteral("always")) return InterruptPolicy::Always;
            known = false;
            return InterruptPolicy::Never;
        }

        AZStd::vector<PlanningCondition> ParseConditions(
            const QJsonValue& value,
            const QString& label,
            QStringList& errors)
        {
            AZStd::vector<PlanningCondition> conditions;
            if (!value.isArray())
            {
                errors.push_back(label + QStringLiteral(" must be an array"));
                return conditions;
            }
            for (const QJsonValue& entry : value.toArray())
            {
                if (!entry.isObject())
                {
                    errors.push_back(label + QStringLiteral(" entries must be objects"));
                    continue;
                }
                const QJsonObject object = entry.toObject();
                PlanningCondition condition;
                ReadString(object, "fact_id", condition.m_factId, errors, true);
                const QJsonValue comparison = object.value(QStringLiteral("comparison"));
                bool known = false;
                if (comparison.isString())
                {
                    condition.m_comparison = ReadComparison(comparison.toString(), known);
                }
                if (!known)
                {
                    errors.push_back(label + QStringLiteral(" contains an unknown comparison"));
                }
                ReadSignedInteger(object, "value", condition.m_value, errors);
                conditions.push_back(AZStd::move(condition));
            }
            return conditions;
        }

        AZStd::vector<PlanningEffect> ParseEffects(
            const QJsonValue& value,
            QStringList& errors)
        {
            AZStd::vector<PlanningEffect> effects;
            if (!value.isArray())
            {
                errors.push_back(QStringLiteral("action effects must be an array"));
                return effects;
            }
            for (const QJsonValue& entry : value.toArray())
            {
                if (!entry.isObject())
                {
                    errors.push_back(QStringLiteral("action effect entries must be objects"));
                    continue;
                }
                const QJsonObject object = entry.toObject();
                PlanningEffect effect;
                ReadString(object, "fact_id", effect.m_factId, errors, true);
                ReadSignedInteger(object, "assigned_value", effect.m_assignedValue, errors);
                effects.push_back(AZStd::move(effect));
            }
            return effects;
        }

        bool ParsePackage(
            const QByteArray& payload,
            PackageManifest& manifest,
            QStringList& errors)
        {
            QJsonParseError parseError;
            const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
            if (parseError.error != QJsonParseError::NoError || !document.isObject())
            {
                errors.push_back(QStringLiteral("Invalid JSON object: %1").arg(parseError.errorString()));
                return false;
            }
            const QJsonObject object = document.object();
            const QJsonValue schema = object.value(QStringLiteral("schema_version"));
            if (!schema.isDouble() || schema.toDouble() != 1.0)
            {
                errors.push_back(QStringLiteral("schema_version must be 1"));
            }
            manifest.m_schemaVersion = 1;
            ReadString(object, "package_id", manifest.m_packageId, errors, true);
            ReadString(object, "display_name", manifest.m_displayName, errors, true);
            ReadString(object, "package_version", manifest.m_packageVersion, errors, true);
            ReadString(object, "blackboard_namespace", manifest.m_blackboardNamespace, errors, true);
            qint64 blackboardSchema = 0;
            qint64 cadence = 0;
            ReadInteger(object, "blackboard_schema_version", blackboardSchema, errors);
            ReadInteger(object, "maximum_policy_cadence_ms", cadence, errors);
            manifest.m_blackboardSchemaVersion = static_cast<int>(blackboardSchema);
            manifest.m_maximumPolicyCadenceMilliseconds = static_cast<AZ::u64>(cadence);
            ReadStrings(object, "supported_actor_roles", manifest.m_supportedActorRoles, errors);
            ReadStrings(object, "required_capabilities", manifest.m_requiredCapabilities, errors);
            ReadBool(object, "execution_enabled", manifest.m_executionEnabled, errors, false);
            ReadBool(object, "host_linked", manifest.m_hostLinked, errors, false);

            const QJsonValue apiValue = object.value(QStringLiteral("required_runtime_api"));
            if (!apiValue.isObject())
            {
                errors.push_back(QStringLiteral("required_runtime_api must be an object"));
            }
            else
            {
                qint64 major = 0;
                qint64 minor = 0;
                ReadInteger(apiValue.toObject(), "major", major, errors);
                ReadInteger(apiValue.toObject(), "minor", minor, errors);
                manifest.m_requiredRuntimeApi.m_major = static_cast<int>(major);
                manifest.m_requiredRuntimeApi.m_minor = static_cast<int>(minor);
            }

            const QJsonValue keys = object.value(QStringLiteral("blackboard_keys"));
            if (!keys.isArray())
            {
                errors.push_back(QStringLiteral("blackboard_keys must be an array"));
            }
            else
            {
                for (const QJsonValue& entry : keys.toArray())
                {
                    if (!entry.isObject())
                    {
                        errors.push_back(QStringLiteral("blackboard key entries must be objects"));
                        continue;
                    }
                    const QJsonObject keyObject = entry.toObject();
                    BlackboardKey key;
                    ReadString(keyObject, "namespace", key.m_namespace, errors, true);
                    ReadString(keyObject, "name", key.m_name, errors, true);
                    ReadString(keyObject, "value_type", key.m_valueType, errors, true);
                    qint64 keySchema = 0;
                    ReadInteger(keyObject, "schema_version", keySchema, errors);
                    key.m_schemaVersion = static_cast<int>(keySchema);
                    const QString access = keyObject.value(QStringLiteral("access")).toString();
                    const QString scope = keyObject.value(QStringLiteral("scope")).toString();
                    if (access == QStringLiteral("package_local"))
                    {
                        key.m_access = BlackboardAccess::PackageLocal;
                    }
                    else if (access == QStringLiteral("authoritative"))
                    {
                        key.m_access = BlackboardAccess::Authoritative;
                    }
                    else if (access == QStringLiteral("derived"))
                    {
                        key.m_access = BlackboardAccess::Derived;
                    }
                    else
                    {
                        errors.push_back(QStringLiteral("Unknown blackboard access"));
                    }
                    if (scope == QStringLiteral("actor"))
                    {
                        key.m_scope = BlackboardScope::Actor;
                    }
                    else if (scope == QStringLiteral("world"))
                    {
                        key.m_scope = BlackboardScope::World;
                    }
                    else
                    {
                        errors.push_back(QStringLiteral("Unknown blackboard scope"));
                    }
                    ReadBool(keyObject, "persistent", key.m_persistent, errors, false);
                    manifest.m_blackboardKeys.push_back(AZStd::move(key));
                }
            }

            const QJsonValue goals = object.value(QStringLiteral("goals"));
            if (!goals.isArray())
            {
                errors.push_back(QStringLiteral("goals must be an array"));
            }
            else
            {
                for (const QJsonValue& entry : goals.toArray())
                {
                    if (!entry.isObject())
                    {
                        errors.push_back(QStringLiteral("goal entries must be objects"));
                        continue;
                    }
                    const QJsonObject goalObject = entry.toObject();
                    GoalDefinition goal;
                    ReadString(goalObject, "goal_id", goal.m_goalId, errors, true);
                    ReadSignedInteger(goalObject, "priority", goal.m_priority, errors);
                    goal.m_conditions = ParseConditions(
                        goalObject.value(QStringLiteral("conditions")),
                        QStringLiteral("goal conditions"),
                        errors);
                    manifest.m_goals.push_back(AZStd::move(goal));
                }
            }

            const QJsonValue actions = object.value(QStringLiteral("actions"));
            if (!actions.isArray())
            {
                errors.push_back(QStringLiteral("actions must be an array"));
            }
            else
            {
                for (const QJsonValue& entry : actions.toArray())
                {
                    if (!entry.isObject())
                    {
                        errors.push_back(QStringLiteral("action entries must be objects"));
                        continue;
                    }
                    const QJsonObject actionObject = entry.toObject();
                    ActionDefinition action;
                    ReadString(actionObject, "action_id", action.m_actionId, errors, true);
                    ReadString(actionObject, "target_key_id", action.m_targetKeyId, errors);
                    ReadString(actionObject, "required_capability", action.m_requiredCapability, errors, true);
                    action.m_conditions = ParseConditions(
                        actionObject.value(QStringLiteral("conditions")),
                        QStringLiteral("action conditions"),
                        errors);
                    action.m_effects = ParseEffects(
                        actionObject.value(QStringLiteral("effects")), errors);
                    bool known = false;
                    const QJsonValue policy = actionObject.value(QStringLiteral("interrupt_policy"));
                    if (policy.isString())
                    {
                        action.m_interruptPolicy = ReadInterruptPolicy(policy.toString(), known);
                    }
                    if (!known)
                    {
                        errors.push_back(QStringLiteral("Unknown action interrupt_policy"));
                    }
                    qint64 timeout = 0;
                    ReadInteger(actionObject, "timeout_ms", timeout, errors);
                    action.m_timeoutMilliseconds = static_cast<AZ::u64>(timeout);
                    manifest.m_actions.push_back(AZStd::move(action));
                }
            }
            return errors.empty();
        }

        QJsonObject BuildStarterDocument()
        {
            QJsonObject api;
            api.insert(QStringLiteral("major"), 2);
            api.insert(QStringLiteral("minor"), 0);

            QJsonObject key;
            key.insert(QStringLiteral("namespace"), QStringLiteral("avalon.preview"));
            key.insert(QStringLiteral("name"), QStringLiteral("target"));
            key.insert(QStringLiteral("schema_version"), 1);
            key.insert(QStringLiteral("access"), QStringLiteral("package_local"));
            key.insert(QStringLiteral("scope"), QStringLiteral("actor"));
            key.insert(QStringLiteral("value_type"), QStringLiteral("entity_ref"));
            key.insert(QStringLiteral("persistent"), false);

            QJsonObject condition;
            condition.insert(QStringLiteral("fact_id"), QStringLiteral("fact.target-visible"));
            condition.insert(QStringLiteral("comparison"), QStringLiteral("equal"));
            condition.insert(QStringLiteral("value"), 1);

            QJsonObject goal;
            goal.insert(QStringLiteral("goal_id"), QStringLiteral("goal.observe-target"));
            goal.insert(QStringLiteral("priority"), 10);
            goal.insert(QStringLiteral("conditions"), QJsonArray{ condition });

            QJsonObject effect;
            effect.insert(QStringLiteral("fact_id"), QStringLiteral("fact.target-observed"));
            effect.insert(QStringLiteral("assigned_value"), 1);

            QJsonObject action;
            action.insert(QStringLiteral("action_id"), QStringLiteral("action.observe-target"));
            action.insert(QStringLiteral("conditions"), QJsonArray{ condition });
            action.insert(QStringLiteral("effects"), QJsonArray{ effect });
            action.insert(QStringLiteral("target_key_id"), QStringLiteral("avalon.preview.target"));
            action.insert(QStringLiteral("required_capability"), QStringLiteral("capability.observe"));
            action.insert(QStringLiteral("interrupt_policy"), QStringLiteral("on_invalid_target"));
            action.insert(QStringLiteral("timeout_ms"), 1000);

            QJsonObject root;
            root.insert(QStringLiteral("schema_version"), 1);
            root.insert(QStringLiteral("package_id"), QStringLiteral("avalon-ai.preview"));
            root.insert(QStringLiteral("display_name"), QStringLiteral("Preview AI package"));
            root.insert(QStringLiteral("package_version"), QStringLiteral("0.1.0"));
            root.insert(QStringLiteral("required_runtime_api"), api);
            root.insert(QStringLiteral("blackboard_namespace"), QStringLiteral("avalon.preview"));
            root.insert(QStringLiteral("blackboard_schema_version"), 1);
            root.insert(QStringLiteral("maximum_policy_cadence_ms"), 1000);
            root.insert(QStringLiteral("supported_actor_roles"), QJsonArray{ QStringLiteral("role.actor") });
            root.insert(QStringLiteral("required_capabilities"), QJsonArray{ QStringLiteral("capability.observe") });
            root.insert(QStringLiteral("blackboard_keys"), QJsonArray{ key });
            root.insert(QStringLiteral("goals"), QJsonArray{ goal });
            root.insert(QStringLiteral("actions"), QJsonArray{ action });
            root.insert(QStringLiteral("execution_enabled"), false);
            root.insert(QStringLiteral("host_linked"), false);
            return root;
        }
    } // namespace

    AvalonAIEditorWidget::AvalonAIEditorWidget(QWidget* parent)
        : QWidget(parent)
    {
        setObjectName(QStringLiteral("TaintedGrailAvalonAIEditor"));
        setAccessibleName(tr("Tainted Grail Avalon AI Editor"));
        auto* layout = new QVBoxLayout(this);
        auto* heading = new QLabel(tr("Tainted Grail Avalon AI Editor"), this);
        QFont font = heading->font();
        font.setBold(true);
        font.setPointSize(font.pointSize() + 3);
        heading->setFont(font);
        layout->addWidget(heading);

        auto* boundary = new QLabel(
            tr("Author and validate Avalon AI API 2.0 packages and deterministic inert plans. "
               "The Editor does not link a runtime host, dispatch actions, mutate blackboards, "
               "launch FoA, deploy files, or write saves."),
            this);
        boundary->setWordWrap(true);
        layout->addWidget(boundary);

        auto* group = new QGroupBox(tr("Typed Avalon AI package JSON"), this);
        auto* groupLayout = new QVBoxLayout(group);
        m_document = new QTextEdit(group);
        m_document->setAcceptRichText(false);
        m_document->setAccessibleName(tr("Avalon AI package JSON"));
        m_document->setTabChangesFocus(true);
        groupLayout->addWidget(m_document, 1);
        layout->addWidget(group, 1);

        auto* buttons = new QHBoxLayout();
        auto* create = new QPushButton(tr("New"), this);
        auto* load = new QPushButton(tr("Load"), this);
        auto* validate = new QPushButton(tr("Validate and Plan"), this);
        m_save = new QPushButton(tr("Save"), this);
        auto* revert = new QPushButton(tr("Revert"), this);
        buttons->addWidget(create);
        buttons->addWidget(load);
        buttons->addWidget(validate);
        buttons->addWidget(m_save);
        buttons->addWidget(revert);
        buttons->addStretch(1);
        layout->addLayout(buttons);

        m_status = new QLabel(tr("Load a workspace package or create a new inert package."), this);
        m_status->setWordWrap(true);
        m_status->setTextInteractionFlags(Qt::TextSelectableByKeyboard | Qt::TextSelectableByMouse);
        layout->addWidget(m_status);
        m_save->setEnabled(false);

        connect(create, &QPushButton::clicked, this, [this]() { CreateDocument(); });
        connect(load, &QPushButton::clicked, this, [this]() { LoadDocument(); });
        connect(validate, &QPushButton::clicked, this, [this]() { ValidateDocument(); });
        connect(m_save, &QPushButton::clicked, this, [this]() { SaveDocument(); });
        connect(revert, &QPushButton::clicked, this, [this]() { RevertDocument(); });
        connect(m_document, &QTextEdit::textChanged, this, [this]()
        {
            m_valid = false;
            m_save->setEnabled(false);
        });
    }

    void AvalonAIEditorWidget::CreateDocument()
    {
        ExtensionAPI::ProfileView profile;
        AZStd::string error;
        bool available = false;
        ExtensionRequestBus::BroadcastResult(
            available,
            &ExtensionRequests::GetActiveProfile,
            AZStd::string(ExtensionId),
            profile,
            &error);
        if (!available)
        {
            SetStatus(tr("Cannot create a package: %1").arg(ToQString(error)), true);
            return;
        }
        m_lastLoaded = QJsonDocument(BuildStarterDocument()).toJson(QJsonDocument::Indented);
        m_document->setPlainText(QString::fromUtf8(m_lastLoaded));
        SetStatus(
            tr("Created an unsaved inert package for profile %1.").arg(ToQString(profile.m_profileId)),
            false);
    }

    void AvalonAIEditorWidget::LoadDocument()
    {
        AZStd::string contents;
        AZStd::string error;
        bool loaded = false;
        ExtensionRequestBus::BroadcastResult(
            loaded,
            &ExtensionRequests::LoadExtensionDocument,
            AZStd::string(ExtensionId),
            AZStd::string(DocumentPath),
            contents,
            &error);
        if (!loaded)
        {
            SetStatus(tr("Load failed: %1").arg(ToQString(error)), true);
            return;
        }
        m_lastLoaded = QByteArray(contents.data(), static_cast<qsizetype>(contents.size()));
        m_document->setPlainText(QString::fromUtf8(m_lastLoaded));
        SetStatus(tr("Loaded the workspace Avalon AI package. Validate before saving changes."), false);
    }

    void AvalonAIEditorWidget::ValidateDocument()
    {
        PackageManifest manifest;
        QStringList errors;
        ParsePackage(m_document->toPlainText().toUtf8(), manifest, errors);

        ExtensionAPI::ProfileView profile;
        AZStd::string profileError;
        bool profileAvailable = false;
        ExtensionRequestBus::BroadcastResult(
            profileAvailable,
            &ExtensionRequests::GetActiveProfile,
            AZStd::string(ExtensionId),
            profile,
            &profileError);
        if (!profileAvailable)
        {
            errors.push_back(tr("Exact profile unavailable: %1").arg(ToQString(profileError)));
        }
        if (errors.empty())
        {
            const ValidationResult result = ValidateAndPlan(manifest);
            for (const ValidationIssue& issue : result.m_issues)
            {
                errors.push_back(
                    QStringLiteral("%1: %2")
                        .arg(ToQString(issue.m_code), ToQString(issue.m_message)));
            }
            m_valid = result.m_accepted;
            if (m_valid)
            {
                SetStatus(
                    tr("Valid inert authoring plan for %1 goal(s) and %2 action(s). Fingerprint: %3")
                        .arg(static_cast<qulonglong>(result.m_plan.m_goalIds.size()))
                        .arg(static_cast<qulonglong>(result.m_plan.m_actionIds.size()))
                        .arg(ToQString(result.m_plan.m_canonicalFingerprint)),
                    false);
            }
        }
        if (!errors.empty())
        {
            m_valid = false;
            SetStatus(errors.join(QLatin1Char('\n')), true);
        }
        m_save->setEnabled(m_valid);
    }

    void AvalonAIEditorWidget::SaveDocument()
    {
        ValidateDocument();
        if (!m_valid)
        {
            return;
        }
        QJsonParseError parseError;
        const QJsonDocument normalized = QJsonDocument::fromJson(
            m_document->toPlainText().toUtf8(), &parseError);
        const QByteArray payload = normalized.toJson(QJsonDocument::Indented);
        const AZStd::string contents(payload.constData(), static_cast<size_t>(payload.size()));
        AZStd::string error;
        bool saved = false;
        ExtensionRequestBus::BroadcastResult(
            saved,
            &ExtensionRequests::SaveExtensionDocument,
            AZStd::string(ExtensionId),
            AZStd::string(DocumentPath),
            contents,
            &error);
        if (!saved)
        {
            SetStatus(tr("Save failed: %1").arg(ToQString(error)), true);
            return;
        }
        m_lastLoaded = payload;
        m_document->setPlainText(QString::fromUtf8(payload));
        m_valid = true;
        m_save->setEnabled(true);
        SetStatus(tr("Avalon AI package saved atomically to the active workspace."), false);
    }

    void AvalonAIEditorWidget::RevertDocument()
    {
        if (m_lastLoaded.isEmpty())
        {
            LoadDocument();
            return;
        }
        m_document->setPlainText(QString::fromUtf8(m_lastLoaded));
        SetStatus(tr("Reverted to the last loaded or saved package."), false);
    }

    void AvalonAIEditorWidget::SetStatus(const QString& text, bool error)
    {
        m_status->setText(text);
        m_status->setStyleSheet(error ? QStringLiteral("color: #c43b3b;") : QString());
    }
} // namespace AvalonAIAuthoring
