/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#include "SourceScenePlacementComponent.h"
#include <AzCore/Component/TransformBus.h>
#include <AzCore/Interface/Interface.h>
#include <AzToolsFramework/Prefab/PrefabPublicInterface.h>
#include <AzCore/JSON/document.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Script/ScriptContextAttributes.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/containers/set.h>
#include <charconv>
#include <cmath>
namespace TaintedGrailModdingSDK
{
    namespace
    {
        using Json = rapidjson::Value;
        bool Keys(const Json& value, AZStd::initializer_list<const char*> keys)
        {
            if (!value.IsObject() || value.MemberCount() != keys.size()) { return false; }
            AZStd::set<AZStd::string> seen;
            for (auto it = value.MemberBegin(); it != value.MemberEnd(); ++it)
            {
                if (!seen.insert(AZStd::string(it->name.GetString(), it->name.GetStringLength())).second) { return false; }
            }
            for (const char* key : keys) { if (!value.HasMember(key)) { return false; } }
            return true;
        }
        bool Text(const Json& value, size_t maximum)
        {
            return value.IsString() && value.GetStringLength() > 0 && value.GetStringLength() <= maximum &&
                !memchr(value.GetString(), 0, value.GetStringLength());
        }
        bool Hex(const Json& value, AZ::u32 length)
        {
            if (!Text(value, length) || value.GetStringLength() != length) { return false; }
            for (AZ::u32 i = 0; i < length; ++i)
            {
                const char c = value.GetString()[i];
                if (!(c >= '0' && c <= '9') && !(c >= 'a' && c <= 'f')) { return false; }
            }
            return true;
        }
        bool Digest(const Json& value) { return Hex(value, 64); }
        bool PathId(const Json& value)
        {
            if (!Text(value, 20)) { return false; }
            const char* start = value.GetString(); const char* end = start + value.GetStringLength();
            if (*start == '-' && ++start == end) { return false; }
            if (*start < '1' || *start > '9') { return false; }
            for (const char* p = start; p != end; ++p) { if (*p < '0' || *p > '9') { return false; } }
            AZ::s64 parsed = 0;
            const auto result = std::from_chars(value.GetString(), end, parsed);
            return result.ec == std::errc{} && result.ptr == end && parsed != 0;
        }
        bool Matrix(const Json& value, AZ::Matrix4x4& output)
        {
            if (!value.IsArray() || value.Size() != 16) { return false; }
            float decoded[16];
            for (AZ::u32 i = 0; i < 16; ++i)
            {
                if (!value[i].IsUint()) { return false; }
                const AZ::u32 bits = value[i].GetUint(); memcpy(&decoded[i], &bits, 4);
                if (!std::isfinite(decoded[i])) { return false; }
            }
            if (decoded[12] != 0 || decoded[13] != 0 || decoded[14] != 0 || decoded[15] != 1) { return false; }
            output = AZ::Matrix4x4::CreateFromRowMajorFloat16(decoded);
            return true;
        }
        bool Vector(const Json& value, AZ::Vector3& output)
        {
            if (!value.IsArray() || value.Size() != 3) { return false; }
            for (AZ::u32 i = 0; i < 3; ++i)
            {
                if (!value[i].IsNumber() || !std::isfinite(value[i].GetDouble()) || std::abs(value[i].GetDouble()) > 3.402823466e38) { return false; }
                output.SetElement(i, static_cast<float>(value[i].GetDouble()));
            }
            return true;
        }
        AZ::Aabb Bounds(const AZ::Matrix4x4& matrix, const AZ::Aabb& local)
        {
            if (!local.IsValid() || !matrix.IsFinite()) { return AZ::Aabb::CreateNull(); }
            AZ::Aabb output = AZ::Aabb::CreateNull();
            for (int corner = 0; corner < 8; ++corner)
            {
                AZ::Vector3 p;
                for (int axis = 0; axis < 3; ++axis) { p.SetElement(axis, (corner & (1 << axis) ? local.GetMax() : local.GetMin()).GetElement(axis)); }
                const AZ::Vector3 transformed = matrix * p;
                if (!transformed.IsFinite()) { return AZ::Aabb::CreateNull(); }
                output.AddPoint(transformed);
            }
            return output;
        }
        bool Parse(const AZStd::string& descriptor, AZ::Matrix4x4& world, AZ::Aabb& bounds, AZ::Vector3& anchor)
        {
            if (descriptor.empty() || descriptor.size() > 8192) { return false; }
            rapidjson::Document doc;
            doc.Parse<rapidjson::kParseIterativeFlag | rapidjson::kParseValidateEncodingFlag>(descriptor.data(), descriptor.size());
            if (doc.HasParseError() || !doc.IsObject() || !doc.HasMember("schema_version") || !doc["schema_version"].IsUint()) { return false; }
            const auto version = doc["schema_version"].GetUint();
            if (version == 1 || version == 3)
            {
                if (!Keys(doc, {"schema_version", "profile", "identity", "source_parent", "source_local_bits", "source_world_bits", "local_bounds"})) { return false; }
            }
            else if (version == 2)
            {
                if (!Keys(doc, {"schema_version", "profile", "identity", "source_parent", "source_local_bits", "source_world_bits", "local_bounds", "native_anchor_bits"})) { return false; }
            }
            else { return false; }
            if (!Text(doc["profile"], 64) || AZStd::string_view(doc["profile"].GetString()) != "unity-6000.0.64f1") { return false; }
            const auto& id = doc["identity"];
            const auto& parent = doc["source_parent"];
            if (version == 3)
            {
                if (!Keys(id, {"archive_sha256", "member_guid", "member_sha256", "instance_ordinal", "record_sha256"}) ||
                    !Digest(id["archive_sha256"]) || !Hex(id["member_guid"], 32) || !Digest(id["member_sha256"]) ||
                    !Digest(id["record_sha256"]) || !id["instance_ordinal"].IsUint() || id["instance_ordinal"].GetUint() >= 1000000 ||
                    !parent.IsNull()) { return false; }
            }
            else
            {
                if (!Keys(id, {"bundle_sha256", "serialized_file", "path_id", "gameobject_id", "record_sha256"}) ||
                    !Digest(id["bundle_sha256"]) || !Digest(id["record_sha256"]) || !Text(id["serialized_file"], 256) ||
                    !PathId(id["path_id"]) || !PathId(id["gameobject_id"])) { return false; }
                if (!parent.IsNull() && (!Keys(parent, {"serialized_file", "path_id"}) || !Text(parent["serialized_file"], 256) || !PathId(parent["path_id"]))) { return false; }
                if (!parent.IsNull() && AZStd::string_view(parent["serialized_file"].GetString()) == id["serialized_file"].GetString() &&
                    AZStd::string_view(parent["path_id"].GetString()) == id["path_id"].GetString()) { return false; }
            }
            AZ::Matrix4x4 local, source;
            if (!Matrix(doc["source_local_bits"], local) || !Matrix(doc["source_world_bits"], source)) { return false; }
            if (version == 3)
            {
                // Archive instances store world matrices; they have no source Transform parent.
                for (AZ::u32 i = 0; i < 16; ++i)
                {
                    if (doc["source_local_bits"][i].GetUint() != doc["source_world_bits"][i].GetUint()) { return false; }
                }
            }
            // Exact coordinate permutation preserves all captured coefficients, including signed zero.
            constexpr int order[] = {0, 2, 1, 3};
            for (int r = 0; r < 4; ++r) { for (int c = 0; c < 4; ++c) { world.SetElement(r, c, source.GetElement(order[r], order[c])); } }
            anchor = world.GetTranslation();
            if (version == 2)
            {
                const auto& value = doc["native_anchor_bits"];
                if (!value.IsArray() || value.Size() != 3) { return false; }
                for (AZ::u32 i = 0; i < 3; ++i)
                {
                    if (!value[i].IsUint()) { return false; }
                    const AZ::u32 bits = value[i].GetUint(); float number = 0; memcpy(&number, &bits, 4);
                    if (!std::isfinite(number)) { return false; }
                    anchor.SetElement(i, number);
                }
            }
            bounds = AZ::Aabb::CreateNull();
            if (!doc["local_bounds"].IsNull())
            {
                const auto& box = doc["local_bounds"]; AZ::Vector3 minimum, maximum;
                if (!Keys(box, {"min", "max"}) || !Vector(box["min"], minimum) || !Vector(box["max"], maximum) || !minimum.IsLessEqualThan(maximum)) { return false; }
                bounds = AZ::Aabb::CreateFromMinMax(AZ::Vector3(minimum.GetX(), minimum.GetZ(), minimum.GetY()), AZ::Vector3(maximum.GetX(), maximum.GetZ(), maximum.GetY()));
                if (!Bounds(world, bounds).IsValid()) { return false; }
            }
            return true;
        }
    }
    void SourceScenePlacementComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* sc = azrtti_cast<AZ::SerializeContext*>(context))
        {
            sc->Class<SourceScenePlacementComponent, EditorComponentBase>()->Version(1)->Field("Source", &SourceScenePlacementComponent::m_source);
            if (auto* ec = sc->GetEditContext())
            {
                ec->Class<SourceScenePlacementComponent>("Source scene placement", "Retains the source affine transform. Use the native Transform to edit placement.")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "FOA")
                    ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC_CE("Game"))
                    ->DataElement(AZ::Edit::UIHandlers::Default, &SourceScenePlacementComponent::m_source, "Source binding", "Private immutable source identity and captured transform.")
                    ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::Hide);
            }
        }
        if (auto* bc = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            bc->EBus<SourceScenePlacementBus>("SourceScenePlacementBus")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Automation)
                ->Attribute(AZ::Script::Attributes::Module, "foa")
                ->Event("BindSource", &SourceScenePlacementRequests::BindSource)
                ->Event("GetSource", &SourceScenePlacementRequests::GetSource)
                ->Event("CommitAssemblyState", &SourceScenePlacementRequests::CommitAssemblyState)
                ->Event("GetStatus", &SourceScenePlacementRequests::GetStatus)
                ->Event("GetWorldMatrix", &SourceScenePlacementRequests::GetWorldMatrix)
                ->Event("GetWorldMatrixBits", &SourceScenePlacementRequests::GetWorldMatrixBits)
                ->Event("GetWorldBounds", &SourceScenePlacementRequests::GetWorldBounds);
        }
    }
    void SourceScenePlacementComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& s) { s.push_back(AZ_CRC_CE("SourceScenePlacementService")); }
    void SourceScenePlacementComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& s) { s.push_back(AZ_CRC_CE("SourceScenePlacementService")); }
    void SourceScenePlacementComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& s) { s.push_back(AZ_CRC_CE("TransformService")); }
    void SourceScenePlacementComponent::Activate()
    {
        EditorComponentBase::Activate();
        m_valid = Parse(m_source, m_originalWorld, m_localBounds, m_originalAnchor);
        SourceScenePlacementBus::Handler::BusConnect(GetEntityId());
        AzToolsFramework::EditorComponentSelectionRequestsBus::Handler::BusConnect(GetEntityId());
    }
    void SourceScenePlacementComponent::Deactivate()
    {
        AzToolsFramework::EditorComponentSelectionRequestsBus::Handler::BusDisconnect();
        SourceScenePlacementBus::Handler::BusDisconnect();
        EditorComponentBase::Deactivate();
    }
    bool SourceScenePlacementComponent::BindSource(const AZStd::string& descriptor)
    {
        if (!m_source.empty()) { return descriptor == m_source && m_valid; }
        AZ::Matrix4x4 world; AZ::Aabb bounds; AZ::Vector3 originalAnchor;
        if (!Parse(descriptor, world, bounds, originalAnchor)) { return false; }
        AZ::Transform anchor = AZ::Transform::CreateIdentity();
        AZ::TransformBus::EventResult(anchor, GetEntityId(), &AZ::TransformInterface::GetWorldTM);
        if (anchor != AZ::Transform::CreateTranslation(originalAnchor)) { return false; }
        AZ::EntityId nativeParent;
        AZ::TransformBus::EventResult(nativeParent, GetEntityId(), &AZ::TransformInterface::GetParentId);
        AZStd::string parentSource;
        SourceScenePlacementBus::EventResult(parentSource, nativeParent, &SourceScenePlacementRequests::GetSource);
        rapidjson::Document sourceDoc, parentDoc;
        sourceDoc.Parse<rapidjson::kParseIterativeFlag>(descriptor.data(), descriptor.size());
        if (sourceDoc["schema_version"].GetUint() == 2)
        {
            AZ::Transform parentWorld = AZ::Transform::CreateIdentity();
            if (nativeParent.IsValid()) { AZ::TransformBus::EventResult(parentWorld, nativeParent, &AZ::TransformInterface::GetWorldTM); }
            const auto parentPosition = parentWorld.GetTranslation();
            if (parentWorld != AZ::Transform::CreateTranslation(parentPosition)) { return false; }
            for (int axis = 0; axis < 3; ++axis)
            {
                // Keep the native float32 subtraction/addition boundary; do not algebraically cancel it.
                volatile float localPosition = world.GetTranslation().GetElement(axis) - parentPosition.GetElement(axis);
                const float reconstructed = parentPosition.GetElement(axis) + localPosition;
                if (!std::isfinite(reconstructed) || reconstructed != originalAnchor.GetElement(axis)) { return false; }
            }
        }
        const auto& expectedParent = sourceDoc["source_parent"];
        if (expectedParent.IsNull())
        {
            if (!parentSource.empty()) { return false; }
        }
        else
        {
            parentDoc.Parse<rapidjson::kParseIterativeFlag>(parentSource.data(), parentSource.size());
            if (parentDoc.HasParseError() || !parentDoc.IsObject() || !parentDoc.HasMember("identity")) { return false; }
            const auto& actualParent = parentDoc["identity"];
            if (!actualParent.IsObject() || !actualParent.HasMember("bundle_sha256") || !actualParent["bundle_sha256"].IsString() ||
                AZStd::string_view(actualParent["bundle_sha256"].GetString()) != sourceDoc["identity"]["bundle_sha256"].GetString()) { return false; }
            for (const char* key : {"serialized_file", "path_id"})
            {
                if (!actualParent.IsObject() || !actualParent.HasMember(key) || !actualParent[key].IsString() ||
                    AZStd::string_view(actualParent[key].GetString()) != expectedParent[key].GetString()) { return false; }
            }
        }
        m_source = descriptor; m_originalWorld = world; m_originalAnchor = originalAnchor; m_localBounds = bounds; m_valid = true; SetDirty();
        return true;
    }
    bool SourceScenePlacementComponent::CommitAssemblyState()
    {
        if (!m_valid) { return false; }
        auto* prefab = AZ::Interface<AzToolsFramework::Prefab::PrefabPublicInterface>::Get();
        AzToolsFramework::UndoSystem::URSequencePoint* undo = nullptr;
        AzToolsFramework::ToolsApplicationRequestBus::BroadcastResult(
            undo, &AzToolsFramework::ToolsApplicationRequests::GetCurrentUndoBatch);
        if (!prefab || !undo) { return false; }
        // Publish the complete new entity before the importer yields to prefab propagation.
        // Otherwise a later creation can cache a template that still lacks this binding.
        if (!prefab->GenerateUndoNodesForEntityChangeAndUpdateCache(GetEntityId(), undo).IsSuccess()) { return false; }
        AzToolsFramework::ToolsApplicationRequestBus::Broadcast(
            &AzToolsFramework::ToolsApplicationRequests::RemoveDirtyEntity, GetEntityId());
        return true;
    }
    AZStd::string SourceScenePlacementComponent::GetSource() const { return m_source; }
    AZStd::string SourceScenePlacementComponent::GetStatus() const
    {
        return m_source.empty() ? "UNBOUND" : !m_valid ? "FAILED: invalid source binding" :
            !GetWorldMatrix().IsFinite() ? "FAILED: nonfinite placement" : "READY";
    }
    AZ::Matrix4x4 SourceScenePlacementComponent::GetWorldMatrix() const
    {
        if (!m_valid) { return AZ::Matrix4x4::CreateIdentity(); }
        AZ::Transform anchor = AZ::Transform::CreateIdentity();
        AZ::TransformBus::EventResult(anchor, GetEntityId(), &AZ::TransformInterface::GetWorldTM);
        const auto baseline = AZ::Transform::CreateTranslation(m_originalAnchor);
        if (anchor == baseline) { return m_originalWorld; }
        // Left-compose the editor's placement delta with the full source affine matrix.
        // Do not decompose the source matrix into the host's uniform-scale Transform.
        AZ::Matrix4x4 linear = m_originalWorld;
        linear.SetTranslation(m_originalWorld.GetTranslation() - m_originalAnchor);
        return AZ::Matrix4x4::CreateFromTransform(anchor) * linear;
    }
    AZStd::vector<AZ::u32> SourceScenePlacementComponent::GetWorldMatrixBits() const
    {
        // Float-to-double script marshalling can flush subnormal coefficients.
        // Copy the actual native matrix storage through an integer-only boundary.
        float values[16]; GetWorldMatrix().StoreToRowMajorFloat16(values);
        AZStd::vector<AZ::u32> bits(16);
        memcpy(bits.data(), values, sizeof(values));
        return bits;
    }
    AZ::Aabb SourceScenePlacementComponent::GetWorldBounds() const { return m_valid ? Bounds(GetWorldMatrix(), m_localBounds) : AZ::Aabb::CreateNull(); }
    AZ::Aabb SourceScenePlacementComponent::GetEditorSelectionBoundsViewport(const AzFramework::ViewportInfo&) { return GetWorldBounds(); }
}
