/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#include "SourceShaderRenderComponent.h"
#include "SourceScenePlacementComponent.h"
#include <AzCore/Asset/AssetManager.h>
#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/Asset/AssetDataStream.h>
#include <AzCore/Asset/AssetTypeInfoBus.h>
#include <AzCore/JSON/document.h>
#include <AzCore/JSON/stringbuffer.h>
#include <AzCore/JSON/writer.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Script/ScriptContextAttributes.h>
#include <AzCore/std/containers/set.h>
#include <AzCore/std/chrono/chrono.h>
#include <AzFramework/Asset/AssetCatalogBus.h>
#include <Atom/RPI.Public/ViewportContext.h>
#include <Atom/RPI.Public/Scene.h>
#include <Atom/RPI.Public/View.h>
#include <Atom/RPI.Public/DynamicDraw/DynamicDrawInterface.h>
#include <Atom/RPI.Public/Shader/Shader.h>
#include <Atom/RPI.Public/Shader/ShaderResourceGroup.h>
#include <Atom/RPI.Public/Buffer/BufferSystemInterface.h>
#include <Atom/RPI.Public/Buffer/Buffer.h>
#include <Atom/RPI.Public/Image/StreamingImage.h>
#include <Atom/RHI/DrawPacketBuilder.h>
#include <Atom/RHI/DeviceDrawArguments.h>
#include <Atom/RHI/PipelineState.h>
#include <Atom/RHI.Reflect/InputStreamLayoutBuilder.h>
#include <cmath>
#include <limits>
#include <QCryptographicHash>
namespace TaintedGrailModdingSDK
{
    namespace
    {
        using Bytes = AZStd::vector<AZ::u8>;
        using Json = rapidjson::Value;
        using Clock = AZStd::chrono::steady_clock;
        constexpr size_t MaxDescriptor = 16 * 1024 * 1024;
        constexpr AZ::u32 MaxInstancesPerDraw = 4096;
        constexpr size_t MaxInstanceIndexWork = 2 * 1024 * 1024;
        constexpr size_t MaxEntityDraws = 1024;
        constexpr size_t MaxDrawsPerEntity = 128;
        constexpr size_t MaxGeometryBuffers = 32768;
        constexpr size_t MaxTickVisits = 64;
        constexpr size_t MaxSamplerReservations = 512;
        constexpr size_t MaxResidentBytes = 64 * 1024 * 1024;
        bool Keys(const Json& value, AZStd::initializer_list<const char*> keys)
        {
            if (!value.IsObject() || value.MemberCount() != keys.size()) { return false; }
            AZStd::set<AZStd::string> seen;
            for (const auto& member : value.GetObject())
            {
                bool known = false;
                for (const auto* key : keys) { known |= member.name == key; }
                if (!known || !seen.emplace(member.name.GetString(),member.name.GetStringLength()).second) { return false; }
            }
            return true;
        }
        bool Uint(const Json& value, AZ::u32 maximum, AZ::u32 minimum = 0)
        { return value.IsUint() && value.GetUint() >= minimum && value.GetUint() <= maximum; }
        bool Text(const Json& value, size_t maximum)
        { return value.IsString() && value.GetStringLength() && value.GetStringLength() <= maximum && !memchr(value.GetString(),0,value.GetStringLength()); }
        bool AssetPath(const Json& value, AZStd::string_view suffix)
        {
            if (!Text(value,1024)) { return false; }
            const AZStd::string_view path(value.GetString(),value.GetStringLength());
            return path.ends_with(suffix) && path.front() != '/' && path.find("..") == path.npos &&
                path.find(':') == path.npos && path.find('\\') == path.npos;
        }
        Bytes Hex(const Json& value, size_t maximum)
        {
            if (!Text(value,maximum*2) || value.GetStringLength()%2) { return {}; }
            Bytes output; output.reserve(value.GetStringLength()/2);
            auto nibble = [](char c) -> int { return c >= '0' && c <= '9' ? c-'0' : c >= 'a' && c <= 'f' ? c-'a'+10 : -1; };
            for (size_t i = 0; i < value.GetStringLength(); i += 2)
            {
                const int a = nibble(value.GetString()[i]), b = nibble(value.GetString()[i+1]);
                if (a < 0 || b < 0) { return {}; }
                output.push_back(AZ::u8(a*16+b));
            }
            return output;
        }
        template<class T> AZ::Data::Asset<T> QueueAsset(const AZStd::string& path)
        {
            AZ::Data::AssetId id;
            AZ::Data::AssetCatalogRequestBus::BroadcastResult(id,&AZ::Data::AssetCatalogRequests::GetAssetIdByPath,path.c_str(),azrtti_typeid<T>(),false);
            return id.IsValid() ? AZ::Data::AssetManager::Instance().GetAsset<T>(id,AZ::Data::AssetLoadBehavior::PreLoad) : AZ::Data::Asset<T>{};
        }
        AZ::Data::Instance<AZ::RPI::Buffer> Buffer(const Bytes& bytes, AZ::RPI::CommonBufferPoolType pool, AZ::u32 stride, bool raw = false)
        {
            AZ::RPI::CommonBufferDescriptor descriptor;
            descriptor.m_poolType = pool; descriptor.m_bufferName = "FOA source draw";
            descriptor.m_byteCount = bytes.size(); descriptor.m_bufferData = bytes.data(); descriptor.m_elementSize = stride;
            if (raw) { descriptor.m_elementFormat = AZ::RHI::Format::R32_UINT; }
            auto* system = AZ::RPI::BufferSystemInterface::Get();
            return system ? system->CreateBufferFromCommonPool(descriptor) : nullptr;
        }
        bool Sampler(const Json& value, AZ::RHI::SamplerState& sampler)
        {
            if (!value.IsArray() || value.Size() != 14) { return false; }
            const AZ::u32 maxima[]{16,1,1,1,1,3,7,4,4,4};
            for (AZ::u32 i = 0; i < 10; ++i) { if (!Uint(value[i],maxima[i],i==0 ? 1 : 0)) { return false; } }
            for (AZ::u32 i = 10; i < 13; ++i)
            { if (!value[i].IsNumber() || !std::isfinite(value[i].GetDouble()) || std::abs(value[i].GetDouble()) > double(std::numeric_limits<float>::max())) { return false; } }
            if (!Uint(value[13],2) || value[10].GetDouble() > value[11].GetDouble()) { return false; }
            sampler.m_anisotropyMax=value[0].GetUint(); sampler.m_anisotropyEnable=value[1].GetUint();
            sampler.m_filterMin=static_cast<AZ::RHI::FilterMode>(value[2].GetUint());
            sampler.m_filterMag=static_cast<AZ::RHI::FilterMode>(value[3].GetUint());
            sampler.m_filterMip=static_cast<AZ::RHI::FilterMode>(value[4].GetUint());
            sampler.m_reductionType=static_cast<AZ::RHI::ReductionType>(value[5].GetUint());
            sampler.m_comparisonFunc=static_cast<AZ::RHI::ComparisonFunc>(value[6].GetUint());
            sampler.m_addressU=static_cast<AZ::RHI::AddressMode>(value[7].GetUint());
            sampler.m_addressV=static_cast<AZ::RHI::AddressMode>(value[8].GetUint());
            sampler.m_addressW=static_cast<AZ::RHI::AddressMode>(value[9].GetUint());
            sampler.m_mipLodMin=float(value[10].GetDouble()); sampler.m_mipLodMax=float(value[11].GetDouble());
            sampler.m_mipLodBias=float(value[12].GetDouble()); sampler.m_borderColor=static_cast<AZ::RHI::BorderColor>(value[13].GetUint());
            return true;
        }
    }
    struct SourceShaderRenderComponent::SharedGeometryBuffer
    {
        AZStd::string key;
        Bytes bytes;
        AZ::Data::Instance<AZ::RPI::Buffer> buffer;
    };
    struct SourceShaderRenderComponent::SharedStage
    {
        AZStd::string key, signature;
        AZ::Data::Instance<AZ::RPI::ShaderResourceGroup> srg;
        size_t samplers = 0;
    };
    struct SourceShaderRenderComponent::Draw
    {
        struct Stream { AZ::RHI::ShaderSemantic semantic; AZ::u32 components{},type{}; Bytes bytes; AZStd::shared_ptr<SharedGeometryBuffer> shared; };
        struct Constant { AZ::u32 slot{}; Bytes bytes; AZ::Data::Instance<AZ::RPI::Buffer> buffer; };
        struct DataBuffer { AZ::u32 slot{},type{},stride{}; Bytes bytes; };
        struct Matrix { AZ::u32 stage{}, slot{}, offset{}, value{}, size{64}; };
        struct Image { AZ::u32 slot{}; AZStd::string path; AZ::Data::Asset<AZ::RPI::StreamingImageAsset> asset; };
        struct Sampling { AZ::u32 slot{}; AZ::RHI::SamplerState state; };
        struct Stage { AZStd::string signature; AZStd::vector<Constant> constants; AZStd::vector<DataBuffer> dataBuffers; AZStd::vector<Image> images; AZStd::vector<Sampling> samplers; };
        AZStd::string shaderPath;
        AZ::Data::Asset<AZ::RPI::ShaderAsset> shaderAsset;
        AZ::Data::Instance<AZ::RPI::Shader> shader;
        AZStd::vector<Stream> streams;
        Bytes indices;
        AZStd::shared_ptr<SharedGeometryBuffer> sharedIndices;
        size_t scheduleIndex = 0, ownedSamplers = 0;
        AZStd::shared_ptr<SharedStage> sharedStages[2];
        Stage stages[2];
        AZ::u32 vertexCount{},sortKey{},instanceCount{1};
        AZ::EntityId entity;
        AZStd::string status = "LOADING";
        AZStd::vector<Matrix> matrices;
        size_t residentBytes = 0, dataBufferBytes = 0;
        bool dirty = false, visible = true, camera = false;
        Clock::time_point started = Clock::now();
        AZStd::vector<AZ::Data::Instance<AZ::RPI::Buffer>> buffers;
        AZStd::vector<AZ::Data::Instance<AZ::RPI::StreamingImage>> images;
        AZ::Data::Instance<AZ::RPI::ShaderResourceGroup> srgs[2];
        AZStd::unique_ptr<AZ::RHI::GeometryView> geometry;
        AZ::RHI::ConstPtr<AZ::RHI::DrawPacket> packet;
        AZ::RPI::ScenePtr scene;
        bool Parse(const AZStd::string& text)
        {
            if (text.empty() || text.size() > MaxDescriptor) { return false; }
            rapidjson::Document doc; doc.Parse<rapidjson::kParseIterativeFlag | rapidjson::kParseValidateEncodingFlag | rapidjson::kParseFullPrecisionFlag>(text.data(),text.size());
            return !doc.HasParseError() && ParseFields(doc);
        }
        bool ParseFields(const Json& doc)
        {
            if (!doc.IsObject() || !doc.HasMember("version") || !Uint(doc["version"],3,1)) { return false; }
            const auto version=doc["version"].GetUint();
            if (!(version==3 ? Keys(doc,{"version","shader","vertex_count","streams","indices","stages","sort_key","instance_count"}) :
                Keys(doc,{"version","shader","vertex_count","streams","indices","stages","sort_key"})) ||
                !AssetPath(doc["shader"],".foashader.azshader") ||
                !Uint(doc["vertex_count"],1000000,3) || !Uint(doc["sort_key"],0xffffffffu)) { return false; }
            if (version==3)
            {
                if (!Uint(doc["instance_count"],MaxInstancesPerDraw,1)) { return false; }
                instanceCount=doc["instance_count"].GetUint();
            }
            shaderPath=doc["shader"].GetString(); vertexCount=doc["vertex_count"].GetUint(); sortKey=doc["sort_key"].GetUint();
            const auto& channels=doc["streams"];
            if (!channels.IsArray() || channels.Empty() || channels.Size()>16) { return false; }
            AZStd::set<AZStd::pair<AZStd::string,AZ::u32>> semantics;
            for (const auto& channel : channels.GetArray())
            {
                if (!Keys(channel,{"semantic","index","components","component_type","hex"}) || !Text(channel["semantic"],64) ||
                    !Uint(channel["index"],31) || !Uint(channel["components"],4,1) || !Uint(channel["component_type"],3,1)) { return false; }
                const AZStd::string name=channel["semantic"].GetString();
                for (const char c : name) { if (!((c>='A' && c<='Z') || (c>='a' && c<='z') || (c>='0' && c<='9') || c=='_')) { return false; } }
                if (!semantics.emplace(name,channel["index"].GetUint()).second) { return false; }
                Stream stream; stream.semantic=AZ::RHI::ShaderSemantic(AZ::Name(name),channel["index"].GetUint());
                stream.components=channel["components"].GetUint(); stream.type=channel["component_type"].GetUint();
                stream.bytes=Hex(channel["hex"],8*1024*1024);
                if (stream.bytes.size()!=size_t(vertexCount)*stream.components*4) { return false; }
                streams.push_back(AZStd::move(stream));
            }
            indices=Hex(doc["indices"],8*1024*1024);
            if (indices.empty() || indices.size()%12 ||
                indices.size()/4 > MaxInstanceIndexWork/instanceCount) { return false; }
            for (size_t i=0;i<indices.size();i+=4)
            {
                const AZ::u32 index=indices[i]|(AZ::u32(indices[i+1])<<8)|(AZ::u32(indices[i+2])<<16)|(AZ::u32(indices[i+3])<<24);
                if (index>=vertexCount) { return false; }
            }
            if (!doc["stages"].IsArray() || doc["stages"].Size()!=2) { return false; }
            for (AZ::u32 i=0;i<2;++i)
            {
                const auto& stage=doc["stages"][i]; auto& target=stages[i];
                if (!(doc["version"].GetUint() == 1 ? Keys(stage,{"constants","images","samplers"}) :
                    Keys(stage,{"constants","images","samplers","buffers"})) || !stage["constants"].IsArray() || stage["constants"].Size()>14 ||
                    !stage["images"].IsArray() || stage["images"].Size()>128 || !stage["samplers"].IsArray() || stage["samplers"].Size()>16) { return false; }
                AZStd::set<AZ::u32> registers;
                for (const auto& row : stage["constants"].GetArray())
                {
                    if (!Keys(row,{"slot","hex"}) || !Uint(row["slot"],13) || !registers.insert(row["slot"].GetUint()).second) { return false; }
                    Constant constant; constant.slot=row["slot"].GetUint(); constant.bytes=Hex(row["hex"],65536);
                    if (constant.bytes.empty() || constant.bytes.size()%16) { return false; }
                    target.constants.push_back(AZStd::move(constant));
                }
                registers.clear();
                for (const auto& row : stage["images"].GetArray())
                {
                    if (!Keys(row,{"slot","asset"}) || !Uint(row["slot"],127) || !registers.insert(row["slot"].GetUint()).second || !AssetPath(row["asset"],".streamingimage")) { return false; }
                    Image image; image.slot=row["slot"].GetUint(); image.path=row["asset"].GetString(); target.images.push_back(AZStd::move(image));
                }
                if (version >= 2)
                {
                    if (!stage["buffers"].IsArray() || stage["buffers"].Size()>128) { return false; }
                    for (const auto& row : stage["buffers"].GetArray())
                    {
                        if (!Keys(row,{"slot","type","stride","hex"}) || !Uint(row["slot"],127) || !Uint(row["type"],4,2) ||
                            !Uint(row["stride"],2048,4) || !registers.insert(row["slot"].GetUint()).second) { return false; }
                        DataBuffer buffer; buffer.slot=row["slot"].GetUint(); buffer.type=row["type"].GetUint(); buffer.stride=row["stride"].GetUint();
                        if ((buffer.type!=2 && buffer.type!=4) || buffer.stride%4 || (buffer.type==4 && buffer.stride!=4)) { return false; }
                        buffer.bytes=Hex(row["hex"],8*1024*1024);
                        if (buffer.bytes.empty() || buffer.bytes.size()%buffer.stride) { return false; }
                        target.dataBuffers.push_back(AZStd::move(buffer));
                    }
                }
                registers.clear();
                for (const auto& row : stage["samplers"].GetArray())
                {
                    Sampling sampling;
                    if (!Keys(row,{"slot","values"}) || !Uint(row["slot"],15) || !registers.insert(row["slot"].GetUint()).second || !Sampler(row["values"],sampling.state)) { return false; }
                    sampling.slot=row["slot"].GetUint(); target.samplers.push_back(sampling);
                }
                rapidjson::StringBuffer serialized; rapidjson::Writer<rapidjson::StringBuffer> writer(serialized);
                if (!stage.Accept(writer)) { return false; }
                target.signature.assign(serialized.GetString(),serialized.GetSize());
            }
            // Geometry is counted once at admission; mutable constants remain per draw.
            residentBytes = 0; dataBufferBytes = 0;
            for (const auto& stage : stages)
            {
                for (const auto& constant : stage.constants) { residentBytes += constant.bytes.size(); }
                for (const auto& buffer : stage.dataBuffers) { residentBytes += buffer.bytes.size(); dataBufferBytes += buffer.bytes.size(); }
            }
            return true;
        }
        bool Destination(const Json& value, bool vector, AZ::u32 version)
        {
            if (!Keys(value,{"stage","slot","offset","layout","value"}) || !Uint(value["stage"],1) ||
                !Uint(value["slot"],13) || !Uint(value["offset"],65536-16) || value["offset"].GetUint()%16 ||
                !value["layout"].IsString() || value["layout"]!=(vector ? "float4" : "column_major") ||
                !value["value"].IsString()) { return false; }
            Matrix matrix{value["stage"].GetUint(),value["slot"].GetUint(),value["offset"].GetUint()};
            if (vector)
            {
                if (version!=2 || value["value"]!="viewport_source_camera_position") { return false; }
                matrix.value=3; matrix.size=16;
            }
            else if (value["value"]=="source_object_to_world") { matrix.value=0; }
            else if (version==2 && value["value"]=="viewport_source_world_to_clip") { matrix.value=1; }
            else if (version==2 && value["value"]=="viewport_source_relative_world_to_clip") { matrix.value=2; }
            else { return false; }
            bool found=false;
            for (const auto& constant : stages[matrix.stage].constants)
            { found |= constant.slot==matrix.slot && matrix.offset+matrix.size<=constant.bytes.size(); }
            if (!found) { return false; }
            for (const auto& previous : matrices)
            {
                if (previous.stage==matrix.stage && previous.slot==matrix.slot &&
                    previous.offset<matrix.offset+matrix.size && matrix.offset<previous.offset+previous.size) { return false; }
            }
            camera |= matrix.value!=0; matrices.push_back(matrix); return true;
        }
        bool ParseEntity(const Json& doc)
        {
            if (!doc.IsObject() || !doc.HasMember("version") || !Uint(doc["version"],2,1)) { return false; }
            const auto version=doc["version"].GetUint();
            if (!(version==1 ? Keys(doc,{"version","draw","matrices"}) : Keys(doc,{"version","draw","matrices","vectors"})) ||
                !doc["matrices"].IsArray() || doc["matrices"].Empty() || doc["matrices"].Size()>8) { return false; }
            if (!ParseFields(doc["draw"])) { return false; }
            for (const auto& value : doc["matrices"].GetArray()) { if (!Destination(value,false,version)) { return false; } }
            if (version==2)
            {
                if (!doc["vectors"].IsArray() || doc["vectors"].Size()>8) { return false; }
                for (const auto& value : doc["vectors"].GetArray()) { if (!Destination(value,true,version)) { return false; } }
            }
            dirty=true; return true;
        }
    };
    SourceShaderRenderComponent::SourceShaderRenderComponent() = default;
    SourceShaderRenderComponent::~SourceShaderRenderComponent() = default;
    void SourceShaderRenderComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* sc=azrtti_cast<AZ::SerializeContext*>(context)) { sc->Class<SourceShaderRenderComponent,AZ::Component>()->Version(1); }
        if (auto* bc=azrtti_cast<AZ::BehaviorContext*>(context))
        {
            bc->EBus<SourceShaderRenderBus>("SourceShaderRenderBus")
                ->Attribute(AZ::Script::Attributes::Scope,AZ::Script::Attributes::ScopeFlags::Automation)
                ->Attribute(AZ::Script::Attributes::Module,"foa")
                ->Event("SetDraw",&SourceShaderRenderRequests::SetDraw)
                ->Event("GetStatus",&SourceShaderRenderRequests::GetStatus)
                ->Event("GetStatistics",&SourceShaderRenderRequests::GetStatistics)
                ->Event("GetViewportWorldToClip",&SourceShaderRenderRequests::GetViewportWorldToClip)
                ->Event("GetViewportCameraPosition",&SourceShaderRenderRequests::GetViewportCameraPosition)
                ->Event("ClearDraw",&SourceShaderRenderRequests::ClearDraw);
        }
    }
    void SourceShaderRenderComponent::Activate()
    {
        SourceShaderRenderBus::Handler::BusConnect(); AZ::TickBus::Handler::BusConnect();
        if (auto* viewports=AZ::RPI::ViewportContextRequests::Get())
        { AZ::RPI::ViewportContextNotificationBus::Handler::BusConnect(viewports->GetDefaultViewportContextName()); }
    }
    void SourceShaderRenderComponent::Deactivate()
    {
        AZ::RPI::ViewportContextNotificationBus::Handler::BusDisconnect(); AZ::TickBus::Handler::BusDisconnect();
        SourceShaderRenderBus::Handler::BusDisconnect(); ClearDraw();
        for (auto& [id,draws] : m_entities) for (auto& draw : draws) { Retire(AZStd::move(draw)); }
        m_entities.clear(); m_entityOrder.clear(); m_nextEntity=0; CollectRetired(); m_cameraView.reset(); m_cameraValid=false;
    }
    bool SourceShaderRenderComponent::Queue(Draw& draw)
    {
        draw.shaderAsset=QueueAsset<AZ::RPI::ShaderAsset>(draw.shaderPath);
        if (!draw.shaderAsset.GetId().IsValid()) { return false; }
        for (auto& stage : draw.stages) for (auto& image : stage.images)
        {
            image.asset=QueueAsset<AZ::RPI::StreamingImageAsset>(image.path);
            if (!image.asset.GetId().IsValid()) { return false; }
        }
        return true;
    }
    bool SourceShaderRenderComponent::Admit(Draw& draw)
    {
        // Only immutable input-assembly bytes are shared. Equality is checked after hashing,
        // and mutable constants, semantics, views and source identities remain draw-local.
        AZStd::unordered_map<AZStd::string, AZStd::shared_ptr<SharedGeometryBuffer>> pending;
        size_t addedBytes=draw.residentBytes, geometryBytes=0, reuses=0;
        auto acquire=[&](Bytes& bytes, AZStd::shared_ptr<SharedGeometryBuffer>& target)
        {
            const auto digest=QCryptographicHash::hash(QByteArrayView(reinterpret_cast<const char*>(bytes.data()),bytes.size()),QCryptographicHash::Sha256);
            AZStd::string key(digest.constData(),digest.size());
            auto existing=m_geometryBuffers.find(key);
            auto local=pending.find(key);
            if (existing!=m_geometryBuffers.end()) { target=existing->second; }
            else if (local!=pending.end()) { target=local->second; }
            if (target)
            {
                if (target->bytes!=bytes) { return false; }
                ++reuses; Bytes{}.swap(bytes); return true;
            }
            if (bytes.size()>MaxResidentBytes-m_residentBytes || addedBytes>MaxResidentBytes-m_residentBytes-bytes.size() ||
                m_geometryBuffers.size()+pending.size()>=MaxGeometryBuffers) { return false; }
            addedBytes+=bytes.size(); geometryBytes+=bytes.size();
            target=AZStd::make_shared<SharedGeometryBuffer>(); target->key=key; target->bytes=AZStd::move(bytes);
            pending.emplace(AZStd::move(key),target); return true;
        };
        if (addedBytes>MaxResidentBytes-m_residentBytes) { return false; }
        for (auto& stream : draw.streams) { if (!acquire(stream.bytes,stream.shared)) { return false; } }
        if (!acquire(draw.indices,draw.sharedIndices)) { return false; }
        for (auto& [key,value] : pending) { m_geometryBuffers.emplace(key,AZStd::move(value)); }
        m_residentBytes+=addedBytes; m_dataBufferBytes+=draw.dataBufferBytes; m_geometryBytes+=geometryBytes; m_geometryReuses+=reuses; return true;
    }
    void SourceShaderRenderComponent::Release(Draw& draw)
    {
        m_residentBytes-=draw.residentBytes; m_dataBufferBytes-=draw.dataBufferBytes;
        auto release=[&](AZStd::shared_ptr<SharedGeometryBuffer>& value)
        {
            if (value && value.use_count()==2)
            {
                m_residentBytes-=value->bytes.size(); m_geometryBytes-=value->bytes.size();
                m_geometryBuffers.erase(value->key);
            }
            value.reset();
        };
        for (auto& stream : draw.streams) { release(stream.shared); }
        release(draw.sharedIndices);
        m_samplerReservations-=draw.ownedSamplers;
        for (auto& stage : draw.sharedStages)
        {
            if (stage && stage.use_count()==2)
            { m_samplerReservations-=stage->samplers; m_sharedStages.erase(stage->key); }
            stage.reset();
        }
    }
    AZStd::string SourceShaderRenderComponent::SetDraw(const AZStd::string& descriptor)
    {
        CollectRetired();
        if (m_retired.size()>=MaxEntityDraws+4) { return "REJECTED: pending draw retirement"; }
        auto candidate=AZStd::make_unique<Draw>();
        if (!candidate->Parse(descriptor)) { return "REJECTED: invalid explicit draw descriptor"; }
        if (!Queue(*candidate)) { return "REJECTED: shader or image asset is absent"; }
        if (!Admit(*candidate)) { return "REJECTED: resident draw budget or geometry conflict"; }
        ClearDraw(); m_draw=AZStd::move(candidate); return m_draw->status;
    }
    AZStd::string SourceShaderRenderComponent::GetStatus() const { return m_draw ? m_draw->status : "EMPTY"; }
    AZStd::string SourceShaderRenderComponent::GetStatistics() const
    {
        size_t dirty=0, loading=0, failed=0;
        for (const auto& [id,draws] : m_entities) for (const auto& draw : draws)
        {
            dirty+=draw->dirty; loading+=draw->status=="LOADING"; failed+=draw->status.starts_with("FAILED");
        }
        return AZStd::string::format(
            "{\"entity_draws\":%zu,\"retired_draws\":%zu,\"resident_payload_bytes\":%zu,\"last_tick_work\":%zu,\"peak_tick_work\":%zu,"
            "\"geometry_payload_bytes\":%zu,\"constant_payload_bytes\":%zu,\"read_only_buffer_payload_bytes\":%zu,\"unique_geometry_buffers\":%zu,\"geometry_buffer_builds\":%zu,"
            "\"geometry_buffer_reuses\":%zu,\"last_tick_visits\":%zu,\"peak_tick_visits\":%zu,\"dirty_entity_draws\":%zu,\"loading_entity_draws\":%zu,\"failed_entity_draws\":%zu,\"shared_stages\":%zu,\"shared_stage_reuses\":%zu,\"sampler_reservations\":%zu,\"registered_entities\":%zu,\"submitted_entities\":%zu,\"submitted_entity_draws\":%zu}",
            m_entityOrder.size(),m_retired.size(),m_residentBytes,m_lastTickWork,m_peakTickWork,m_geometryBytes,m_residentBytes-m_geometryBytes-m_dataBufferBytes,m_dataBufferBytes,
            m_geometryBuffers.size(),m_geometryBuilds,m_geometryReuses,m_lastTickVisits,m_peakTickVisits,dirty,loading,failed,m_sharedStages.size(),m_stageReuses,m_samplerReservations,m_entities.size(),m_submittedEntities,m_submittedEntityDraws);
    }
    void SourceShaderRenderComponent::CollectRetired()
    {
        // DynamicDraw retains packets until FrameEnd, including borrowed geometry/SRG pointers.
        for (auto iterator=m_retired.begin();iterator!=m_retired.end();)
        {
            if ((*iterator)->packet->use_count()==1)
            { Release(**iterator); iterator=m_retired.erase(iterator); }
            else { ++iterator; }
        }
    }
    void SourceShaderRenderComponent::Retire(AZStd::unique_ptr<Draw> draw)
    {
        if (!draw) { return; }
        if (draw->packet && draw->packet->use_count()>1) { m_retired.push_back(AZStd::move(draw)); }
        else { Release(*draw); }
    }
    void SourceShaderRenderComponent::ClearDraw() { Retire(AZStd::move(m_draw)); CollectRetired(); }
    AZStd::string SourceShaderRenderComponent::AddEntityDraw(AZ::EntityId entity, const AZStd::string& binding)
    {
        CollectRetired();
        if (!entity.IsValid() || m_entities.contains(entity)) { return "REJECTED: invalid or already registered entity"; }
        if (binding.empty() || binding.size()>MaxDescriptor) { return "REJECTED: entity descriptor budget"; }
        rapidjson::Document document;
        document.Parse<rapidjson::kParseIterativeFlag | rapidjson::kParseValidateEncodingFlag | rapidjson::kParseFullPrecisionFlag>(binding.data(),binding.size());
        if (document.HasParseError() || !document.IsObject() || !document.HasMember("version") || !Uint(document["version"],3,1))
        { return "REJECTED: invalid source entity draw binding"; }
        const bool grouped=document["version"].GetUint()==3;
        size_t count=1;
        if (grouped)
        {
            if (!Keys(document,{"version","draws"}) || !document["draws"].IsArray() || document["draws"].Empty() ||
                document["draws"].Size()>MaxDrawsPerEntity) { return "REJECTED: invalid entity draw group"; }
            count=document["draws"].Size();
        }
        if (count>MaxEntityDraws-m_entityOrder.size() || m_retired.size()>=MaxEntityDraws+4) { return "REJECTED: entity draw budget"; }
        AZStd::vector<AZStd::unique_ptr<Draw>> candidates; candidates.reserve(count);
        for (size_t i=0;i<count;++i)
        {
            auto candidate=AZStd::make_unique<Draw>(); candidate->entity=entity;
            if (!candidate->ParseEntity(grouped ? document["draws"][rapidjson::SizeType(i)] : document))
            { return "REJECTED: invalid source entity draw binding"; }
            if (grouped && i && candidates.back()->sortKey>=candidate->sortKey) { return "REJECTED: entity draw order"; }
            if (!UpdateMatrices(*candidate,false)) { return "REJECTED: source placement is unavailable"; }
            candidates.push_back(AZStd::move(candidate));
        }
        for (auto& candidate : candidates)
        { if (!Queue(*candidate)) { return "REJECTED: shader or image asset is absent"; } }
        for (size_t i=0;i<count;++i)
        {
            if (!Admit(*candidates[i]))
            {
                // Failed admission can hold shared references. Drop them before releasing
                // earlier reservations so the final owner removes its cache entry.
                candidates[i].reset();
                for (size_t j=i;j>0;--j) { Release(*candidates[j-1]); }
                return "REJECTED: resident draw budget or geometry conflict";
            }
        }
        for (auto& draw : candidates) { draw->scheduleIndex=m_entityOrder.size(); m_entityOrder.push_back(draw.get()); }
        m_entities.emplace(entity,AZStd::move(candidates)); return "LOADING";
    }
    AZStd::string SourceShaderRenderComponent::GetEntityDrawStatus(AZ::EntityId entity) const
    {
        const auto found=m_entities.find(entity); if (found==m_entities.end()) { return "ABSENT"; }
        if (found->second.size()==1) { return found->second[0]->status; }
        bool loading=false;
        for (const auto& draw : found->second)
        {
            if (draw->status.starts_with("FAILED")) { return draw->status; }
            loading |= draw->status!="READY" || draw->dirty;
        }
        return loading ? "LOADING" : "READY";
    }
    void SourceShaderRenderComponent::RemoveEntityDraw(AZ::EntityId entity)
    {
        const auto found=m_entities.find(entity);
        if (found!=m_entities.end())
        {
            for (auto& draw : found->second)
            {
                const size_t index=draw->scheduleIndex;
                auto* moved=m_entityOrder.back(); m_entityOrder[index]=moved; m_entityOrder.pop_back();
                moved->scheduleIndex=index; Retire(AZStd::move(draw));
            }
            m_entities.erase(found); CollectRetired();
        }
    }
    void SourceShaderRenderComponent::InvalidateEntityPlacement(AZ::EntityId entity)
    {
        const auto found=m_entities.find(entity);
        if (found!=m_entities.end()) for (auto& draw : found->second) { draw->dirty=true; }
    }
    void SourceShaderRenderComponent::SetEntityVisibility(AZ::EntityId entity, bool visible)
    {
        const auto found=m_entities.find(entity);
        if (found!=m_entities.end()) for (auto& draw : found->second) { draw->visible=visible; }
    }
    AZ::Matrix4x4 SourceShaderRenderComponent::GetViewportWorldToClip() const
    {
        auto* viewports=AZ::RPI::ViewportContextRequests::Get();
        auto viewport=viewports ? viewports->GetDefaultViewportContext() : nullptr;
        auto view=viewport ? viewport->GetDefaultView() : nullptr;
        return view ? view->GetWorldToClipMatrix() : AZ::Matrix4x4::CreateIdentity();
    }
    AZ::Vector3 SourceShaderRenderComponent::GetViewportCameraPosition() const
    {
        auto* viewports=AZ::RPI::ViewportContextRequests::Get();
        auto viewport=viewports ? viewports->GetDefaultViewportContext() : nullptr;
        auto view=viewport ? viewport->GetDefaultView() : nullptr;
        return view ? view->GetViewToWorldMatrix().GetTranslation() : AZ::Vector3::CreateZero();
    }
    void SourceShaderRenderComponent::RefreshCamera()
    {
        auto* viewports=AZ::RPI::ViewportContextRequests::Get();
        auto viewport=viewports ? viewports->GetDefaultViewportContext() : nullptr;
        auto view=viewport ? viewport->GetDefaultView() : nullptr;
        const auto projection=view ? view->GetWorldToClipMatrix() : AZ::Matrix4x4::CreateIdentity();
        const auto position=view ? view->GetViewToWorldMatrix().GetTranslation() : AZ::Vector3::CreateZero();
        const bool valid=view && projection.IsFinite() && position.IsFinite();
        if (view==m_cameraView && valid==m_cameraValid && projection==m_worldToClip && position==m_cameraPosition) { return; }
        m_cameraView=view; m_cameraValid=valid; m_worldToClip=projection; m_cameraPosition=position;
        for (auto& [id,draws] : m_entities) for (auto& draw : draws) { if (draw->camera) { draw->dirty=true; } }
    }
    bool SourceShaderRenderComponent::UpdateMatrices(Draw& draw, bool upload)
    {
        if (!draw.entity.IsValid()) { return true; }
        AZStd::string status; AZ::Matrix4x4 world;
        SourceScenePlacementBus::EventResult(status,draw.entity,&SourceScenePlacementRequests::GetStatus);
        if (status!="READY" || (draw.camera && !m_cameraValid)) { return false; }
        SourceScenePlacementBus::EventResult(world,draw.entity,&SourceScenePlacementRequests::GetWorldMatrix);
        if (!world.IsFinite()) { return false; }
        // Original streams retain source axes. Object matrices exchange both axes; projection matrices
        // exchange input columns only. Clip coordinates retain the native viewport convention.
        const AZ::u32 axis[]{0,2,1,3}; float packed[4][16]{};
        auto relative=m_worldToClip;
        if (draw.camera)
        {
            relative=m_worldToClip*AZ::Matrix4x4::CreateTranslation(m_cameraPosition);
            if (!relative.IsFinite()) { return false; }
        }
        for (AZ::u32 col=0;col<4;++col) for (AZ::u32 row=0;row<4;++row)
        {
            packed[0][col*4+row]=world.GetElement(axis[row],axis[col]);
            packed[1][col*4+row]=m_worldToClip.GetElement(row,axis[col]);
            packed[2][col*4+row]=relative.GetElement(row,axis[col]);
        }
        packed[3][0]=m_cameraPosition.GetX(); packed[3][1]=m_cameraPosition.GetZ(); packed[3][2]=m_cameraPosition.GetY();
        for (AZ::u32 stage=0;stage<2;++stage) for (auto& constant : draw.stages[stage].constants)
        {
            bool changed=false;
            for (const auto& matrix : draw.matrices)
            {
                if (matrix.stage==stage && matrix.slot==constant.slot)
                { memcpy(constant.bytes.data()+matrix.offset,packed[matrix.value],matrix.size); changed=true; }
            }
            if (changed && upload && (!constant.buffer || !constant.buffer->UpdateData(constant.bytes.data(),constant.bytes.size()))) { return false; }
        }
        draw.dirty=false; return true;
    }
    void SourceShaderRenderComponent::OnTick(float, AZ::ScriptTimePoint)
    {
        CollectRetired(); RefreshCamera();
        size_t work=4;
        if (m_draw) { Advance(*m_draw,work); }
        // Dense scheduling avoids walking from the beginning of a hash table every tick.
        // Visits and expensive resource work have separate bounds, including unavailable assets.
        m_lastTickVisits=0;
        while (!m_entityOrder.empty() && m_lastTickVisits<m_entityOrder.size() && m_lastTickVisits<MaxTickVisits && work)
        {
            m_nextEntity%=m_entityOrder.size();
            auto* draw=m_entityOrder[m_nextEntity++]; ++m_lastTickVisits;
            Advance(*draw,work);
        }
        if (m_lastTickVisits>m_peakTickVisits) { m_peakTickVisits=m_lastTickVisits; }
        m_lastTickWork=4-work;
        if (m_lastTickWork>m_peakTickWork) { m_peakTickWork=m_lastTickWork; }
    }
    void SourceShaderRenderComponent::Advance(Draw& draw, size_t& work)
    {
        if (draw.camera && !m_cameraValid)
        {
            if (draw.status=="LOADING" && Clock::now()-draw.started>AZStd::chrono::seconds(60))
            { draw.status="FAILED: viewport camera unavailable"; }
            return;
        }
        if (draw.packet)
        {
            if (draw.dirty && work) { --work; draw.status=UpdateMatrices(draw,true) ? "READY" : "FAILED: source placement update"; }
            return;
        }
        if (draw.status!="LOADING") { return; }
        bool ready=draw.shaderAsset.IsReady(),error=draw.shaderAsset.IsError();
        for (auto& stage : draw.stages) for (auto& image : stage.images)
        { ready &= image.asset.IsReady(); error |= image.asset.IsError(); }
        if (error || Clock::now()-draw.started>AZStd::chrono::seconds(60))
        { draw.status="FAILED: asset load error or timeout"; return; }
        if (!ready) { return; }
        auto* viewports=AZ::RPI::ViewportContextRequests::Get();
        auto viewport=viewports ? viewports->GetDefaultViewportContext() : nullptr;
        if (!viewport || !viewport->GetRenderScene()) { return; }
        if (!work) { return; }
        --work; draw.scene=viewport->GetRenderScene();
        if (!UpdateMatrices(draw,false) || !Build(draw)) { draw.status="FAILED: native resource or pipeline binding"; return; }
        draw.status="READY";
    }
    bool SourceShaderRenderComponent::Build(Draw& draw)
    {
        draw.shader=AZ::RPI::Shader::FindOrCreate(draw.shaderAsset);
        if (!draw.shader) { return false; }
        const auto& contract=draw.shaderAsset->GetInputContract();
        if (contract.m_streamChannels.size()!=draw.streams.size()) { return false; }
        for (size_t i=0;i<draw.streams.size();++i)
        {
            if (contract.m_streamChannels[i].m_semantic!=draw.streams[i].semantic ||
                contract.m_streamChannels[i].m_componentCount!=draw.streams[i].components) { return false; }
        }
        AZ::RHI::PipelineStateDescriptorForDraw pipeline;
        draw.shader->GetRootVariant().ConfigurePipelineState(pipeline);
        AZ::RHI::InputStreamLayoutBuilder layout;
        AZ::RHI::StreamBufferIndices streamIndices;
        const AZ::RHI::Format formats[3][4]{
            {AZ::RHI::Format::R32_UINT,AZ::RHI::Format::R32G32_UINT,AZ::RHI::Format::R32G32B32_UINT,AZ::RHI::Format::R32G32B32A32_UINT},
            {AZ::RHI::Format::R32_SINT,AZ::RHI::Format::R32G32_SINT,AZ::RHI::Format::R32G32B32_SINT,AZ::RHI::Format::R32G32B32A32_SINT},
            {AZ::RHI::Format::R32_FLOAT,AZ::RHI::Format::R32G32_FLOAT,AZ::RHI::Format::R32G32B32_FLOAT,AZ::RHI::Format::R32G32B32A32_FLOAT}};
        auto geometryBuffer=[&](const AZStd::shared_ptr<SharedGeometryBuffer>& shared)
        {
            if (!shared->buffer)
            {
                shared->buffer=Buffer(shared->bytes,AZ::RPI::CommonBufferPoolType::StaticInputAssembly,4);
                if (shared->buffer) { ++m_geometryBuilds; }
            }
            return shared->buffer;
        };
        for (const auto& stream : draw.streams)
        {
            auto buffer=geometryBuffer(stream.shared);
            if (!buffer) { return false; }
            if (!draw.geometry) { draw.geometry=AZStd::make_unique<AZ::RHI::GeometryView>(buffer->GetRHIBuffer()->GetDeviceMask()); }
            layout.AddBuffer()->Channel(stream.semantic,formats[stream.type-1][stream.components-1]);
            streamIndices.AddIndex(AZ::u8(draw.geometry->GetStreamBufferViews().size()));
            draw.geometry->AddStreamBufferView(AZ::RHI::StreamBufferView(*buffer->GetRHIBuffer(),0,AZ::u32(stream.shared->bytes.size()),stream.components*4));
            draw.buffers.push_back(buffer);
        }
        pipeline.m_inputStreamLayout=layout.End();
        auto indexBuffer=geometryBuffer(draw.sharedIndices);
        if (!indexBuffer || !draw.geometry) { return false; }
        draw.geometry->SetIndexBufferView(AZ::RHI::IndexBufferView(*indexBuffer->GetRHIBuffer(),0,AZ::u32(draw.sharedIndices->bytes.size()),AZ::RHI::IndexFormat::Uint32));
        draw.geometry->SetDrawArguments(AZ::RHI::DrawIndexed(0,AZ::u32(draw.sharedIndices->bytes.size()/4),0)); draw.buffers.push_back(indexBuffer);
        for (AZ::u32 stage=0;stage<2;++stage)
        {
            auto& source=draw.stages[stage]; bool dynamic=false;
            for (const auto& destination : draw.matrices) { dynamic |= destination.stage==stage; }
            AZStd::string signature, key;
            if (!dynamic)
            {
                // Include resolved resource instances, not just paths, to avoid reusing an older asset load.
                signature=AZStd::string::format("%p:%u:",draw.shader.get(),stage)+source.signature;
                for (const auto& image : source.images)
                { signature+=AZStd::string::format(":%u:%p",image.slot,image.asset.Get()); }
                const auto digest=QCryptographicHash::hash(QByteArrayView(signature.data(),signature.size()),QCryptographicHash::Sha256);
                key.assign(digest.constData(),digest.size());
                const auto cached=m_sharedStages.find(key);
                if (cached!=m_sharedStages.end())
                {
                    if (cached->second->signature!=signature) { return false; }
                    draw.sharedStages[stage]=cached->second; draw.srgs[stage]=cached->second->srg; ++m_stageReuses; continue;
                }
            }
            // Native DX12 rings consume one sampler table per in-flight frame. Keep this
            // service's reservations bounded before asking RHI to allocate a new table.
            if (source.samplers.size()>MaxSamplerReservations-m_samplerReservations) { return false; }
            m_samplerReservations+=source.samplers.size(); draw.ownedSamplers+=source.samplers.size();
            auto srg=AZ::RPI::ShaderResourceGroup::Create(draw.shaderAsset,AZ::Name(stage==0 ? "SourceVertexSrg" : "SourceFragmentSrg"));
            if (!srg) { return false; }
            const auto* srgLayout=srg->GetLayout();
            if (srgLayout->GetShaderInputListForBuffers().size()!=source.constants.size()+source.dataBuffers.size() ||
                srgLayout->GetShaderInputListForImages().size()!=source.images.size() || srgLayout->GetShaderInputListForSamplers().size()!=source.samplers.size()) { return false; }
            for (auto& constant : source.constants)
            {
                const auto index=srg->FindShaderInputBufferIndex(AZ::Name(AZStd::string::format("cb%u",constant.slot)));
                if (!index.IsValid() || srgLayout->GetShaderInput(index).m_strideSize!=constant.bytes.size()) { return false; }
                auto buffer=Buffer(constant.bytes,AZ::RPI::CommonBufferPoolType::Constant,16);
                if (!buffer || !srg->SetBuffer(index,buffer)) { return false; } constant.buffer=buffer; draw.buffers.push_back(buffer);
            }
            for (const auto& data : source.dataBuffers)
            {
                const auto index=srg->FindShaderInputBufferIndex(AZ::Name(AZStd::string::format("t%u",data.slot)));
                if (!index.IsValid()) { return false; }
                const auto& input=srgLayout->GetShaderInput(index);
                if (input.m_access!=AZ::RHI::ShaderInputBufferAccess::Read || AZ::u32(input.m_type)!=data.type || input.m_strideSize!=data.stride) { return false; }
                auto buffer=Buffer(data.bytes,AZ::RPI::CommonBufferPoolType::ReadOnly,data.stride,data.type==4);
                if (!buffer || !srg->SetBuffer(index,buffer)) { return false; }
                draw.buffers.push_back(buffer);
            }
            for (const auto& image : source.images)
            {
                const auto index=srg->FindShaderInputImageIndex(AZ::Name(AZStd::string::format("t%u",image.slot)));
                if (!index.IsValid()) { return false; }
                auto native=AZ::RPI::StreamingImage::FindOrCreate(image.asset);
                if (!native || !srg->SetImage(index,native)) { return false; } draw.images.push_back(native);
            }
            for (const auto& sampler : source.samplers)
            {
                const auto index=srg->FindShaderInputSamplerIndex(AZ::Name(AZStd::string::format("s%u",sampler.slot)));
                if (!index.IsValid() || !srg->SetSampler(index,sampler.state)) { return false; }
            }
            srg->Compile(); draw.srgs[stage]=srg;
            if (!dynamic)
            {
                auto shared=AZStd::make_shared<SharedStage>(); shared->key=key; shared->signature=AZStd::move(signature);
                shared->srg=srg; shared->samplers=source.samplers.size(); draw.ownedSamplers-=shared->samplers;
                m_sharedStages.emplace(key,shared); draw.sharedStages[stage]=AZStd::move(shared);
            }
        }
        if (!draw.scene->ConfigurePipelineState(draw.shader->GetDrawListTag(),pipeline)) { return false; }
        const auto* state=draw.shader->AcquirePipelineState(pipeline);
        if (!state || !state->IsInitialized() || state->GetType()!=AZ::RHI::PipelineStateType::Draw) { return false; }
        AZ::RHI::DrawPacketBuilder builder(indexBuffer->GetRHIBuffer()->GetDeviceMask()); builder.Begin(nullptr);
        builder.SetGeometryView(draw.geometry.get());
        builder.SetDrawInstanceArguments(AZ::RHI::DrawInstanceArguments(draw.instanceCount,0));
        for (const auto& srg : draw.srgs) { builder.AddShaderResourceGroup(srg->GetRHIShaderResourceGroup()); }
        AZ::RHI::DrawPacketBuilder::DrawRequest request; request.m_pipelineState=state; request.m_listTag=draw.shader->GetDrawListTag();
        request.m_streamIndices=streamIndices; request.m_sortKey=draw.sortKey; builder.AddDrawItem(request); draw.packet=builder.End();
        return bool(draw.packet);
    }
    void SourceShaderRenderComponent::OnRenderTick()
    {
        RefreshCamera();
        auto* viewports=AZ::RPI::ViewportContextRequests::Get();
        auto viewport=viewports ? viewports->GetDefaultViewportContext() : nullptr;
        auto* dynamic=AZ::RPI::DynamicDrawInterface::Get();
        m_submittedEntities=0; m_submittedEntityDraws=0;
        if (!viewport || !dynamic) { return; }
        auto submit=[&](Draw& draw)
        {
            if (draw.status!="READY" || !draw.visible || draw.dirty) { return; }
            if (viewport->GetRenderScene()!=draw.scene) { draw.status="FAILED: render scene changed"; return; }
            dynamic->AddDrawPacket(draw.scene.get(),draw.packet);
        };
        if (m_draw) { submit(*m_draw); }
        for (auto& [id,draws] : m_entities)
        {
            bool ready=true;
            for (auto& draw : draws)
            {
                if (draw->packet && viewport->GetRenderScene()!=draw->scene) { draw->status="FAILED: render scene changed"; }
                ready &= draw->status=="READY" && draw->visible && !draw->dirty;
            }
            if (!ready) { continue; }
            for (auto& draw : draws) { submit(*draw); ++m_submittedEntityDraws; }
            ++m_submittedEntities;
        }
    }
}
