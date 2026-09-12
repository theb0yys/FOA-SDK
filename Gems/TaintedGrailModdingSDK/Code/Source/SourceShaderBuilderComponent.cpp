/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#include "SourceShaderBuilderComponent.h"
#include <AssetBuilderSDK/SerializationDependencies.h>
#include <AzCore/IO/SystemFile.h>
#include <AzCore/IO/Path/Path.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContextConstants.inl>
#include <AzCore/Serialization/Utils.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/containers/set.h>
#include <QCryptographicHash>

#if defined(TG_SOURCE_SHADER_DX12)
#include <Atom/RHI.Reflect/DX12/ShaderStageFunction.h>
#include <Atom/RHI.Reflect/DX12/PipelineLayoutDescriptor.h>
#include <Atom/RPI.Reflect/Shader/ShaderAssetCreator.h>
#include <Atom/RPI.Edit/Shader/ShaderVariantAssetCreator.h>
#endif

namespace TaintedGrailModdingSDK
{
    namespace
    {
        constexpr const char* JobKey = "FOA Source Shader";
#if defined(TG_SOURCE_SHADER_DX12)
        using Bytes = AZStd::vector<AZ::u8>;
        constexpr size_t MaxPacket = 64 * 1024 * 1024;
        constexpr size_t HeaderSize = 56;
        AZ::u32 U32(const AZ::u8* data)
        {
            return data[0] | (AZ::u32(data[1]) << 8) | (AZ::u32(data[2]) << 16) | (AZ::u32(data[3]) << 24);
        }
        struct Reader
        {
            const Bytes& m_bytes;
            size_t m_cursor = HeaderSize;
            bool m_good = true;
            AZ::u32 Uint(AZ::u32 maximum = 0xffffffffu, AZ::u32 minimum = 0)
            {
                if (!m_good || m_cursor + 4 > m_bytes.size()) { m_good = false; return 0; }
                const auto value = U32(m_bytes.data() + m_cursor); m_cursor += 4;
                if (value < minimum || value > maximum) { m_good = false; return 0; }
                return value;
            }
            Bytes Blob(AZ::u32 maximum, AZ::u32 minimum)
            {
                const auto size = Uint(maximum, minimum);
                if (!m_good || size > m_bytes.size() - m_cursor) { m_good = false; return {}; }
                Bytes result(m_bytes.begin() + m_cursor, m_bytes.begin() + m_cursor + size);
                m_cursor += size; return result;
            }
            AZStd::string Name()
            {
                auto bytes = Blob(64, 1);
                for (auto c : bytes)
                {
                    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-'))
                    { m_good = false; return {}; }
                }
                return AZStd::string(bytes.begin(), bytes.end());
            }
        };
        bool Signature(const AZ::u8* bytes, size_t size, bool extended, bool vertex,
            AZ::RPI::ShaderInputContract& input, AZ::RPI::ShaderOutputContract& output)
        {
            if (size < 8) { return false; }
            const auto count = U32(bytes); const size_t stride = extended ? 32 : 24;
            if (count > 32 || 8 + count * stride > size) { return false; }
            AZStd::set<AZStd::pair<AZStd::string, AZ::u32>> seen;
            for (AZ::u32 i = 0; i < count; ++i)
            {
                const auto* row = bytes + 8 + i * stride;
                if (extended && U32(row) != 0) { return false; }
                row += extended ? 4 : 0;
                const auto offset = U32(row), index = U32(row + 4), system = U32(row + 8);
                const auto mask = row[20];
                if (offset < 8 + count * stride || offset >= size || !mask || mask > 15 || index > 31) { return false; }
                size_t end = offset;
                while (end < size && end - offset <= 64 && bytes[end] != 0) { ++end; }
                if (end == size || end == offset || end - offset > 64) { return false; }
                AZStd::string name(reinterpret_cast<const char*>(bytes + offset), end - offset);
                if (!seen.emplace(name, index).second) { return false; }
                AZ::u32 components = 0;
                for (AZ::u32 bit = 1; bit <= mask; bit <<= 1) { ++components; }
                if (vertex && system == 0)
                {
                    AZ::RPI::ShaderInputContract::StreamChannelInfo channel;
                    channel.m_semantic = AZ::RHI::ShaderSemantic(AZ::Name(name), index);
                    channel.m_componentCount = components; input.m_streamChannels.push_back(channel);
                }
                if (!vertex && (name == "SV_Target" || name == "SV_TARGET"))
                {
                    if (index >= 8) { return false; }
                    if (output.m_requiredColorAttachments.size() <= index) { output.m_requiredColorAttachments.resize(index + 1); }
                    output.m_requiredColorAttachments[index].m_componentCount = components;
                }
            }
            return true;
        }
        struct ShaderBuffer { AZ::u32 slot{}, type{}, stride{}; };
        bool ShaderBuffers(const AZ::u8* data, size_t size, AZStd::vector<ShaderBuffer>& buffers)
        {
            // SM5.0 direct SRV declarations from the published DirectX token format.
            // No element contents or runtime resource ownership are inferred here.
            AZStd::set<AZ::u32> resourceSlots;
            size_t cursor = 8;
            while (cursor < size)
            {
                const auto token = U32(data+cursor), opcode = token & 0x7ffu;
                AZ::u32 length = (token >> 24) & 0x7fu;
                if (opcode == 53)
                {
                    if (size-cursor < 8) { return false; }
                    length = U32(data+cursor+4);
                    if (length < 2) { return false; }
                }
                if (!length || length > (size-cursor)/4) { return false; }
                if (opcode == 161 || opcode == 162)
                {
                    if (token != (((opcode == 161 ? 3u : 4u) << 24) | opcode) || U32(data+cursor+4) != 0x107000u) { return false; }
                    const auto slot = U32(data+cursor+8), stride = opcode == 161 ? 4u : U32(data+cursor+12);
                    if (slot > 127 || stride < 4 || stride > 2048 || stride%4 || !resourceSlots.insert(slot).second) { return false; }
                    buffers.push_back({slot,opcode == 161 ? 4u : 2u,stride});
                }
                cursor += size_t(length)*4;
            }
            return cursor == size;
        }
        bool Program(const Bytes& code, bool vertex, AZ::RPI::ShaderInputContract& input, AZ::RPI::ShaderOutputContract& output,
            AZStd::vector<ShaderBuffer>& buffers)
        {
            if (code.size() < 32 || memcmp(code.data(), "DXBC", 4) || U32(code.data()+20) != 1 || U32(code.data()+24) != code.size()) { return false; }
            const auto count = U32(code.data()+28); const size_t tableEnd = 32 + size_t(count)*4;
            if (!count || count > 64 || tableEnd > code.size()) { return false; }
            AZStd::vector<AZStd::pair<size_t, size_t>> ranges{{0, tableEnd}};
            AZ::u32 instructions = 0, signatures = 0;
            for (AZ::u32 i = 0; i < count; ++i)
            {
                const size_t start = U32(code.data()+32+i*4);
                if (start < tableEnd || start % 4 || start + 8 > code.size()) { return false; }
                const size_t size = U32(code.data()+start+4);
                if (size > code.size()-start-8) { return false; }
                ranges.emplace_back(start, start+8+size);
                const auto* tag = code.data()+start; const auto* payload = tag+8;
                if (!memcmp(tag,"SHDR",4) || !memcmp(tag,"SHEX",4))
                {
                    if (++instructions != 1 || size < 8 || size % 4 || U32(payload+4) != size/4) { return false; }
                    const auto version = U32(payload);
                    if (version >> 16 != (vertex ? 1u : 0u) || (version & 0xffff) != 0x50 || !ShaderBuffers(payload,size,buffers)) { return false; }
                }
                const bool legacy = !memcmp(tag,vertex ? "ISGN" : "OSGN",4);
                const bool extended = !memcmp(tag,vertex ? "ISG1" : "OSG1",4);
                if (legacy || extended)
                {
                    if (++signatures != 1 || !Signature(payload,size,extended,vertex,input,output)) { return false; }
                }
            }
            AZStd::sort(ranges.begin(),ranges.end()); size_t cursor = 0;
            for (const auto& range : ranges) { if (range.first != cursor) { return false; } cursor = range.second; }
            return cursor == code.size() && instructions == 1 && signatures == 1;
        }
        template<class T>
        bool Save(const AZ::Data::Asset<T>& asset, const AZStd::string& path, AZ::u32 subId, AssetBuilderSDK::ProcessJobResponse& response)
        {
            if (!AZ::Utils::SaveObjectToFile(path, AZ::DataStream::ST_BINARY, asset.Get())) { return false; }
            AssetBuilderSDK::JobProduct product;
            if (!AssetBuilderSDK::OutputObject(asset.Get(), path, azrtti_typeid<T>(), subId, product)) { return false; }
            response.m_outputProducts.push_back(AZStd::move(product)); return true;
        }
        bool BuildShader(const AssetBuilderSDK::ProcessJobRequest& request, AssetBuilderSDK::ProcessJobResponse& response,
            const AZStd::function<bool()>& cancelled)
        {
            AZ::IO::SystemFile source;
            if (!source.Open(request.m_fullPath.c_str(), AZ::IO::SystemFile::SF_OPEN_READ_ONLY)) { return false; }
            const auto length = source.Length();
            if (length <= HeaderSize || length > MaxPacket || cancelled()) { return false; }
            Bytes bytes(static_cast<size_t>(length));
            if (source.Read(length,bytes.data()) != length || source.Length() != length) { return false; }
            const auto packetVersion = U32(bytes.data()+8);
            if (memcmp(bytes.data(),"FOASHD01",8) || (packetVersion != 1 && packetVersion != 2) || U32(bytes.data()+12) != 1 ||
                U32(bytes.data()+16) != bytes.size()-HeaderSize || U32(bytes.data()+20) != 0) { return false; }
            QCryptographicHash hash(QCryptographicHash::Sha256);
            hash.addData(QByteArrayView(reinterpret_cast<const char*>(bytes.data()),24));
            hash.addData(QByteArrayView(reinterpret_cast<const char*>(bytes.data()+HeaderSize),bytes.size()-HeaderSize));
            const auto digest = hash.result();
            if (memcmp(digest.constData(),bytes.data()+24,32)) { return false; }
            Reader reader{bytes}; const auto drawList = reader.Name();
            AZ::RHI::RenderStates states;
            states.m_rasterState.m_cullMode = static_cast<AZ::RHI::CullMode>(reader.Uint(2));
            auto& depth = states.m_depthStencilState.m_depth;
            depth.m_enable = reader.Uint(1); depth.m_writeMask = static_cast<AZ::RHI::DepthWriteMask>(reader.Uint(1));
            depth.m_func = static_cast<AZ::RHI::ComparisonFunc>(reader.Uint(7));
            auto& blend = states.m_blendState.m_targets[0]; blend.m_enable = reader.Uint(1);
            blend.m_blendSource = static_cast<AZ::RHI::BlendFactor>(reader.Uint(16));
            blend.m_blendDest = static_cast<AZ::RHI::BlendFactor>(reader.Uint(16));
            blend.m_blendOp = static_cast<AZ::RHI::BlendOp>(reader.Uint(4));
            blend.m_blendAlphaSource = static_cast<AZ::RHI::BlendFactor>(reader.Uint(16));
            blend.m_blendAlphaDest = static_cast<AZ::RHI::BlendFactor>(reader.Uint(16));
            blend.m_blendAlphaOp = static_cast<AZ::RHI::BlendOp>(reader.Uint(4)); blend.m_writeMask = reader.Uint(15);
            if (!reader.m_good) { return false; }
            auto pipeline = AZ::DX12::PipelineLayoutDescriptor::Create();
            AZ::RPI::ShaderResourceGroupLayoutList layouts;
            AZ::RPI::ShaderInputContract inputs; AZ::RPI::ShaderOutputContract outputs;
            AZ::RPI::ShaderVariantAssetCreator variant;
            const AZ::Data::AssetId variantId(request.m_sourceFileUUID,1);
            variant.Begin(variantId,{},AZ::RPI::ShaderVariantStableId{0},true);
            for (AZ::u32 stage = 0; stage < 2; ++stage)
            {
                if (cancelled()) { return false; }
                const auto code = reader.Blob(32*1024*1024,32);
                AZStd::vector<ShaderBuffer> declaredBuffers, suppliedBuffers;
                if (!reader.m_good || !Program(code,stage == 0,inputs,outputs,declaredBuffers)) { return false; }
                const auto shaderStage = stage == 0 ? AZ::RHI::ShaderStage::Vertex : AZ::RHI::ShaderStage::Fragment;
                const auto mask = stage == 0 ? AZ::RHI::ShaderStageMask::Vertex : AZ::RHI::ShaderStageMask::Fragment;
                auto function = AZ::DX12::ShaderStageFunction::Create(shaderStage); function->SetByteCode(0,code);
                if (function->Finalize() != AZ::RHI::ResultCode::Success) { return false; }
                variant.SetShaderFunction(shaderStage,function);
                auto layout = AZ::RHI::ShaderResourceGroupLayout::Create();
                layout->SetName(AZ::Name(stage == 0 ? "SourceVertexSrg" : "SourceFragmentSrg"));
                layout->SetBindingSlot(stage+3);
                layout->SetUniqueId(AZStd::string::format("foa-source-shader-v%u-%s-%u",packetVersion,digest.toHex().constData(),stage));
                AZ::RHI::ShaderResourceGroupBindingInfo binding;
                auto resource = [&](const AZStd::string& name, AZ::u32 slot)
                {
                    return binding.m_resourcesRegisterMap.emplace(AZ::Name(name),AZ::RHI::ResourceBindingInfo{mask,slot,0}).second;
                };
                for (AZ::u32 i = 0, count = reader.Uint(14); i < count; ++i)
                {
                    const auto slot = reader.Uint(13), size = reader.Uint(65536,16);
                    const auto name = AZStd::string::format("cb%u",slot);
                    if (!reader.m_good || size%16 || !resource(name,slot)) { return false; }
                    layout->AddShaderInput(AZ::RHI::ShaderInputBufferDescriptor(AZ::Name(name),AZ::RHI::ShaderInputBufferAccess::Constant,
                        AZ::RHI::ShaderInputBufferType::Constant,1,size,slot,0));
                }
                for (AZ::u32 i = 0, count = reader.Uint(128); i < count; ++i)
                {
                    const auto slot = reader.Uint(127), type = reader.Uint(9,1);
                    const auto name = AZStd::string::format("t%u",slot);
                    if (!reader.m_good || !resource(name,slot)) { return false; }
                    layout->AddShaderInput(AZ::RHI::ShaderInputImageDescriptor(AZ::Name(name),AZ::RHI::ShaderInputImageAccess::Read,
                        static_cast<AZ::RHI::ShaderInputImageType>(type),1,slot,0));
                }
                for (AZ::u32 i = 0, count = reader.Uint(16); i < count; ++i)
                {
                    const auto slot = reader.Uint(15); const auto name = AZStd::string::format("s%u",slot);
                    if (!reader.m_good || !resource(name,slot)) { return false; }
                    layout->AddShaderInput(AZ::RHI::ShaderInputSamplerDescriptor(AZ::Name(name),1,slot,0));
                }
                if (packetVersion == 2)
                {
                    for (AZ::u32 i = 0, count = reader.Uint(128); i < count; ++i)
                    {
                        const auto slot = reader.Uint(127), type = reader.Uint(4,2), stride = reader.Uint(2048,4);
                        const auto name = AZStd::string::format("t%u",slot);
                        if (!reader.m_good || (type != 2 && type != 4) || stride%4 || (type == 4 && stride != 4) || !resource(name,slot)) { return false; }
                        layout->AddShaderInput(AZ::RHI::ShaderInputBufferDescriptor(AZ::Name(name),AZ::RHI::ShaderInputBufferAccess::Read,
                            static_cast<AZ::RHI::ShaderInputBufferType>(type),1,stride,slot,0));
                        suppliedBuffers.push_back({slot,type,stride});
                    }
                }
                auto order = [](const ShaderBuffer& a,const ShaderBuffer& b) { return a.slot < b.slot; };
                AZStd::sort(declaredBuffers.begin(),declaredBuffers.end(),order);
                AZStd::sort(suppliedBuffers.begin(),suppliedBuffers.end(),order);
                if (declaredBuffers.size() != suppliedBuffers.size()) { return false; }
                for (size_t i = 0; i < declaredBuffers.size(); ++i)
                {
                    const auto& a = declaredBuffers[i]; const auto& b = suppliedBuffers[i];
                    if (a.slot != b.slot || a.type != b.type || a.stride != b.stride) { return false; }
                }
                if (!reader.m_good || !layout->Finalize()) { return false; }
                pipeline->AddShaderResourceGroupLayoutInfo(*layout,binding);
                AZ::DX12::ShaderResourceGroupVisibility visibility; visibility.m_descriptorTableShaderStageMask = mask;
                pipeline->AddShaderResourceGroupVisibility(visibility); layouts.push_back(layout);
            }
            if (!reader.m_good || reader.m_cursor != bytes.size() || cancelled()) { return false; }
            if (outputs.m_requiredColorAttachments.size() > 1 && blend.m_enable) { return false; }
            for (const auto& output : outputs.m_requiredColorAttachments) { if (!output.m_componentCount) { return false; } }
            if (pipeline->Finalize() != AZ::RHI::ResultCode::Success) { return false; }
            AZ::Data::Asset<AZ::RPI::ShaderVariantAsset> variantAsset;
            if (!variant.End(variantAsset)) { return false; }
            auto options = AZ::RPI::ShaderOptionGroupLayout::Create();
            options->Finalize();
            if (!options->IsFinalized()) { return false; }
            const auto stem = AZ::IO::Path(request.m_fullPath).Filename().String();
            AZ::RPI::ShaderAssetCreator shader; shader.Begin(AZ::Data::AssetId(request.m_sourceFileUUID,0));
            shader.SetName(AZ::Name(stem)); shader.SetDrawListName(AZ::Name(drawList)); shader.SetShaderOptionGroupLayout(options);
            shader.BeginAPI(AZ::RHI::APIType{"dx12"}); shader.BeginSupervariant(AZ::Name{});
            shader.SetSrgLayoutList(layouts); shader.SetPipelineLayout(pipeline); shader.SetInputContract(inputs); shader.SetOutputContract(outputs);
            shader.SetRenderStates(states); AZ::RHI::ShaderStageAttributeMapList attributes; attributes.resize(AZ::RHI::ShaderStageCount);
            shader.SetShaderStageAttributeMapList(attributes); shader.SetRootShaderVariantAsset(variantAsset);
            if (!shader.EndSupervariant() || !shader.EndAPI()) { return false; }
            AZ::Data::Asset<AZ::RPI::ShaderAsset> shaderAsset;
            if (!shader.End(shaderAsset) || cancelled()) { return false; }
            const AZ::IO::Path output(request.m_tempDirPath);
            return Save(variantAsset,(output/(stem+".azshadervariant")).String(),1,response) &&
                Save(shaderAsset,(output/(stem+".azshader")).String(),0,response);
        }
#endif
    }
    void SourceShaderBuilderComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serialize = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serialize->Class<SourceShaderBuilderComponent,AZ::Component>()->Version(1)
                ->Attribute(AZ::Edit::Attributes::SystemComponentTags,AZStd::vector<AZ::Crc32>{AssetBuilderSDK::ComponentTags::AssetBuilder});
        }
    }
    void SourceShaderBuilderComponent::Activate()
    {
        m_stopping = false;
        AssetBuilderSDK::AssetBuilderDesc descriptor; descriptor.m_name = JobKey;
        descriptor.m_busId = azrtti_typeid<SourceShaderBuilderComponent>(); descriptor.m_version = 2;
        descriptor.m_patterns.emplace_back("*.foashader",AssetBuilderSDK::AssetBuilderPattern::PatternType::Wildcard);
        descriptor.m_createJobFunction = [this](const auto& request,auto& response) { CreateJobs(request,response); };
        descriptor.m_processJobFunction = [this](const auto& request,auto& response) { ProcessJob(request,response); };
        BusConnect(descriptor.m_busId);
        AssetBuilderSDK::AssetBuilderBus::Broadcast(&AssetBuilderSDK::AssetBuilderBusTraits::RegisterBuilderInformation,descriptor);
    }
    void SourceShaderBuilderComponent::Deactivate() { BusDisconnect(); }
    void SourceShaderBuilderComponent::ShutDown() { m_stopping = true; }
    void SourceShaderBuilderComponent::CreateJobs(const AssetBuilderSDK::CreateJobsRequest& request,AssetBuilderSDK::CreateJobsResponse& response)
    {
        if (m_stopping) { response.m_result = AssetBuilderSDK::CreateJobsResultCode::ShuttingDown; return; }
        for (const auto& platform : request.m_enabledPlatforms)
        {
            if (platform.m_identifier == "pc")
            {
                AssetBuilderSDK::JobDescriptor job; job.m_jobKey = JobKey; job.SetPlatformIdentifier(platform.m_identifier.c_str());
                response.m_createJobOutputs.push_back(AZStd::move(job));
            }
        }
        response.m_result = AssetBuilderSDK::CreateJobsResultCode::Success;
    }
    void SourceShaderBuilderComponent::ProcessJob(const AssetBuilderSDK::ProcessJobRequest& request,AssetBuilderSDK::ProcessJobResponse& response)
    {
        AssetBuilderSDK::JobCancelListener listener(request.m_jobId);
        auto cancelled = [&]() { return m_stopping || listener.IsCancelled(); };
        bool passed = false;
#if defined(TG_SOURCE_SHADER_DX12)
        passed = request.m_platformInfo.m_identifier == "pc" && BuildShader(request,response,cancelled);
#endif
        response.m_resultCode = passed ? AssetBuilderSDK::ProcessJobResult_Success :
            cancelled() ? AssetBuilderSDK::ProcessJobResult_Cancelled : AssetBuilderSDK::ProcessJobResult_Failed;
        if (!passed)
        {
            response.m_outputProducts.clear();
            AZ_Error(JobKey,cancelled(),"Source shader packet is invalid, unsupported on this host, or could not build its native assets.");
        }
    }
}
