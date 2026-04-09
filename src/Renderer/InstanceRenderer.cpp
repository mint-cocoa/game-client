#include "Renderer/InstanceRenderer.h"
#include <algorithm>
#include <windows.h>

using namespace DirectX;

static ComPtr<ID3D11ShaderResourceView> CreateSRV(ID3D11Device* device, ID3D11Buffer* buf, uint32_t numElements)
{
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = numElements;

    ComPtr<ID3D11ShaderResourceView> srv;
    HRESULT hr = device->CreateShaderResourceView(buf, &srvDesc, &srv);
    if (FAILED(hr)) {
        OutputDebugStringA("[InstanceRenderer] Failed to create SRV\n");
    }
    return srv;
}

void InstanceRenderer::Init(ID3D11Device* device)
{
    // Create the 1-element structured buffer for single-entity draws
    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = sizeof(XMFLOAT4X4);
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    bd.StructureByteStride = sizeof(XMFLOAT4X4); // 64 bytes

    HRESULT hr = device->CreateBuffer(&bd, nullptr, &singleBuffer_);
    if (FAILED(hr)) {
        OutputDebugStringA("[InstanceRenderer] Failed to create single buffer\n");
        return;
    }
    singleSRV_ = CreateSRV(device, singleBuffer_.Get(), 1);
}

void InstanceRenderer::EnsureBuffer(ID3D11Device* device, uint32_t neededCount)
{
    if (neededCount == 0) return;
    if (neededCount <= bufferCapacity_) return;

    // Grow with some headroom to avoid frequent reallocations
    uint32_t newCapacity = (neededCount < 256) ? 256 : ((neededCount * 3) / 2);

    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = newCapacity * sizeof(XMFLOAT4X4);
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    bd.StructureByteStride = sizeof(XMFLOAT4X4);

    instanceBuffer_.Reset();
    instanceSRV_.Reset();

    HRESULT hr = device->CreateBuffer(&bd, nullptr, &instanceBuffer_);
    if (FAILED(hr)) {
        OutputDebugStringA("[InstanceRenderer] Failed to create instance buffer\n");
        bufferCapacity_ = 0;
        return;
    }

    instanceSRV_ = CreateSRV(device, instanceBuffer_.Get(), newCapacity);
    bufferCapacity_ = newCapacity;

}

void InstanceRenderer::Prepare(ID3D11Device* device, ID3D11DeviceContext* ctx,
                                const std::vector<std::pair<std::string, XMFLOAT4X4>>& instances)
{
    batches_.clear();
    sortedWorlds_.clear();

    if (instances.empty()) return;

    // Build sorted index by mesh name for grouping
    std::vector<size_t> indices(instances.size());
    for (size_t i = 0; i < indices.size(); ++i) indices[i] = i;

    std::sort(indices.begin(), indices.end(), [&](size_t a, size_t b) {
        return instances[a].first < instances[b].first;
    });

    // Fill sortedWorlds and record batch info
    sortedWorlds_.reserve(instances.size());
    const std::string* currentMesh = nullptr;
    uint32_t batchStart = 0;

    for (size_t i = 0; i < indices.size(); ++i) {
        const auto& inst = instances[indices[i]];
        sortedWorlds_.push_back(inst.second);

        if (!currentMesh || *currentMesh != inst.first) {
            // Close previous batch
            if (currentMesh) {
                batches_[*currentMesh] = { batchStart, (uint32_t)(i - batchStart) };
            }
            currentMesh = &inst.first;
            batchStart = (uint32_t)i;
        }
    }
    // Close last batch
    if (currentMesh) {
        batches_[*currentMesh] = { batchStart, (uint32_t)(sortedWorlds_.size() - batchStart) };
    }

    // Upload to GPU
    EnsureBuffer(device, (uint32_t)sortedWorlds_.size());

    if (instanceBuffer_) {
        D3D11_MAPPED_SUBRESOURCE mapped;
        HRESULT hr = ctx->Map(instanceBuffer_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        if (SUCCEEDED(hr)) {
            memcpy(mapped.pData, sortedWorlds_.data(), sortedWorlds_.size() * sizeof(XMFLOAT4X4));
            ctx->Unmap(instanceBuffer_.Get(), 0);
        }
    }
}

void InstanceRenderer::Bind(ID3D11DeviceContext* ctx)
{
    if (instanceSRV_) {
        ctx->VSSetShaderResources(1, 1, instanceSRV_.GetAddressOf());
    }
}

void InstanceRenderer::BindSingle(ID3D11DeviceContext* ctx, const XMFLOAT4X4& world)
{
    if (!singleBuffer_) return;

    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = ctx->Map(singleBuffer_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (SUCCEEDED(hr)) {
        memcpy(mapped.pData, &world, sizeof(XMFLOAT4X4));
        ctx->Unmap(singleBuffer_.Get(), 0);
    }

    ctx->VSSetShaderResources(1, 1, singleSRV_.GetAddressOf());
}
