#pragma once
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <string>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

struct PerFrameData {
    XMFLOAT4X4 ViewProj;
    XMFLOAT3 LightDir;
    float _pad0;
    XMFLOAT3 AmbientColor;
    float _pad1;
};

struct PerObjectData {
    XMFLOAT4X4 World;
    float    HitFlash = 0.0f;       // 0=normal, 1=full red flash
    uint32_t InstanceOffset = 0;    // start index in InstanceWorlds for current batch
    float    _obj_pad[2] = {};
};

class Pipeline {
public:
    bool Init(ID3D11Device* device, const std::string& shaderPath);
    void Bind(ID3D11DeviceContext* ctx);          // non-instanced VS
    void BindInstanced(ID3D11DeviceContext* ctx); // instanced VS (uses InstanceWorlds @ t1)
    void UpdatePerFrame(ID3D11DeviceContext* ctx, const PerFrameData& data);
    void UpdatePerObject(ID3D11DeviceContext* ctx, const PerObjectData& data);

private:
    ComPtr<ID3D11VertexShader> vs_;
    ComPtr<ID3D11VertexShader> vsInstanced_;
    ComPtr<ID3D11PixelShader> ps_;
    ComPtr<ID3D11InputLayout> inputLayout_;
    ComPtr<ID3D11Buffer> cbPerFrame_;
    ComPtr<ID3D11Buffer> cbPerObject_;
    ComPtr<ID3D11SamplerState> sampler_;
};
