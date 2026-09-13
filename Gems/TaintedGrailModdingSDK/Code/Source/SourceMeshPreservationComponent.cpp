// Copyright (c) Contributors to the Open 3D Engine Project.
// For complete copyright and license terms please see the LICENSE at the root of this distribution.
// SPDX-License-Identifier: Apache-2.0 OR MIT
#include "SourceMeshPreservationComponent.h"
#include <AzCore/IO/SystemFile.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/JSON/document.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/smart_ptr/make_shared.h>
#include <SceneAPI/SceneCore/Containers/Scene.h>
#include <SceneAPI/SceneCore/DataTypes/GraphData/IMeshData.h>
#include <SceneAPI/SceneCore/DataTypes/GraphData/ICustomPropertyData.h>
#include <SceneAPI/SceneCore/DataTypes/GraphData/IMeshVertexBitangentData.h>
#include <SceneAPI/SceneCore/Utilities/SceneGraphSelector.h>
#include <SceneAPI/SceneData/GraphData/MeshVertexTangentData.h>
#include <cmath>
#include <cstring>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        using Value = rapidjson::Value;
        using Graph = AZ::SceneAPI::Containers::SceneGraph;
        using Mesh = AZ::SceneAPI::DataTypes::IMeshData;
        using Properties = AZ::SceneAPI::DataTypes::ICustomPropertyData;
        constexpr size_t MaxBinaryBytes = 64 * 1024 * 1024;
        constexpr size_t MaxJsonBytes = 1024 * 1024;
        struct View { size_t offset = 0, count = 0, width = 0; };
        AZ::u32 U32(const AZ::u8* bytes)
        {
            return AZ::u32(bytes[0]) | (AZ::u32(bytes[1]) << 8) | (AZ::u32(bytes[2]) << 16) | (AZ::u32(bytes[3]) << 24);
        }
        float Float(const AZ::u8* bytes)
        {
            const AZ::u32 bits = U32(bytes);
            float result;
            std::memcpy(&result, &bits, sizeof(result));
            return result;
        }
        const Value* Member(const Value& value, const char* key)
        {
            if (!value.IsObject()) { return nullptr; }
            const auto found = value.FindMember(key);
            return found == value.MemberEnd() ? nullptr : &found->value;
        }
        bool UInt(const Value& value, const char* key, AZ::u32& output)
        {
            const auto* field = Member(value, key);
            if (!field || !field->IsUint()) { return false; }
            output = field->GetUint(); return true;
        }
        bool Accessor(const Value& doc, const Value& index, size_t dimensions, AZ::u32 componentType, size_t binarySize, View& result)
        {
            const auto* accessors = Member(doc, "accessors");
            const auto* views = Member(doc, "bufferViews");
            if (!index.IsUint() || !accessors || !accessors->IsArray() || index.GetUint() >= accessors->Size() || !views || !views->IsArray()) { return false; }
            const auto& accessor = (*accessors)[index.GetUint()];
            AZ::u32 viewIndex, type, count;
            if (!UInt(accessor,"bufferView",viewIndex) || !UInt(accessor,"componentType",type) || !UInt(accessor,"count",count)
                || viewIndex >= views->Size() || type != componentType || count == 0 || count > 6000000
                || Member(accessor,"sparse") || Member(accessor,"byteOffset")) { return false; }
            const auto* shape = Member(accessor,"type");
            const char* expectedShape = dimensions == 1 ? "SCALAR" : (dimensions == 3 ? "VEC3" : "VEC4");
            if (!shape || !shape->IsString() || std::strcmp(shape->GetString(), expectedShape) != 0) { return false; }
            const auto& view = (*views)[viewIndex];
            AZ::u32 buffer, offset, size;
            if (!UInt(view,"buffer",buffer) || buffer != 0 || !UInt(view,"byteOffset",offset) || !UInt(view,"byteLength",size)
                || Member(view,"byteStride") || size_t(size) != size_t(count)*dimensions*4 || size_t(offset)+size > binarySize) { return false; }
            result = {offset,count,dimensions*4}; return true;
        }
        Properties* MeshProperties(Graph& graph, Graph::NodeIndex mesh)
        {
            for (auto child = graph.GetNodeChild(mesh); child.IsValid(); child = graph.GetNodeSibling(child))
            {
                if (auto* property = azrtti_cast<Properties*>(graph.GetNodeContent(child).get())) { return property; }
            }
            return nullptr;
        }
        bool Reject(int checkLine)
        {
            AZ_TracePrintf("FOA Source Mesh", "Projection preservation check failed at line %d.\n", checkLine);
            return false;
        }
        bool Apply(AZ::SceneAPI::Containers::Scene& scene)
        {
            AZ::IO::SystemFile file;
            if (!file.Open(scene.GetSourceFilename().c_str(), AZ::IO::SystemFile::SF_OPEN_READ_ONLY)) { return Reject(__LINE__); }
            const auto size = file.Length();
            if (size < 28 || size > MaxBinaryBytes+MaxJsonBytes+28) { return Reject(__LINE__); }
            AZStd::vector<AZ::u8> bytes(static_cast<size_t>(size));
            if (file.Read(size, bytes.data()) != size) { return Reject(__LINE__); }
            if (U32(bytes.data()) != 0x46546c67 || U32(bytes.data()+4) != 2 || U32(bytes.data()+8) != size) { return Reject(__LINE__); }
            const size_t jsonSize = U32(bytes.data()+12);
            if (jsonSize > MaxJsonBytes || 28+jsonSize > size || U32(bytes.data()+16) != 0x4e4f534a) { return Reject(__LINE__); }
            const size_t binarySize = U32(bytes.data()+20+jsonSize);
            if (binarySize > MaxBinaryBytes || binarySize != size-28-jsonSize || U32(bytes.data()+24+jsonSize) != 0x004e4942) { return Reject(__LINE__); }
            const auto* binary = bytes.data()+28+jsonSize;
            rapidjson::Document doc;
            doc.Parse(reinterpret_cast<const char*>(bytes.data()+20),jsonSize);
            if (doc.HasParseError()) { return Reject(__LINE__); }
            const auto* asset = Member(doc,"asset");
            const auto* extras = asset ? Member(*asset,"extras") : nullptr;
            const auto* schema = extras ? Member(*extras,"foaSourceMeshProjection") : nullptr;
            if (!schema || !schema->IsUint() || schema->GetUint() != 1) { return Reject(__LINE__); }
            const auto* nodes = Member(doc,"nodes");
            const auto* scenes = Member(doc,"scenes");
            const auto* buffers = Member(doc,"buffers");
            AZ::u32 activeScene, nodeMesh, declaredBytes;
            if (!UInt(doc,"scene",activeScene) || activeScene != 0
                || !nodes || !nodes->IsArray() || nodes->Size() != 1
                || !scenes || !scenes->IsArray() || scenes->Size() != 1
                || !buffers || !buffers->IsArray() || buffers->Size() != 1
                || !UInt((*nodes)[0],"mesh",nodeMesh) || nodeMesh != 0
                || !UInt((*buffers)[0],"byteLength",declaredBytes) || declaredBytes != binarySize
                || Member((*buffers)[0],"uri")) { return Reject(__LINE__); }
            const auto* roots = Member((*scenes)[0],"nodes");
            const auto& node = (*nodes)[0];
            const auto* matrix = Member(node,"matrix");
            constexpr double SourceRoot[16] = {-1,0,0,0, 0,0,1,0, 0,1,0,0, 0,0,0,1};
            if (!roots || !roots->IsArray() || roots->Size() != 1 || !(*roots)[0].IsUint() || (*roots)[0].GetUint() != 0
                || !matrix || !matrix->IsArray() || matrix->Size() != 16
                || Member(node,"translation") || Member(node,"rotation") || Member(node,"scale")
                || Member(node,"children") || Member(node,"skin")) { return Reject(__LINE__); }
            for (size_t i=0; i<16; ++i)
            {
                const auto& entry = (*matrix)[static_cast<rapidjson::SizeType>(i)];
                if (!entry.IsNumber() || entry.GetDouble() != SourceRoot[i]) { return Reject(__LINE__); }
            }
            const auto* meshes = Member(doc,"meshes");
            if (!meshes || !meshes->IsArray() || meshes->Size() != 1) { return Reject(__LINE__); }
            const auto* primitives = Member((*meshes)[0],"primitives");
            if (!primitives || !primitives->IsArray() || primitives->Empty() || primitives->Size() > 4096) { return Reject(__LINE__); }
            const auto* attributes = Member((*primitives)[0],"attributes");
            const auto* position = attributes ? Member(*attributes,"POSITION") : nullptr;
            View positions, tangents;
            if (!position || !Accessor(doc,*position,3,5126,binarySize,positions) || positions.count > 1000000) { return Reject(__LINE__); }
            const auto* tangent = Member(*attributes,"TANGENT");
            if (tangent && (!Accessor(doc,*tangent,4,5126,binarySize,tangents) || tangents.count != positions.count)) { return Reject(__LINE__); }
            AZStd::vector<View> indexViews;
            size_t faceCount = 0;
            for (const auto& primitive : primitives->GetArray())
            {
                const auto* indices = Member(primitive,"indices");
                const auto* sameAttributes = Member(primitive,"attributes");
                AZ::u32 mode, material; View view;
                if (!indices || !sameAttributes || *sameAttributes != *attributes || !UInt(primitive,"mode",mode) || mode != 4
                    || !UInt(primitive,"material",material) || material != indexViews.size()
                    || !Accessor(doc,*indices,1,5125,binarySize,view) || view.count%3 != 0) { return Reject(__LINE__); }
                faceCount += view.count/3; indexViews.push_back(view);
            }
            if (faceCount > 2000000) { return Reject(__LINE__); }
            auto& graph = scene.GetGraph();
            if (graph.GetNodeCount() > 100000) { return Reject(__LINE__); }
            Graph::NodeIndex original;
            Mesh* mesh = nullptr;
            for (size_t i=0; i<graph.GetNodeCount(); ++i)
            {
                const auto index = graph.ConvertToNodeIndex(graph.GetContentStorage().begin() + i);
                auto* candidate = azrtti_cast<Mesh*>(graph.GetNodeContent(index).get());
                if (!candidate) { continue; }
                auto* property = MeshProperties(graph,index);
                if (property && property->GetPropertyMap().contains(AZ::SceneAPI::Utilities::OriginalUnoptimizedMeshPropertyMapKey)) { continue; }
                if (mesh) { return Reject(__LINE__); }
                original=index; mesh=candidate;
            }
            if (!mesh || mesh->GetVertexCount() == 0 || mesh->GetVertexCount() > 6000000
                || mesh->GetFaceCount() != faceCount) { return Reject(__LINE__); }
            // Assimp can expand indexed triangles into face-corner vertices. Bind
            // through ordered source faces, never through proximity or vertex order.
            constexpr AZ::u32 UnboundVertex = 0xffffffff;
            AZStd::vector<AZ::u32> sourceVertices(mesh->GetVertexCount(), UnboundVertex);
            size_t face = 0;
            for (size_t slot=0; slot<indexViews.size(); ++slot)
            {
                const auto& view = indexViews[slot];
                for (size_t i=0; i<view.count; i+=3,++face)
                {
                    const auto sourceFace = mesh->GetFaceInfo(static_cast<unsigned>(face));
                    if (mesh->GetFaceMaterialId(static_cast<unsigned>(face)) != slot) { return Reject(__LINE__); }
                    for (size_t corner=0; corner<3; ++corner)
                    {
                        const auto vertex = U32(binary+view.offset+(i+corner)*4);
                        const auto importedVertex = sourceFace.vertexIndex[corner];
                        if (vertex >= positions.count || importedVertex >= sourceVertices.size()) { return Reject(__LINE__); }
                        auto& bound = sourceVertices[importedVertex];
                        if (bound != UnboundVertex && bound != vertex) { return Reject(__LINE__); }
                        const auto* p = binary+positions.offset+vertex*positions.width;
                        const AZ::Vector3 expected(Float(p),Float(p+4),Float(p+8));
                        if (!expected.IsFinite() || mesh->GetPosition(importedVertex) != expected) { return Reject(__LINE__); }
                        bound = vertex;
                    }
                }
            }
            for (const auto vertex : sourceVertices)
            {
                if (vertex == UnboundVertex) { return Reject(__LINE__); }
            }
            AZStd::shared_ptr<AZ::SceneData::GraphData::MeshVertexTangentData> sourceTangents;
            if (tangent)
            {
                sourceTangents = AZStd::make_shared<AZ::SceneData::GraphData::MeshVertexTangentData>();
                sourceTangents->ReserveContainerSpace(mesh->GetVertexCount());
                sourceTangents->SetTangentSetIndex(0);
                for (size_t i=0; i<mesh->GetVertexCount(); ++i)
                {
                    const auto* p = binary+tangents.offset+sourceVertices[i]*tangents.width;
                    const AZ::Vector4 value(Float(p),Float(p+4),Float(p+8),Float(p+12));
                    if (!value.IsFinite() || (value.GetW()!=1.f && value.GetW()!=-1.f) || value.GetAsVector3().GetLengthSq()==0.f) { return Reject(__LINE__); }
                    sourceTangents->AppendTangent(value);
                }
            }
            // Export the verified original, avoiding the host's tolerance-based weld.
            if (auto* property = MeshProperties(graph,original)) { property->GetPropertyMap().erase(AZ::SceneAPI::Utilities::OptimizedMeshPropertyMapKey); }
            // Tangent signs come from source. Bitangents are not source channels and
            // remain absent; no fallback direction is invented for a degenerate frame.
            for (auto child=graph.GetNodeChild(original); child.IsValid(); child=graph.GetNodeSibling(child))
            {
                const auto content=graph.GetNodeContent(child);
                if (azrtti_istypeof<AZ::SceneAPI::DataTypes::IMeshVertexTangentData>(content.get())
                    || azrtti_istypeof<AZ::SceneAPI::DataTypes::IMeshVertexBitangentData>(content.get()))
                {
                    graph.SetContent(child,nullptr);
                }
            }
            if (sourceTangents && !graph.AddChild(original,"foa_source_tangents",sourceTangents).IsValid()) { return Reject(__LINE__); }
            AZ_TracePrintf("FOA Source Mesh", "Preserved %u original vertices, %u faces, and source tangent presence/signs.\n",mesh->GetVertexCount(),mesh->GetFaceCount());
            return true;
        }
    }
    SourceMeshPreservationComponent::SourceMeshPreservationComponent() { BindToCall(&SourceMeshPreservationComponent::Preserve); }
    void SourceMeshPreservationComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serialization=azrtti_cast<AZ::SerializeContext*>(context))
        {
            serialization->Class<SourceMeshPreservationComponent,AZ::SceneAPI::SceneCore::GenerationComponent>()->Version(1);
        }
    }
    AZ::SceneAPI::Events::ProcessingResult SourceMeshPreservationComponent::Preserve(AZ::SceneAPI::Events::PostGenerateEventContext& context)
    {
        using Result=AZ::SceneAPI::Events::ProcessingResult;
        if (!context.GetScene().GetSourceFilename().ends_with("_foamesh.glb")) { return Result::Ignored; }
        if (!Apply(context.GetScene()))
        {
            AZ_Error("FOA Source Mesh",false,"Source projection framing or exact native mesh correspondence failed; export blocked.");
            return Result::Failure;
        }
        return Result::Success;
    }
}
