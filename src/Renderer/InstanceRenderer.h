#pragma once
#include <d3d11.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <vector>
#include <string>
#include <unordered_map>

using Microsoft::WRL::ComPtr;

class InstanceRenderer {
public:
    void Init(ID3D11Device* device);

    // Group instances by mesh, upload world matrices to structured buffer
    void Prepare(ID3D11Device* device, ID3D11DeviceContext* ctx,
                 const std::vector<std::pair<std::string, DirectX::XMFLOAT4X4>>& instances);

    struct BatchInfo { uint32_t startIndex; uint32_t count; };
    const std::unordered_map<std::string, BatchInfo>& GetBatches() const { return batches_; }

    // Bind instance SRV to VS slot 1
    void Bind(ID3D11DeviceContext* ctx);

    // Upload a single world matrix for non-instanced entities and bind
    void BindSingle(ID3D11DeviceContext* ctx, const DirectX::XMFLOAT4X4& world);

private:
    void EnsureBuffer(ID3D11Device* device, uint32_t neededCount);

    ComPtr<ID3D11Buffer> instanceBuffer_;
    ComPtr<ID3D11ShaderResourceView> instanceSRV_;
    uint32_t bufferCapacity_ = 0;

    // Small 1-element buffer for single-entity draws
    ComPtr<ID3D11Buffer> singleBuffer_;
    ComPtr<ID3D11ShaderResourceView> singleSRV_;

    std::unordered_map<std::string, BatchInfo> batches_;
    std::vector<DirectX::XMFLOAT4X4> sortedWorlds_;
};
