/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
// Isolated Unity diagnostic: read the selected shader-visible constant range without changing bindings.
#include <d3d11_1.h>
#include <wrl/client.h>
#include <IUnityGraphics.h>
#include <IUnityGraphicsD3D11.h>
#include <cstring>
using Microsoft::WRL::ComPtr;
namespace { IUnityGraphicsD3D11* graphics = nullptr; }
extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces* interfaces)
{
    auto* api = interfaces->Get<IUnityGraphics>();
    graphics = api && api->GetRenderer() == kUnityGfxRendererD3D11 ? interfaces->Get<IUnityGraphicsD3D11>() : nullptr;
}
extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginUnload() { graphics = nullptr; }
static int ReadDeviceConstants(ID3D11Device* device, int stage, int slot, int bytes, unsigned char* output, int capacity, unsigned int* metadata)
{
    if ((stage != 0 && stage != 1) || slot < 0 || slot >= 14 || bytes < 16 || bytes > 65536 || bytes % 16 ||
        !output || capacity < bytes || capacity > 65536 || !metadata) { return -1; }
    if (!device) { return -2; }
    ComPtr<ID3D11DeviceContext> context; device->GetImmediateContext(&context);
    ComPtr<ID3D11DeviceContext1> context1;
    if (FAILED(context.As(&context1))) { return -3; }
    ComPtr<ID3D11Buffer> buffer; UINT first = 0, count = 0;
    if (stage == 0) { context1->VSGetConstantBuffers1(UINT(slot),1,buffer.GetAddressOf(),&first,&count); }
    else { context1->PSGetConstantBuffers1(UINT(slot),1,buffer.GetAddressOf(),&first,&count); }
    if (!buffer) { return -4; }
    D3D11_BUFFER_DESC desc{}; buffer->GetDesc(&desc);
    const UINT64 begin = UINT64(first)*16, extent = UINT64(count)*16;
    if (desc.ByteWidth > 64*1024*1024 || !(desc.BindFlags & D3D11_BIND_CONSTANT_BUFFER) ||
        begin + UINT64(bytes) > desc.ByteWidth || UINT64(bytes) > extent) { return -5; }
    D3D11_BUFFER_DESC stagingDesc{}; stagingDesc.ByteWidth = UINT(bytes);
    stagingDesc.Usage = D3D11_USAGE_STAGING; stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Buffer> staging;
    if (FAILED(device->CreateBuffer(&stagingDesc,nullptr,staging.GetAddressOf()))) { return -6; }
    D3D11_BOX region{UINT(begin),0,0,UINT(begin)+UINT(bytes),1,1};
    context->CopySubresourceRegion(staging.Get(),0,0,0,0,buffer.Get(),0,&region);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped))) { return -7; }
    std::memcpy(output,mapped.pData,size_t(bytes)); context->Unmap(staging.Get(),0);
    metadata[0] = desc.ByteWidth; metadata[1] = first; metadata[2] = count; metadata[3] = UINT(bytes);
    return bytes;
}

extern "C" int __declspec(dllexport) ReadConstants(int stage, int slot, int bytes, unsigned char* output, int capacity, unsigned int* metadata)
{
    return ReadDeviceConstants(graphics ? graphics->GetDevice() : nullptr, stage, slot, bytes, output, capacity, metadata);
}
#ifdef FOA_MATERIAL_BINDING_SELF_TEST
#include <array>
#include <cstdio>
int main()
{
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    if (FAILED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,
        device.GetAddressOf(),nullptr,context.GetAddressOf()))) return 1;
    ComPtr<ID3D11DeviceContext1> context1;
    if (FAILED(context.As(&context1))) return 2;
    std::array<unsigned char,1024> input{};
    for (size_t i=0;i<input.size();++i) input[i]=static_cast<unsigned char>((i*37+i/256)%251);
    D3D11_BUFFER_DESC desc{}; desc.ByteWidth=UINT(input.size()); desc.Usage=D3D11_USAGE_DEFAULT;
    desc.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
    D3D11_SUBRESOURCE_DATA initial{input.data(),0,0}; ComPtr<ID3D11Buffer> buffer;
    if (FAILED(device->CreateBuffer(&desc,&initial,buffer.GetAddressOf()))) return 3;
    for(int stage=0;stage<2;++stage)
    {
        UINT first=UINT(stage+1)*16, count=16, slot=stage?13:0;
        ID3D11Buffer* raw=buffer.Get();
        if (stage==0) context1->VSSetConstantBuffers1(slot,1,&raw,&first,&count);
        else context1->PSSetConstantBuffers1(slot,1,&raw,&first,&count);
        std::array<unsigned char,272> output{}; unsigned int metadata[4]{};
        for(int bytes : {128,256})
        {
            output.fill(0x5a);
            if(ReadDeviceConstants(device.Get(),stage,int(slot),bytes,output.data(),int(output.size()),metadata)!=bytes ||
                std::memcmp(output.data(),input.data()+first*16,size_t(bytes)) || metadata[0]!=1024 ||
                metadata[1]!=first || metadata[2]!=count || metadata[3]!=UINT(bytes)) return 4;
            for(size_t i=size_t(bytes);i<output.size();++i) if(output[i]!=0x5a) return 5;
        }
        output.fill(0x5a); metadata[0]=123;
        if(ReadDeviceConstants(device.Get(),stage,int(slot),272,output.data(),272,metadata)!=-5 || metadata[0]!=123) return 6;
        for(auto value:output) if(value!=0x5a) return 7;
        ComPtr<ID3D11Buffer> unchanged; UINT actualFirst=0,actualCount=0;
        if(stage==0) context1->VSGetConstantBuffers1(slot,1,unchanged.GetAddressOf(),&actualFirst,&actualCount);
        else context1->PSGetConstantBuffers1(slot,1,unchanged.GetAddressOf(),&actualFirst,&actualCount);
        if(unchanged.Get()!=raw || actualFirst!=first || actualCount!=count) return 8;
        if(ReadDeviceConstants(device.Get(),stage,7,16,output.data(),272,metadata)!=-4 ||
            ReadDeviceConstants(device.Get(),stage,int(slot),17,output.data(),272,metadata)!=-1 ||
            ReadDeviceConstants(device.Get(),stage,int(slot),16,nullptr,272,metadata)!=-1 ||
            ReadDeviceConstants(device.Get(),stage,int(slot),16,output.data(),272,nullptr)!=-1 ||
            ReadDeviceConstants(nullptr,stage,int(slot),16,output.data(),272,metadata)!=-2) return 9;
    }
    std::puts("PASSED: VS/PS nonzero ranges, 128/256-byte copies, unchanged bindings/tails and rejected invalid reads.");
}
#endif
