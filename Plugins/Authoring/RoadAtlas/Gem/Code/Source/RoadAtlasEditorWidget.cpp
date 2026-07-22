/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "RoadAtlasEditorWidget.h"

#include <ExtensionRequestBus.h>
#include <RoadAtlasExtension.h>

#include <AzCore/std/utility/move.h>

#include <QDateTime>
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

namespace RoadAtlasAuthoring
{
    namespace
    {
        using namespace TaintedGrailModdingSDK;
        using namespace TaintedGrailModdingSDK::RoadAtlasExtension;

        constexpr const char* ExtensionId = "extension.avalon-core-road-atlas";
        constexpr const char* DocumentPath = "road-atlas.snapshot.json";

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

        bool ReadStrings(
            const QJsonObject& object,
            const char* key,
            AZStd::vector<AZStd::string>& target,
            QStringList& errors)
        {
            target.clear();
            const QJsonValue value = object.value(QString::fromUtf8(key));
            if (value.isUndefined())
            {
                return true;
            }
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

        ElementKind ReadElementKind(const QString& value)
        {
            if (value == QStringLiteral("road")) return ElementKind::Road;
            if (value == QStringLiteral("name")) return ElementKind::Name;
            if (value == QStringLiteral("junction")) return ElementKind::Junction;
            if (value == QStringLiteral("segment")) return ElementKind::Segment;
            if (value == QStringLiteral("anchor")) return ElementKind::Anchor;
            if (value == QStringLiteral("connector")) return ElementKind::Connector;
            return ElementKind::Unknown;
        }

        PromotionState ReadPromotionState(const QString& value)
        {
            if (value == QStringLiteral("raw")) return PromotionState::Raw;
            if (value == QStringLiteral("candidate")) return PromotionState::Candidate;
            if (value == QStringLiteral("mapped")) return PromotionState::Mapped;
            if (value == QStringLiteral("linked")) return PromotionState::Linked;
            if (value == QStringLiteral("confirmed")) return PromotionState::Confirmed;
            if (value == QStringLiteral("approved_for_planning")) return PromotionState::ApprovedForPlanning;
            if (value == QStringLiteral("blocked")) return PromotionState::Blocked;
            return PromotionState::Unknown;
        }

        EvidenceRequirementKind ReadRequirementKind(const QString& value)
        {
            if (value == QStringLiteral("name_authority")) return EvidenceRequirementKind::NameAuthority;
            if (value == QStringLiteral("source_register")) return EvidenceRequirementKind::SourceRegister;
            if (value == QStringLiteral("runtime_coordinate")) return EvidenceRequirementKind::RuntimeCoordinate;
            if (value == QStringLiteral("scene_reference")) return EvidenceRequirementKind::SceneReference;
            if (value == QStringLiteral("segment_geometry")) return EvidenceRequirementKind::SegmentGeometry;
            if (value == QStringLiteral("junction_connectivity")) return EvidenceRequirementKind::JunctionConnectivity;
            if (value == QStringLiteral("anchor_visibility")) return EvidenceRequirementKind::AnchorVisibility;
            if (value == QStringLiteral("connector_endpoint")) return EvidenceRequirementKind::ConnectorEndpoint;
            if (value == QStringLiteral("reviewer_approval")) return EvidenceRequirementKind::ReviewerApproval;
            return EvidenceRequirementKind::Unknown;
        }

        void ParseGeometry(
            const QJsonObject& object,
            Element& element,
            QStringList& errors)
        {
            const QJsonValue value = object.value(QStringLiteral("geometry"));
            if (value.isUndefined())
            {
                return;
            }
            if (!value.isArray())
            {
                errors.push_back(QStringLiteral("geometry must be an array"));
                return;
            }
            for (const QJsonValue& entry : value.toArray())
            {
                if (!entry.isObject())
                {
                    errors.push_back(QStringLiteral("geometry entries must be objects"));
                    continue;
                }
                const QJsonObject pointObject = entry.toObject();
                Coordinate point;
                const QJsonValue x = pointObject.value(QStringLiteral("x"));
                const QJsonValue y = pointObject.value(QStringLiteral("y"));
                const QJsonValue z = pointObject.value(QStringLiteral("z"));
                if (!x.isDouble() || !y.isDouble() || !z.isDouble())
                {
                    errors.push_back(QStringLiteral("geometry coordinates must be numbers"));
                    continue;
                }
                point.m_x = x.toDouble();
                point.m_y = y.toDouble();
                point.m_z = z.toDouble();
                ReadString(pointObject, "point_ref", point.m_pointRef, errors, true);
                element.m_geometry.push_back(AZStd::move(point));
            }
        }

        void ParseRequirements(
            const QJsonObject& object,
            Element& element,
            QStringList& errors)
        {
            const QJsonValue value = object.value(QStringLiteral("evidence_requirements"));
            if (value.isUndefined())
            {
                return;
            }
            if (!value.isArray())
            {
                errors.push_back(QStringLiteral("evidence_requirements must be an array"));
                return;
            }
            for (const QJsonValue& entry : value.toArray())
            {
                if (!entry.isObject())
                {
                    errors.push_back(QStringLiteral("evidence requirement entries must be objects"));
                    continue;
                }
                const QJsonObject requirementObject = entry.toObject();
                EvidenceRequirement requirement;
                ReadString(requirementObject, "requirement_id", requirement.m_requirementId, errors, true);
                const QJsonValue kind = requirementObject.value(QStringLiteral("kind"));
                if (!kind.isString())
                {
                    errors.push_back(QStringLiteral("evidence requirement kind must be a string"));
                }
                else
                {
                    requirement.m_kind = ReadRequirementKind(kind.toString());
                }
                ReadBool(requirementObject, "required", requirement.m_required, errors, false);
                ReadBool(requirementObject, "satisfied", requirement.m_satisfied, errors, false);
                ReadStrings(requirementObject, "evidence_ids", requirement.m_evidenceIds, errors);
                ReadString(requirementObject, "notes", requirement.m_notes, errors);
                element.m_evidenceRequirements.push_back(AZStd::move(requirement));
            }
        }

        bool ParseSnapshot(
            const QByteArray& payload,
            Snapshot& snapshot,
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
            if (!schema.isDouble() || schema.toInt(-1) != 1)
            {
                errors.push_back(QStringLiteral("schema_version must be 1"));
            }
            snapshot.m_schemaVersion = 1;
            ReadString(object, "snapshot_id", snapshot.m_snapshotId, errors, true);
            ReadString(object, "profile_id", snapshot.m_profileId, errors, true);
            ReadString(object, "game_version", snapshot.m_gameVersion, errors, true);
            ReadString(object, "branch", snapshot.m_branch, errors, true);
            ReadString(object, "runtime_target", snapshot.m_runtimeTarget, errors, true);
            ReadString(object, "source_fingerprint", snapshot.m_sourceFingerprint, errors, true);
            ReadString(object, "captured_at_utc", snapshot.m_capturedAtUtc, errors, true);
            ReadBool(object, "planning_only", snapshot.m_planningOnly, errors, true);
            ReadBool(object, "runtime_mutation_allowed", snapshot.m_runtimeMutationAllowed, errors, false);

            const QJsonValue elements = object.value(QStringLiteral("elements"));
            if (!elements.isArray())
            {
                errors.push_back(QStringLiteral("elements must be an array"));
                return false;
            }
            for (const QJsonValue& entry : elements.toArray())
            {
                if (!entry.isObject())
                {
                    errors.push_back(QStringLiteral("elements entries must be objects"));
                    continue;
                }
                const QJsonObject elementObject = entry.toObject();
                Element element;
#define ROAD_STRING(key, member) ReadString(elementObject, key, element.member, errors)
                ReadString(elementObject, "element_id", element.m_elementId, errors, true);
                ReadString(elementObject, "owner_pack_id", element.m_ownerPackId, errors, true);
                ROAD_STRING("display_name", m_displayName);
                ROAD_STRING("region_ref", m_regionRef);
                ROAD_STRING("scene_ref", m_sceneRef);
                ROAD_STRING("road_ref", m_roadRef);
                ROAD_STRING("name_ref", m_nameRef);
                ROAD_STRING("segment_ref", m_segmentRef);
                ROAD_STRING("junction_ref", m_junctionRef);
                ROAD_STRING("anchor_ref", m_anchorRef);
                ROAD_STRING("connector_ref", m_connectorRef);
                ROAD_STRING("from_element_ref", m_fromElementRef);
                ROAD_STRING("to_element_ref", m_toElementRef);
                ROAD_STRING("world_node_ref", m_worldNodeRef);
                ROAD_STRING("coordinate_ref", m_coordinateRef);
                ROAD_STRING("notes", m_notes);
#undef ROAD_STRING
                const QJsonValue kind = elementObject.value(QStringLiteral("kind"));
                const QJsonValue state = elementObject.value(QStringLiteral("promotion_state"));
                element.m_kind = kind.isString() ? ReadElementKind(kind.toString()) : ElementKind::Unknown;
                element.m_promotionState = state.isString()
                    ? ReadPromotionState(state.toString())
                    : PromotionState::Unknown;
                ReadStrings(elementObject, "connected_segment_refs", element.m_connectedSegmentRefs, errors);
                ReadStrings(elementObject, "tags", element.m_tags, errors);
                ReadBool(elementObject, "runtime_mutation_allowed", element.m_runtimeMutationAllowed, errors, false);
                ParseGeometry(elementObject, element, errors);
                ParseRequirements(elementObject, element, errors);
                snapshot.m_elements.push_back(AZStd::move(element));
            }
            return errors.empty();
        }

        QJsonObject BuildStarterDocument(const ExtensionAPI::ProfileView& profile)
        {
            QJsonObject element;
            element.insert(QStringLiteral("element_id"), QStringLiteral("road.preview"));
            element.insert(QStringLiteral("owner_pack_id"), QStringLiteral("pack.preview"));
            element.insert(QStringLiteral("kind"), QStringLiteral("road"));
            element.insert(QStringLiteral("promotion_state"), QStringLiteral("raw"));
            element.insert(QStringLiteral("display_name"), QStringLiteral("New road"));
            element.insert(QStringLiteral("region_ref"), QStringLiteral("region.unassigned"));
            element.insert(QStringLiteral("scene_ref"), QStringLiteral("scene.unassigned"));
            element.insert(QStringLiteral("road_ref"), QStringLiteral("road.preview"));
            element.insert(QStringLiteral("connected_segment_refs"), QJsonArray{});
            element.insert(QStringLiteral("geometry"), QJsonArray{});
            element.insert(QStringLiteral("evidence_requirements"), QJsonArray{});
            element.insert(QStringLiteral("tags"), QJsonArray{});
            element.insert(QStringLiteral("runtime_mutation_allowed"), false);

            QJsonObject root;
            root.insert(QStringLiteral("schema_version"), 1);
            root.insert(QStringLiteral("snapshot_id"), QStringLiteral("road-atlas.preview"));
            root.insert(QStringLiteral("profile_id"), ToQString(profile.m_profileId));
            root.insert(QStringLiteral("game_version"), ToQString(profile.m_gameVersion));
            root.insert(QStringLiteral("branch"), ToQString(profile.m_branch));
            root.insert(QStringLiteral("runtime_target"), ToQString(profile.m_runtimeTarget));
            root.insert(
                QStringLiteral("source_fingerprint"),
                QStringLiteral("sha256:") + QString(64, QLatin1Char('0')));
            root.insert(
                QStringLiteral("captured_at_utc"),
                QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
            root.insert(QStringLiteral("elements"), QJsonArray{ element });
            root.insert(QStringLiteral("planning_only"), true);
            root.insert(QStringLiteral("runtime_mutation_allowed"), false);
            return root;
        }
    } // namespace

    RoadAtlasEditorWidget::RoadAtlasEditorWidget(QWidget* parent)
        : QWidget(parent)
    {
        setObjectName(QStringLiteral("TaintedGrailRoadAtlasEditor"));
        setAccessibleName(tr("Tainted Grail Road Atlas Editor"));
        auto* layout = new QVBoxLayout(this);
        auto* heading = new QLabel(tr("Tainted Grail Road Atlas Editor"), this);
        QFont font = heading->font();
        font.setBold(true);
        font.setPointSize(font.pointSize() + 3);
        heading->setFont(font);
        layout->addWidget(heading);

        auto* boundary = new QLabel(
            tr("Author and validate exact-profile Road Atlas schema-1 snapshots. "
               "Documents are stored atomically by the host. Planning output remains inert; "
               "scene mutation, movement, spawning, deployment, and FoA execution are unavailable."),
            this);
        boundary->setWordWrap(true);
        layout->addWidget(boundary);

        auto* group = new QGroupBox(tr("Typed Road Atlas JSON"), this);
        auto* groupLayout = new QVBoxLayout(group);
        m_document = new QTextEdit(group);
        m_document->setAcceptRichText(false);
        m_document->setAccessibleName(tr("Road Atlas snapshot JSON"));
        m_document->setTabChangesFocus(true);
        groupLayout->addWidget(m_document, 1);
        layout->addWidget(group, 1);

        auto* buttons = new QHBoxLayout();
        auto* create = new QPushButton(tr("New"), this);
        auto* load = new QPushButton(tr("Load"), this);
        auto* validate = new QPushButton(tr("Validate"), this);
        m_save = new QPushButton(tr("Save"), this);
        auto* revert = new QPushButton(tr("Revert"), this);
        buttons->addWidget(create);
        buttons->addWidget(load);
        buttons->addWidget(validate);
        buttons->addWidget(m_save);
        buttons->addWidget(revert);
        buttons->addStretch(1);
        layout->addLayout(buttons);

        m_status = new QLabel(tr("Load a workspace document or create a new snapshot."), this);
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

    void RoadAtlasEditorWidget::CreateDocument()
    {
        ExtensionAPI::ProfileView profile;
        AZStd::string error;
        bool loaded = false;
        ExtensionRequestBus::BroadcastResult(
            loaded,
            &ExtensionRequests::GetActiveProfile,
            AZStd::string(ExtensionId),
            profile,
            &error);
        if (!loaded)
        {
            SetStatus(tr("Cannot create a snapshot: %1").arg(ToQString(error)), true);
            return;
        }
        m_lastLoaded = QJsonDocument(BuildStarterDocument(profile)).toJson(QJsonDocument::Indented);
        m_document->setPlainText(QString::fromUtf8(m_lastLoaded));
        SetStatus(tr("Created an unsaved exact-profile starter snapshot."), false);
    }

    void RoadAtlasEditorWidget::LoadDocument()
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
        SetStatus(tr("Loaded the workspace Road Atlas document. Validate before saving changes."), false);
    }

    void RoadAtlasEditorWidget::ValidateDocument()
    {
        Snapshot snapshot;
        QStringList errors;
        const QByteArray payload = m_document->toPlainText().toUtf8();
        ParseSnapshot(payload, snapshot, errors);

        ExtensionAPI::ProfileView profile;
        AZStd::string profileError;
        bool profileLoaded = false;
        ExtensionRequestBus::BroadcastResult(
            profileLoaded,
            &ExtensionRequests::GetActiveProfile,
            AZStd::string(ExtensionId),
            profile,
            &profileError);
        if (!profileLoaded)
        {
            errors.push_back(tr("Exact profile unavailable: %1").arg(ToQString(profileError)));
        }
        if (errors.empty())
        {
            const ValidationResult result = TaintedGrailModdingSDK::RoadAtlasExtension::ValidateSnapshot(
                snapshot,
                ProfileBinding{
                    profile.m_profileId,
                    profile.m_gameVersion,
                    profile.m_branch,
                    profile.m_runtimeTarget });
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
                    tr("Valid planning-only snapshot. Fingerprint: %1")
                        .arg(ToQString(result.m_canonicalFingerprint)),
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

    void RoadAtlasEditorWidget::SaveDocument()
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
        SetStatus(tr("Road Atlas snapshot saved atomically to the active workspace."), false);
    }

    void RoadAtlasEditorWidget::RevertDocument()
    {
        if (m_lastLoaded.isEmpty())
        {
            LoadDocument();
            return;
        }
        m_document->setPlainText(QString::fromUtf8(m_lastLoaded));
        SetStatus(tr("Reverted to the last loaded or saved document."), false);
    }

    void RoadAtlasEditorWidget::SetStatus(const QString& text, bool error)
    {
        m_status->setText(text);
        m_status->setStyleSheet(error ? QStringLiteral("color: #c43b3b;") : QString());
    }
} // namespace RoadAtlasAuthoring
