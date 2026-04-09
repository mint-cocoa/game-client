#pragma once
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <vector>
#include <string>
#include "Game/CombatManager.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

struct EffectCB {
    XMFLOAT3 EffectPos;
    float    EffectTime;
    XMFLOAT3 CameraRight;
    float    EffectScale;
    XMFLOAT3 CameraUp;
    int      EffectType;
    XMFLOAT4 EffectColor;
};

class EffectRenderer {
public:
    bool Init(ID3D11Device* device, const std::string& shaderPath);
    void Render(ID3D11DeviceContext* ctx,
                const XMMATRIX& viewProj,
                const XMFLOAT3& camRight,
                const XMFLOAT3& camUp,
                const std::vector<Projectile>& projectiles,
                const std::vector<AttackEffect>& effects,
                const std::vector<HitEffect>& hitEffects);

private:
    void DrawQuad(ID3D11DeviceContext* ctx, const XMMATRIX& viewProj,
                  const XMFLOAT3& camRight, const XMFLOAT3& camUp,
                  const XMFLOAT3& pos, float scale, float time,
                  int type, const XMFLOAT4& color);

    ComPtr<ID3D11VertexShader>   vs_;
    ComPtr<ID3D11PixelShader>    ps_;
    ComPtr<ID3D11InputLayout>    inputLayout_;
    ComPtr<ID3D11Buffer>         quadVB_;
    ComPtr<ID3D11Buffer>         quadIB_;
    ComPtr<ID3D11Buffer>         cbPerFrame_;
    ComPtr<ID3D11Buffer>         cbEffect_;
    ComPtr<ID3D11BlendState>     blendState_;
    ComPtr<ID3D11DepthStencilState> dsState_;
};
