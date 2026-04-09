#include "Renderer/Pipeline.h"
#include <windows.h>

bool Pipeline::Init(ID3D11Device* device, const std::string& shaderPath)
{
    // Convert path to wide string
    int wlen = MultiByteToWideChar(CP_UTF8, 0, shaderPath.c_str(), -1, nullptr, 0);
    std::wstring wpath(wlen - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, shaderPath.c_str(), -1, wpath.data(), wlen);

    UINT compileFlags = 0;
#ifdef _DEBUG
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    // Compile vertex shader
    ComPtr<ID3DBlob> vsBlob, errBlob;
    HRESULT hr = D3DCompileFromFile(wpath.c_str(), nullptr, nullptr,
        "VSMain", "vs_5_0", compileFlags, 0, &vsBlob, &errBlob);
    if (FAILED(hr)) {
        if (errBlob) OutputDebugStringA((char*)errBlob->GetBufferPointer());
        return false;
    }

    // Compile pixel shader
    ComPtr<ID3DBlob> psBlob;
    hr = D3DCompileFromFile(wpath.c_str(), nullptr, nullptr,
        "PSMain", "ps_5_0", compileFlags, 0, &psBlob, &errBlob);
    if (FAILED(hr)) {
        if (errBlob) OutputDebugStringA((char*)errBlob->GetBufferPointer());
        return false;
    }

    hr = device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
        nullptr, &vs_);
    if (FAILED(hr)) return false;

    // Compile instanced vertex shader
    ComPtr<ID3DBlob> vsInstBlob;
    hr = D3DCompileFromFile(wpath.c_str(), nullptr, nullptr,
        "VSMainInstanced", "vs_5_0", compileFlags, 0, &vsInstBlob, &errBlob);
    if (FAILED(hr)) {
        if (errBlob) OutputDebugStringA((char*)errBlob->GetBufferPointer());
        return false;
    }
    hr = device->CreateVertexShader(vsInstBlob->GetBufferPointer(), vsInstBlob->GetBufferSize(),
        nullptr, &vsInstanced_);
    if (FAILED(hr)) return false;

    hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(),
        nullptr, &ps_);
    if (FAILED(hr)) return false;

    // Input layout: POSITION(12) + NORMAL(12) + TEXCOORD(8) = 32 bytes
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    hr = device->CreateInputLayout(layout, 3,
        vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &inputLayout_);
    if (FAILED(hr)) return false;

    // Constant buffers
    D3D11_BUFFER_DESC cbd = {};
    cbd.Usage = D3D11_USAGE_DYNAMIC;
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    cbd.ByteWidth = sizeof(PerFrameData);
    hr = device->CreateBuffer(&cbd, nullptr, &cbPerFrame_);
    if (FAILED(hr)) return false;

    cbd.ByteWidth = sizeof(PerObjectData);
    hr = device->CreateBuffer(&cbd, nullptr, &cbPerObject_);
    if (FAILED(hr)) return false;

    // Sampler
    D3D11_SAMPLER_DESC sd = {};
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sd.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sd.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
    hr = device->CreateSamplerState(&sd, &sampler_);
    if (FAILED(hr)) return false;

    return true;
}

void Pipeline::Bind(ID3D11DeviceContext* ctx)
{
    ctx->VSSetShader(vs_.Get(), nullptr, 0);
    ctx->PSSetShader(ps_.Get(), nullptr, 0);
    ctx->IASetInputLayout(inputLayout_.Get());
    ctx->PSSetSamplers(0, 1, sampler_.GetAddressOf());
}

void Pipeline::BindInstanced(ID3D11DeviceContext* ctx)
{
    // Same input layout — VS gets per-vertex data + SV_InstanceID (system value, no IA slot)
    ctx->VSSetShader(vsInstanced_.Get(), nullptr, 0);
    ctx->PSSetShader(ps_.Get(), nullptr, 0);
    ctx->IASetInputLayout(inputLayout_.Get());
    ctx->PSSetSamplers(0, 1, sampler_.GetAddressOf());
}

void Pipeline::UpdatePerFrame(ID3D11DeviceContext* ctx, const PerFrameData& data)
{
    D3D11_MAPPED_SUBRESOURCE mapped;
    ctx->Map(cbPerFrame_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    memcpy(mapped.pData, &data, sizeof(data));
    ctx->Unmap(cbPerFrame_.Get(), 0);
    ctx->VSSetConstantBuffers(0, 1, cbPerFrame_.GetAddressOf());
    ctx->PSSetConstantBuffers(0, 1, cbPerFrame_.GetAddressOf());
}

void Pipeline::UpdatePerObject(ID3D11DeviceContext* ctx, const PerObjectData& data)
{
    D3D11_MAPPED_SUBRESOURCE mapped;
    ctx->Map(cbPerObject_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    memcpy(mapped.pData, &data, sizeof(data));
    ctx->Unmap(cbPerObject_.Get(), 0);
    ctx->VSSetConstantBuffers(1, 1, cbPerObject_.GetAddressOf());
    ctx->PSSetConstantBuffers(1, 1, cbPerObject_.GetAddressOf());
}

