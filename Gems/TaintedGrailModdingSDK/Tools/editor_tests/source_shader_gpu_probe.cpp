// Copyright (c) Contributors to the Open 3D Engine Project.
// For complete copyright and license terms please see the LICENSE at the root of this distribution.
// SPDX-License-Identifier: Apache-2.0 OR MIT
// Headless D3D11 test consumer. No window, game process, installation or O3DE mutation.
// Input is a bounded private fixture packet; stdout is execution evidence only.
#define NOMINMAX
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <d3d11.h>
#include <d3d11sdklayers.h>
#include <dxgi.h>
#include <wrl/client.h>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using Microsoft::WRL::ComPtr;
using Bytes = std::vector<uint8_t>;
void Require(bool okay, const char* message) { if (!okay) { throw std::runtime_error(message); } }
void Check(HRESULT value, const char* message)
{
    if (FAILED(value)) { throw std::runtime_error(std::string(message) + " HRESULT=" + std::to_string(value)); }
}
struct Reader
{
    Bytes data;
    size_t cursor = 0;
    explicit Reader(const wchar_t* path)
    {
        const auto size = std::filesystem::file_size(path);
        Require(size >= 16 && size <= 256 * 1024 * 1024, "Packet size exceeds bound");
        data.resize(static_cast<size_t>(size));
        std::ifstream file(path, std::ios::binary);
        Require(bool(file.read(reinterpret_cast<char*>(data.data()), data.size())), "Packet read failed");
    }
    const uint8_t* Take(size_t size)
    {
        Require(size <= data.size() - cursor, "Truncated packet");
        auto result = data.data() + cursor;
        cursor += size;
        return result;
    }
    uint32_t Uint(uint32_t low, uint32_t high)
    {
        uint32_t value;
        std::memcpy(&value, Take(4), 4);
        Require(value >= low && value <= high, "Packet integer exceeds bound");
        return value;
    }
    void Finish() { Require(cursor == data.size(), "Unclaimed packet bytes"); }
};
struct Shader { uint32_t stage; Bytes code; };
struct Constant { uint32_t stage, slot; Bytes data; };
struct Texture { uint32_t stage, slot, width, height; Bytes data; };
struct Sampler { uint32_t stage, slot, filter, u, v; };
struct Fixture
{
    uint32_t mode, width = 0, height = 0, vertices = 0;
    Bytes vertexData;
    std::vector<Shader> shaders;
    std::vector<Constant> constants;
    std::vector<Texture> textures;
    std::vector<Sampler> samplers;
    explicit Fixture(Reader& input)
    {
        Require(std::memcmp(input.Take(8), "FOAGPU01", 8) == 0, "Unknown packet version");
        mode = input.Uint(0, 1);
        const auto count = input.Uint(1, 8192);
        for (uint32_t index = 0; index < count; ++index)
        {
            const auto stage = input.Uint(1, 2);
            const auto size = input.Uint(32, 32 * 1024 * 1024);
            const auto data = input.Take(size);
            Require(std::memcmp(data, "DXBC", 4) == 0, "Expected DXBC container");
            shaders.push_back({stage, Bytes(data, data + size)});
        }
        if (mode == 1) { input.Finish(); return; }
        Require(count == 2 && shaders[0].stage == 1 && shaders[1].stage == 2, "Draw needs vertex/pixel pair");
        width = input.Uint(1, 1024); height = input.Uint(1, 1024);
        vertices = input.Uint(3, 65535);
        Require(vertices % 3 == 0, "Expected triangle list");
        auto data = input.Take(size_t(vertices) * 20);
        vertexData.assign(data, data + size_t(vertices) * 20);
        std::set<std::array<uint32_t, 3>> bindings;
        const auto claim = [&](uint32_t kind, uint32_t stage, uint32_t slot)
        { Require(bindings.insert({kind, stage, slot}).second, "Duplicate resource binding"); };
        const auto cbCount = input.Uint(0, 28);
        for (uint32_t index = 0; index < cbCount; ++index)
        {
            auto stage = input.Uint(1, 2), slot = input.Uint(0, 13), size = input.Uint(16, 65536);
            Require(size % 16 == 0, "Unaligned constant buffer");
            claim(0, stage, slot);
            data = input.Take(size);
            constants.push_back({stage, slot, Bytes(data, data + size)});
        }
        const auto txCount = input.Uint(0, 256);
        for (uint32_t index = 0; index < txCount; ++index)
        {
            auto stage = input.Uint(1, 2), slot = input.Uint(0, 127);
            auto w = input.Uint(1, 1024), h = input.Uint(1, 1024);
            claim(1, stage, slot);
            auto size = size_t(w) * h * 16;
            data = input.Take(size);
            textures.push_back({stage, slot, w, h, Bytes(data, data + size)});
        }
        const auto samplerCount = input.Uint(0, 32);
        for (uint32_t index = 0; index < samplerCount; ++index)
        {
            auto stage = input.Uint(1, 2), slot = input.Uint(0, 15);
            claim(2, stage, slot);
            samplers.push_back({stage, slot, input.Uint(0, 1), input.Uint(1, 3), input.Uint(1, 3)});
        }
        input.Finish();
    }
};

void CheckMessages(ID3D11InfoQueue* queue)
{
    const auto count = queue->GetNumStoredMessagesAllowedByRetrievalFilter();
    Require(count <= 4096, "D3D11 diagnostic count exceeds bound");
    bool failed = false;
    for (UINT64 index = 0; index < count; ++index)
    {
        SIZE_T size = 0;
        Check(queue->GetMessage(index, nullptr, &size), "Diagnostic size");
        Require(size <= 1024 * 1024, "D3D11 diagnostic size exceeds bound");
        Bytes bytes(size);
        auto message = reinterpret_cast<D3D11_MESSAGE*>(bytes.data());
        Check(queue->GetMessage(index, message, &size), "Diagnostic read");
        if (message->Severity <= D3D11_MESSAGE_SEVERITY_WARNING)
        {
            std::cerr << "D3D11 diagnostic " << message->ID << ": " << message->pDescription << '\n';
            failed = true;
        }
    }
    Require(!failed, "D3D11 reported warning/error");
}

int wmain(int argc, wchar_t** argv)
{
    try
    {
        Require(argc == 2, "Usage: probe packet");
        Reader input(argv[1]); Fixture fixture(input);
        ComPtr<ID3D11Device> device; ComPtr<ID3D11DeviceContext> context;
        D3D_FEATURE_LEVEL feature;
        const D3D_FEATURE_LEVEL requested[] = {D3D_FEATURE_LEVEL_11_0};
        Check(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_DEBUG,
            requested, 1, D3D11_SDK_VERSION, &device, &feature, &context), "Hardware debug device");
        ComPtr<ID3D11InfoQueue> queue; Check(device.As(&queue), "D3D11 debug queue");
        ComPtr<IDXGIDevice> dxgiDevice; Check(device.As(&dxgiDevice), "DXGI device");
        ComPtr<IDXGIAdapter> adapter; Check(dxgiDevice->GetAdapter(&adapter), "DXGI adapter");
        DXGI_ADAPTER_DESC adapterDesc{}; Check(adapter->GetDesc(&adapterDesc), "Adapter description");
        ComPtr<ID3D11VertexShader> vertexShader; ComPtr<ID3D11PixelShader> pixelShader;
        for (const auto& shader : fixture.shaders)
        {
            if (shader.stage == 1)
            { vertexShader.Reset(); Check(device->CreateVertexShader(shader.code.data(), shader.code.size(), nullptr, &vertexShader), "Create source vertex shader"); }
            else
            { pixelShader.Reset(); Check(device->CreatePixelShader(shader.code.data(), shader.code.size(), nullptr, &pixelShader), "Create source pixel shader"); }
        }
        Bytes pixels;
        if (fixture.mode == 0)
        {
            context->VSSetShader(vertexShader.Get(), nullptr, 0); context->PSSetShader(pixelShader.Get(), nullptr, 0);
            const D3D11_INPUT_ELEMENT_DESC elements[] = {
                {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
                {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}};
            ComPtr<ID3D11InputLayout> layout;
            Check(device->CreateInputLayout(elements, 2, fixture.shaders[0].code.data(), fixture.shaders[0].code.size(), &layout), "Input layout");
            context->IASetInputLayout(layout.Get());
            context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            D3D11_BUFFER_DESC vbDesc{}; vbDesc.ByteWidth = static_cast<UINT>(fixture.vertexData.size());
            vbDesc.Usage = D3D11_USAGE_IMMUTABLE; vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            D3D11_SUBRESOURCE_DATA vbData{fixture.vertexData.data(), 0, 0}; ComPtr<ID3D11Buffer> vb;
            Check(device->CreateBuffer(&vbDesc, &vbData, &vb), "Vertex buffer");
            UINT stride = 20, offset = 0; auto vbRaw = vb.Get(); context->IASetVertexBuffers(0, 1, &vbRaw, &stride, &offset);
            for (const auto& value : fixture.constants)
            {
                D3D11_BUFFER_DESC desc{}; desc.ByteWidth = static_cast<UINT>(value.data.size());
                desc.Usage = D3D11_USAGE_IMMUTABLE; desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
                D3D11_SUBRESOURCE_DATA data{value.data.data(), 0, 0}; ComPtr<ID3D11Buffer> buffer;
                Check(device->CreateBuffer(&desc, &data, &buffer), "Constant buffer"); auto raw = buffer.Get();
                if (value.stage == 1) { context->VSSetConstantBuffers(value.slot, 1, &raw); }
                else { context->PSSetConstantBuffers(value.slot, 1, &raw); }
            }
            for (const auto& value : fixture.textures)
            {
                D3D11_TEXTURE2D_DESC desc{}; desc.Width = value.width; desc.Height = value.height;
                desc.MipLevels = desc.ArraySize = desc.SampleDesc.Count = 1;
                desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT; desc.Usage = D3D11_USAGE_IMMUTABLE; desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
                D3D11_SUBRESOURCE_DATA data{value.data.data(), value.width * 16, 0}; ComPtr<ID3D11Texture2D> texture;
                Check(device->CreateTexture2D(&desc, &data, &texture), "Fixture texture");
                ComPtr<ID3D11ShaderResourceView> view; Check(device->CreateShaderResourceView(texture.Get(), nullptr, &view), "Texture view"); auto raw = view.Get();
                if (value.stage == 1) { context->VSSetShaderResources(value.slot, 1, &raw); }
                else { context->PSSetShaderResources(value.slot, 1, &raw); }
            }
            for (const auto& value : fixture.samplers)
            {
                D3D11_SAMPLER_DESC desc{};
                desc.Filter = value.filter == 0 ? D3D11_FILTER_MIN_MAG_MIP_POINT : D3D11_FILTER_MIN_MAG_MIP_LINEAR;
                desc.AddressU = static_cast<D3D11_TEXTURE_ADDRESS_MODE>(value.u); desc.AddressV = static_cast<D3D11_TEXTURE_ADDRESS_MODE>(value.v);
                desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP; desc.MaxAnisotropy = 1; desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
                ComPtr<ID3D11SamplerState> sampler; Check(device->CreateSamplerState(&desc, &sampler), "Fixture sampler"); auto raw = sampler.Get();
                if (value.stage == 1) { context->VSSetSamplers(value.slot, 1, &raw); }
                else { context->PSSetSamplers(value.slot, 1, &raw); }
            }
            D3D11_RASTERIZER_DESC rsDesc{}; rsDesc.FillMode = D3D11_FILL_SOLID; rsDesc.CullMode = D3D11_CULL_NONE; rsDesc.DepthClipEnable = TRUE;
            // Qualified by FoaTriangleFacingProbe for direct clip-space draws with GL.invertCulling=false.
            // Camera/pass inversion is an explicit provider input, not inferred by this fixture.
            rsDesc.FrontCounterClockwise = TRUE;
            ComPtr<ID3D11RasterizerState> rasterizer; Check(device->CreateRasterizerState(&rsDesc, &rasterizer), "Rasterizer"); context->RSSetState(rasterizer.Get());
            D3D11_DEPTH_STENCIL_DESC depthDesc{}; depthDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
            ComPtr<ID3D11DepthStencilState> depth; Check(device->CreateDepthStencilState(&depthDesc, &depth), "Depth state"); context->OMSetDepthStencilState(depth.Get(), 0);
            D3D11_VIEWPORT viewport{0, 0, float(fixture.width), float(fixture.height), 0, 1}; context->RSSetViewports(1, &viewport);
            D3D11_TEXTURE2D_DESC targetDesc{}; targetDesc.Width = fixture.width; targetDesc.Height = fixture.height;
            targetDesc.MipLevels = targetDesc.ArraySize = targetDesc.SampleDesc.Count = 1;
            targetDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT; targetDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
            ComPtr<ID3D11Texture2D> target; Check(device->CreateTexture2D(&targetDesc, nullptr, &target), "Float render target");
            ComPtr<ID3D11RenderTargetView> rtv; Check(device->CreateRenderTargetView(target.Get(), nullptr, &rtv), "Target view"); auto rawRtv = rtv.Get();
            context->OMSetRenderTargets(1, &rawRtv, nullptr);
            const float clear[] = {-100, -100, -100, -100}; context->ClearRenderTargetView(rtv.Get(), clear);
            context->Draw(fixture.vertices, 0);
            targetDesc.Usage = D3D11_USAGE_STAGING; targetDesc.BindFlags = 0; targetDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            ComPtr<ID3D11Texture2D> staging; Check(device->CreateTexture2D(&targetDesc, nullptr, &staging), "Readback staging"); context->CopyResource(staging.Get(), target.Get());
            D3D11_QUERY_DESC queryDesc{D3D11_QUERY_EVENT, 0}; ComPtr<ID3D11Query> query;
            Check(device->CreateQuery(&queryDesc, &query), "Completion query"); context->End(query.Get()); context->Flush();
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
            while (true)
            {
                HRESULT result = context->GetData(query.Get(), nullptr, 0, 0); Check(result, "GPU completion");
                if (result == S_OK) { break; }
                Require(std::chrono::steady_clock::now() < deadline, "GPU completion timed out");
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            D3D11_MAPPED_SUBRESOURCE mapped{}; Check(context->Map(staging.Get(), 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &mapped), "GPU readback");
            pixels.resize(size_t(fixture.width) * fixture.height * 16);
            for (uint32_t row = 0; row < fixture.height; ++row)
            { std::memcpy(pixels.data() + size_t(row) * fixture.width * 16, static_cast<const uint8_t*>(mapped.pData) + size_t(row) * mapped.RowPitch, size_t(fixture.width) * 16); }
            context->Unmap(staging.Get(), 0);
        }
        CheckMessages(queue.Get()); Check(device->GetDeviceRemovedReason(), "Device removed");
        context->ClearState(); context->Flush();
        std::cout << "{\"status\":\"PASSED\",\"api\":\"D3D11\",\"hardware\":true,\"debug_layer\":true,\"warnings_errors\":0,\"vendor_id\":"
            << adapterDesc.VendorId << ",\"device_id\":" << adapterDesc.DeviceId << ",\"feature_level\":" << feature
            << ",\"shaders_created\":" << fixture.shaders.size() << ",\"draw_executed\":" << (fixture.mode == 0 ? "true" : "false") << "}\n";
        std::cout.flush();
        if (fixture.mode == 0)
        {
            Require(_setmode(_fileno(stdout), _O_BINARY) != -1, "Binary stdout mode failed");
            Bytes output(16 + pixels.size()); std::memcpy(output.data(), "FOAPIX01", 8);
            std::memcpy(output.data() + 8, &fixture.width, 4); std::memcpy(output.data() + 12, &fixture.height, 4);
            std::memcpy(output.data() + 16, pixels.data(), pixels.size());
            Require(std::fwrite(output.data(), 1, output.size(), stdout) == output.size(), "Pixel stdout failed");
            Require(std::fflush(stdout) == 0, "Pixel stdout flush failed");
        }
        return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
