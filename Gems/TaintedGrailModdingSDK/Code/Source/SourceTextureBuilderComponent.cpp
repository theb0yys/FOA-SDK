/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#include "SourceTextureBuilderComponent.h"
#include <AssetBuilderSDK/AssetBuilderBusses.h>
#include <AzCore/IO/SystemFile.h>
#include <AzCore/IO/Path/Path.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContextConstants.inl>
#include <AzCore/Serialization/Utils.h>
#include <Atom/RPI.Reflect/Image/StreamingImageAssetCreator.h>
#include <Atom/RPI.Reflect/Image/ImageMipChainAssetCreator.h>
#include <Atom/RHI.Reflect/ImageSubresource.h>
#include <QCryptographicHash>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        constexpr AZ::u64 MaxBytes = 128 * 1024 * 1024;
        constexpr AZ::u32 MaxDimension = 8192;
        constexpr size_t HeaderSize = 64;
        constexpr size_t ArrayHeaderSize = 68;
        constexpr AZ::u32 MaxLayers = 256;
        constexpr const char* JobKey = "FOA Source Texture";
        AZ::u32 Read32(const AZStd::vector<AZ::u8>& bytes, size_t offset)
        {
            return bytes[offset] | (AZ::u32(bytes[offset+1]) << 8) |
                (AZ::u32(bytes[offset+2]) << 16) | (AZ::u32(bytes[offset+3]) << 24);
        }
        struct Mip
        {
            AZ::RHI::DeviceImageSubresourceLayout m_layout;
            size_t m_offset = 0;
            size_t m_sourceSize = 0;
        };

        // No source payload is interpreted as colour pixels here. RGB24 receives
        // an explicit opaque lane because RHI/DX12 has no three-byte RGB format.
        bool BuildImage(const AssetBuilderSDK::ProcessJobRequest& request,
            AssetBuilderSDK::ProcessJobResponse& response, const AZStd::function<bool()>& cancelled)
        {
            AZ::IO::SystemFile input;
            if (!input.Open(request.m_fullPath.c_str(), AZ::IO::SystemFile::SF_OPEN_READ_ONLY))
            {
                return false;
            }
            const auto fileSize = input.Length();
            if (fileSize < HeaderSize || fileSize > MaxBytes + ArrayHeaderSize || cancelled())
            {
                return false;
            }
            AZStd::vector<AZ::u8> bytes(static_cast<size_t>(fileSize));
            if (input.Read(fileSize, bytes.data()) != fileSize || input.Length() != fileSize)
            {
                return false;
            }
            const AZ::u32 version = Read32(bytes, 8);
            const bool isArray = version == 2;
            const size_t headerSize = isArray ? ArrayHeaderSize : HeaderSize;
            if (memcmp(bytes.data(), "FOATEX01", 8) != 0 || (version != 1 && version != 2) ||
                bytes.size() < headerSize || bytes.size() > MaxBytes + headerSize)
            {
                return false;
            }
            QCryptographicHash hash(QCryptographicHash::Sha256);
            hash.addData(QByteArrayView(reinterpret_cast<const char*>(bytes.data()), headerSize-32));
            hash.addData(QByteArrayView(reinterpret_cast<const char*>(bytes.data()+headerSize), bytes.size()-headerSize));
            const auto digest = hash.result();
            if (memcmp(digest.constData(), bytes.data()+headerSize-32, 32) != 0)
            {
                return false;
            }
            const AZ::u32 width = Read32(bytes, 12), height = Read32(bytes, 16);
            const AZ::u32 count = Read32(bytes, 20), sourceFormat = Read32(bytes, 24), color = Read32(bytes, 28);
            const AZ::u32 layers = isArray ? Read32(bytes, 32) : 1;
            if (!layers || layers > MaxLayers || !width || !height || width > MaxDimension || height > MaxDimension || !count || count > 14 || color > 1)
            {
                return false;
            }
            AZ::u32 maxMips = 1;
            for (AZ::u32 extent = AZStd::max(width, height); extent > 1; extent /= 2)
            {
                ++maxMips;
            }
            if (count > maxMips)
            {
                return false;
            }
            using AZ::RHI::Format;
            Format format = Format::Unknown;
            AZ::u32 unitBytes = 0;
            switch (sourceFormat)
            {
            case 1:
                // Alpha-only samples retain zero RGB; data colour-space flags do not gamma-transform alpha.
                format = Format::A8_UNORM;
                unitBytes = 1;
                break;
            case 3: case 4:
                format = color ? Format::R8G8B8A8_UNORM_SRGB : Format::R8G8B8A8_UNORM;
                unitBytes = sourceFormat;
                break;
            case 10:
                format = color ? Format::BC1_UNORM_SRGB : Format::BC1_UNORM;
                unitBytes = 8;
                break;
            case 12:
                format = color ? Format::BC3_UNORM_SRGB : Format::BC3_UNORM;
                unitBytes = 16;
                break;
            case 26:
                if (color) { return false; }
                format = Format::BC4_UNORM;
                unitBytes = 8;
                break;
            default: return false;
            }
            AZStd::vector<Mip> mips;
            size_t offset = headerSize, nativeBytes = 0;
            AZ::u32 mipWidth = width, mipHeight = height;
            for (AZ::u32 mip = 0; mip < count; ++mip)
            {
                Mip item;
                item.m_offset = offset;
                item.m_sourceSize = sourceFormat <= 4 ? size_t(mipWidth)*mipHeight*unitBytes :
                    size_t((mipWidth+3)/4)*((mipHeight+3)/4)*unitBytes;
                item.m_layout = AZ::RHI::GetImageSubresourceLayout(AZ::RHI::Size(mipWidth, mipHeight, 1), format);
                const size_t expectedNative = sourceFormat == 3 ? size_t(mipWidth)*mipHeight*4 : item.m_sourceSize;
                if (item.m_layout.m_bytesPerImage != expectedNative || offset + item.m_sourceSize*layers > bytes.size())
                {
                    return false;
                }
                offset += item.m_sourceSize*layers;
                nativeBytes += expectedNative*layers;
                mips.push_back(item);
                mipWidth = AZStd::max(1u, mipWidth/2);
                mipHeight = AZStd::max(1u, mipHeight/2);
            }
            if (offset != bytes.size() || nativeBytes > MaxBytes || cancelled())
            {
                return false;
            }
            AZ::RPI::StreamingImageAssetCreator image;
            // MaterialUtils resolves image source paths using this engine-owned sub-ID.
            constexpr AZ::u32 imageSubId = AZ::RPI::StreamingImageAsset::GetImageAssetSubId();
            image.Begin(AZ::Data::AssetId(request.m_sourceFileUUID, imageSubId));
            auto descriptor = isArray ? AZ::RHI::ImageDescriptor::Create2DArray(
                AZ::RHI::ImageBindFlags::ShaderRead, width, height, static_cast<AZ::u16>(layers), format) :
                AZ::RHI::ImageDescriptor::Create2D(AZ::RHI::ImageBindFlags::ShaderRead, width, height, format);
            descriptor.m_mipLevels = static_cast<AZ::u16>(count);
            image.SetImageDescriptor(descriptor);
            if (isArray)
            {
                AZ::RHI::ImageViewDescriptor view;
                // One-layer arrays must remain Texture2DArray SRVs, not Texture2D.
                view.m_isArray = 1;
                view.m_arraySliceMax = static_cast<AZ::u16>(layers-1);
                image.SetImageViewDescriptor(view);
            }
            AssetBuilderSDK::JobProduct product;
            product.m_dependenciesHandled = true;
            const AZStd::string stem = AZ::IO::Path(request.m_fullPath).Filename().String();
            size_t remaining = nativeBytes;
            AZ::u32 first = 0, chainIndex = 0;
            while (first < count)
            {
                if (cancelled()) { return false; }
                // At most 64 KiB stays resident; all higher mips are streamable.
                const AZ::u32 end = remaining <= 64*1024 || first+1 == count ? count : first+1;
                const bool tail = end == count;
                const AZ::Data::AssetId chainId(request.m_sourceFileUUID, imageSubId + ++chainIndex);
                AZ::RPI::ImageMipChainAssetCreator chain;
                chain.Begin(chainId, static_cast<AZ::u16>(end-first), static_cast<AZ::u16>(layers));
                for (AZ::u32 mip = first; mip < end; ++mip)
                {
                    if (cancelled()) { return false; }
                    const auto& item = mips[mip];
                    chain.BeginMip(item.m_layout);
                    for (AZ::u32 layer = 0; layer < layers; ++layer)
                    {
                        if (cancelled()) { return false; }
                        const AZ::u8* data = bytes.data()+item.m_offset+layer*item.m_sourceSize;
                        AZStd::vector<AZ::u8> expanded;
                        if (sourceFormat == 3)
                        {
                            expanded.resize(item.m_layout.m_bytesPerImage);
                            for (size_t pixel = 0; pixel < item.m_sourceSize/3; ++pixel)
                            {
                                if (pixel % 4096 == 0 && cancelled()) { return false; }
                                memcpy(expanded.data()+pixel*4, data+pixel*3, 3);
                                expanded[pixel*4+3] = 255;
                            }
                            data = expanded.data();
                        }
                        chain.AddSubImage(data, item.m_layout.m_bytesPerImage);
                        remaining -= item.m_layout.m_bytesPerImage;
                    }
                    chain.EndMip();
                }
                AZ::Data::Asset<AZ::RPI::ImageMipChainAsset> chainAsset;
                if (!chain.End(chainAsset)) { return false; }
                image.AddMipChainAsset(*chainAsset.Get());
                if (!tail)
                {
                    const auto output = AZ::IO::Path(request.m_tempDirPath) /
                        AZStd::string::format("%s.%u.imagemipchain", stem.c_str(), chainIndex);
                    if (!AZ::Utils::SaveObjectToFile(output.Native(), AZ::DataStream::ST_BINARY, chainAsset.Get())) { return false; }
                    AssetBuilderSDK::JobProduct chainProduct(output.Native(), azrtti_typeid<AZ::RPI::ImageMipChainAsset>(), imageSubId + chainIndex);
                    chainProduct.m_dependenciesHandled = true;
                    response.m_outputProducts.push_back(AZStd::move(chainProduct));
                    product.m_dependencies.emplace_back(chainId,
                        AZ::Data::ProductDependencyInfo::CreateFlags(AZ::Data::AssetLoadBehavior::NoLoad));
                }
                first = end;
            }
            if (chainIndex == 1) { image.SetFlags(AZ::RPI::StreamingImageFlags::NotStreamable); }
            AZ::Data::Asset<AZ::RPI::StreamingImageAsset> asset;
            if (!image.End(asset) || cancelled()) { return false; }
            const auto output = AZ::IO::Path(request.m_tempDirPath) / (stem + ".streamingimage");
            if (!AZ::Utils::SaveObjectToFile(output.Native(), AZ::DataStream::ST_BINARY, asset.Get())) { return false; }
            product.m_productFileName = output.Native();
            product.m_productSubID = imageSubId;
            product.m_productAssetType = azrtti_typeid<AZ::RPI::StreamingImageAsset>();
            response.m_outputProducts.push_back(AZStd::move(product));
            return true;
        }
    }

    void SourceTextureBuilderComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serialize = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serialize->Class<SourceTextureBuilderComponent, AZ::Component>()->Version(1)
                ->Attribute(AZ::Edit::Attributes::SystemComponentTags,
                    AZStd::vector<AZ::Crc32>{AssetBuilderSDK::ComponentTags::AssetBuilder});
        }
    }
    void SourceTextureBuilderComponent::Activate()
    {
        m_stopping = false;
        AssetBuilderSDK::AssetBuilderDesc descriptor;
        descriptor.m_name = JobKey;
        descriptor.m_busId = azrtti_typeid<SourceTextureBuilderComponent>();
        descriptor.m_version = 4;
        descriptor.m_patterns.emplace_back("*.foatexture", AssetBuilderSDK::AssetBuilderPattern::PatternType::Wildcard);
        descriptor.m_createJobFunction = [this](const auto& request, auto& response) { CreateJobs(request, response); };
        descriptor.m_processJobFunction = [this](const auto& request, auto& response) { ProcessJob(request, response); };
        BusConnect(descriptor.m_busId);
        AssetBuilderSDK::AssetBuilderBus::Broadcast(&AssetBuilderSDK::AssetBuilderBusTraits::RegisterBuilderInformation, descriptor);
    }
    void SourceTextureBuilderComponent::Deactivate() { BusDisconnect(); }
    void SourceTextureBuilderComponent::ShutDown() { m_stopping = true; }
    void SourceTextureBuilderComponent::CreateJobs(const AssetBuilderSDK::CreateJobsRequest& request, AssetBuilderSDK::CreateJobsResponse& response)
    {
        if (m_stopping) { response.m_result = AssetBuilderSDK::CreateJobsResultCode::ShuttingDown; return; }
        for (const auto& platform : request.m_enabledPlatforms)
        {
            if (platform.m_identifier == "pc")
            {
                AssetBuilderSDK::JobDescriptor job;
                job.m_jobKey = JobKey;
                job.SetPlatformIdentifier(platform.m_identifier.c_str());
                job.m_critical = false;
                response.m_createJobOutputs.push_back(AZStd::move(job));
            }
        }
        response.m_result = AssetBuilderSDK::CreateJobsResultCode::Success;
    }
    void SourceTextureBuilderComponent::ProcessJob(const AssetBuilderSDK::ProcessJobRequest& request, AssetBuilderSDK::ProcessJobResponse& response)
    {
        AssetBuilderSDK::JobCancelListener listener(request.m_jobId);
        auto cancelled = [&]() { return m_stopping || listener.IsCancelled(); };
        const bool passed = request.m_platformInfo.m_identifier == "pc" && BuildImage(request, response, cancelled);
        response.m_resultCode = passed ? AssetBuilderSDK::ProcessJobResult_Success :
            cancelled() ? AssetBuilderSDK::ProcessJobResult_Cancelled : AssetBuilderSDK::ProcessJobResult_Failed;
        if (!passed)
        {
            response.m_outputProducts.clear();
            AZ_Error(JobKey, cancelled(), "Source texture projection failed validation or native asset construction.");
        }
    }
}
