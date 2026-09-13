/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
// Diagnostic Unity Editor plugin for synthetic D3D11 fixtures. Queries bindings; does not replace rendering state.
#include <d3d11.h>
#include <wrl/client.h>
#include <IUnityGraphics.h>
#include <IUnityGraphicsD3D11.h>
#include <array>
#include <algorithm>
#include <cstring>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
using Microsoft::WRL::ComPtr;
namespace
{
    IUnityGraphicsD3D11* graphics = nullptr;
    IUnityGraphics* unityGraphics = nullptr;
    std::array<std::string,64> results;
    std::mutex mutex;
    int firstEvent = -1;
    bool ReadTexture(ID3D11Device* device, ID3D11DeviceContext* context, ID3D11Texture2D* texture,
        const D3D11_TEXTURE2D_DESC& descriptor, std::string& output)
    {
        const UINT bytesPerTexel = descriptor.Format == DXGI_FORMAT_R32G32B32A32_FLOAT ? 16 :
            descriptor.Format == DXGI_FORMAT_R8G8B8A8_UNORM || descriptor.Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB ? 4 : 0;
        if (!bytesPerTexel || !descriptor.Width || !descriptor.Height || descriptor.Width > 64 || descriptor.Height > 64 ||
            !descriptor.MipLevels || descriptor.MipLevels > 7 || !descriptor.ArraySize || descriptor.ArraySize > 2 ||
            descriptor.SampleDesc.Count != 1) { return false; }
        size_t total = 0;
        for (UINT mip = 0; mip < descriptor.MipLevels; ++mip)
        { total += size_t((std::max)(1u,descriptor.Width >> mip)) * (std::max)(1u,descriptor.Height >> mip) * bytesPerTexel * descriptor.ArraySize; }
        if (total > 16384) { return false; }
        auto stagingDescriptor = descriptor;
        stagingDescriptor.Usage = D3D11_USAGE_STAGING; stagingDescriptor.BindFlags = 0;
        stagingDescriptor.CPUAccessFlags = D3D11_CPU_ACCESS_READ; stagingDescriptor.MiscFlags = 0;
        ComPtr<ID3D11Texture2D> staging;
        if (FAILED(device->CreateTexture2D(&stagingDescriptor,nullptr,staging.GetAddressOf()))) { return false; }
        context->CopyResource(staging.Get(),texture);
        std::ostringstream json; json << '['; bool comma = false;
        constexpr char hex[] = "0123456789abcdef";
        for (UINT layer = 0; layer < descriptor.ArraySize; ++layer)
        {
            for (UINT mip = 0; mip < descriptor.MipLevels; ++mip)
            {
                const UINT width = (std::max)(1u,descriptor.Width >> mip), height = (std::max)(1u,descriptor.Height >> mip);
                const UINT subresource = D3D11CalcSubresource(mip,layer,descriptor.MipLevels);
                D3D11_MAPPED_SUBRESOURCE mapped{};
                if (FAILED(context->Map(staging.Get(),subresource,D3D11_MAP_READ,0,&mapped))) { return false; }
                if (mapped.RowPitch < width * bytesPerTexel) { context->Unmap(staging.Get(),subresource); return false; }
                if (comma) { json << ','; } comma = true;
                json << "{\"layer\":" << layer << ",\"mip\":" << mip << ",\"width\":" << width << ",\"height\":" << height << ",\"hex\":\"";
                for (UINT y = 0; y < height; ++y)
                {
                    const auto* row = static_cast<const unsigned char*>(mapped.pData) + size_t(y)*mapped.RowPitch;
                    for (UINT x = 0; x < width * bytesPerTexel; ++x) { json << hex[row[x] >> 4] << hex[row[x] & 15]; }
                }
                context->Unmap(staging.Get(),subresource); json << "\"}";
            }
        }
        json << ']'; output = json.str(); return true;
    }
    void UNITY_INTERFACE_API Capture(int event)
    {
        const int index = event - firstEvent;
        if (index < 0 || index >= int(results.size())) { return; }
        std::string result;
        auto* device = graphics ? graphics->GetDevice() : nullptr;
        if (!device) { result = "{\"status\":\"NO_DEVICE\"}"; }
        else
        {
            ComPtr<ID3D11DeviceContext> context; device->GetImmediateContext(&context);
            ComPtr<ID3D11ShaderResourceView> view; ComPtr<ID3D11SamplerState> sampler;
            context->PSGetShaderResources(0,1,view.GetAddressOf()); context->PSGetSamplers(0,1,sampler.GetAddressOf());
            if (!view || !sampler) { result = "{\"status\":\"UNBOUND\"}"; }
            else
            {
                ComPtr<ID3D11Resource> resource; view->GetResource(&resource);
                ComPtr<ID3D11Texture2D> texture;
                if (FAILED(resource.As(&texture))) { result = "{\"status\":\"NOT_TEXTURE2D\"}"; }
                else
                {
                    D3D11_TEXTURE2D_DESC t{}; texture->GetDesc(&t);
                    D3D11_SHADER_RESOURCE_VIEW_DESC v{}; view->GetDesc(&v);
                    D3D11_SAMPLER_DESC s{}; sampler->GetDesc(&s);
                    UINT mip = 0, levels = 0, first = 0, count = 1;
                    if (v.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2D) { mip=v.Texture2D.MostDetailedMip; levels=v.Texture2D.MipLevels; }
                    else if (v.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2DARRAY)
                    { mip=v.Texture2DArray.MostDetailedMip; levels=v.Texture2DArray.MipLevels; first=v.Texture2DArray.FirstArraySlice; count=v.Texture2DArray.ArraySize; }
                    std::ostringstream json; json << std::setprecision(17);
                    json << "{\"status\":\"PASSED\",\"event\":" << event << ",\"index\":" << index;
                    json << ",\"texture\":[" << t.Width << ',' << t.Height << ',' << t.MipLevels << ',' << t.ArraySize << ',' << t.Format << ',' << t.SampleDesc.Count << ',' << t.SampleDesc.Quality << ',' << t.Usage << ',' << t.BindFlags << ',' << t.CPUAccessFlags << ',' << t.MiscFlags << ']';
                    json << ",\"view\":[" << v.Format << ',' << v.ViewDimension << ',' << mip << ',' << levels << ',' << first << ',' << count << ']';
                    json << ",\"sampler\":[" << s.Filter << ',' << s.AddressU << ',' << s.AddressV << ',' << s.AddressW << ',' << s.MipLODBias << ',' << s.MaxAnisotropy << ',' << s.ComparisonFunc;
                    for (float c : s.BorderColor) { json << ',' << c; }
                    json << ',' << s.MinLOD << ',' << s.MaxLOD << ']';
                    std::string texels;
                    if (!ReadTexture(device,context.Get(),texture.Get(),t,texels)) { result="{\"status\":\"READBACK_FAILED\"}"; }
                    else { json << ",\"subresources\":" << texels << '}'; result=json.str(); }
                }
            }
        }
        std::lock_guard<std::mutex> lock(mutex); results[index]=std::move(result);
    }
}
extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces* interfaces)
{
    graphics=interfaces->Get<IUnityGraphicsD3D11>(); unityGraphics=interfaces->Get<IUnityGraphics>();
    if (unityGraphics && unityGraphics->GetRenderer()==kUnityGfxRendererD3D11) { firstEvent=unityGraphics->ReserveEventIDRange(64); }
}
extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginUnload()
{
    std::lock_guard<std::mutex> lock(mutex); graphics=nullptr; unityGraphics=nullptr; firstEvent=-1;
    for (auto& result : results) { result.clear(); }
}
extern "C" UnityRenderingEvent __declspec(dllexport) GetCaptureEvent() { return &Capture; }
extern "C" void __declspec(dllexport) CaptureNow(int event) { Capture(event); }
extern "C" int __declspec(dllexport) GetFirstEvent() { return firstEvent; }
extern "C" int __declspec(dllexport) FetchCapture(int event, char* output, int capacity)
{
    const int index=event-firstEvent;
    if (index<0 || index>=int(results.size()) || !output || capacity<1 || capacity>65536) { return -1; }
    std::lock_guard<std::mutex> lock(mutex);
    const auto& value=results[index]; if (value.empty()) { return 0; }
    if (value.size()>=size_t(capacity)) { return -2; }
    memcpy(output,value.data(),value.size()); output[value.size()]=0; return int(value.size());
}
