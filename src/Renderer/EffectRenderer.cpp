#include "Renderer/EffectRenderer.h"
#include <windows.h>

struct EffectVertex {
    XMFLOAT2 Pos;
    XMFLOAT2 UV;
};

bool EffectRenderer::Init(ID3D11Device* device, const std::string& shaderPath)
{
    // Compile shaders
    int wlen = MultiByteToWideChar(CP_UTF8, 0, shaderPath.c_str(), -1, nullptr, 0);
    std::wstring wpath(wlen - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, shaderPath.c_str(), -1, wpath.data(), wlen);

    UINT flags = 0;
#ifdef _DEBUG
    flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ComPtr<ID3DBlob> vsBlob, psBlob, errBlob;
    HRESULT hr = D3DCompileFromFile(wpath.c_str(), nullptr, nullptr,
        "VSMain", "vs_5_0", flags, 0, &vsBlob, &errBlob);
    if (FAILED(hr)) {
        if (errBlob) OutputDebugStringA((char*)errBlob->GetBufferPointer());
        return false;
    }

    hr = D3DCompileFromFile(wpath.c_str(), nullptr, nullptr,
        "PSMain", "ps_5_0", flags, 0, &psBlob, &errBlob);
    if (FAILED(hr)) {
        if (errBlob) OutputDebugStringA((char*)errBlob->GetBufferPointer());
        return false;
    }

    device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vs_);
    device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &ps_);

    // Input layout: POSITION(float2) + TEXCOORD(float2)
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,  8, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    device->CreateInputLayout(layout, 2,
        vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &inputLayout_);

    // Quad VB: 4 vertices
    EffectVertex quadVerts[] = {
        { {-1, -1}, {0, 1} },
        { {-1,  1}, {0, 0} },
        { { 1,  1}, {1, 0} },
        { { 1, -1}, {1, 1} },
    };
    D3D11_BUFFER_DESC vbd = {};
    vbd.ByteWidth = sizeof(quadVerts);
    vbd.Usage = D3D11_USAGE_IMMUTABLE;
    vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vsd = { quadVerts };
    device->CreateBuffer(&vbd, &vsd, &quadVB_);

    // Quad IB: 2 triangles
    uint16_t indices[] = { 0, 1, 2, 0, 2, 3 };
    D3D11_BUFFER_DESC ibd = {};
    ibd.ByteWidth = sizeof(indices);
    ibd.Usage = D3D11_USAGE_IMMUTABLE;
    ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA isd = { indices };
    device->CreateBuffer(&ibd, &isd, &quadIB_);

    // Constant buffers
    D3D11_BUFFER_DESC cbd = {};
    cbd.Usage = D3D11_USAGE_DYNAMIC;
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    cbd.ByteWidth = sizeof(XMFLOAT4X4);  // ViewProj
    device->CreateBuffer(&cbd, nullptr, &cbPerFrame_);

    cbd.ByteWidth = sizeof(EffectCB);
    device->CreateBuffer(&cbd, nullptr, &cbEffect_);

    // Blend state: Additive
    D3D11_BLEND_DESC bd = {};
    bd.RenderTarget[0].BlendEnable = TRUE;
    bd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    bd.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
    bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    device->CreateBlendState(&bd, &blendState_);

    // Depth stencil: read only, no write
    D3D11_DEPTH_STENCIL_DESC dsd = {};
    dsd.DepthEnable = TRUE;
    dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    dsd.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    device->CreateDepthStencilState(&dsd, &dsState_);

    return true;
}

void EffectRenderer::DrawQuad(ID3D11DeviceContext* ctx, const XMMATRIX& viewProj,
                               const XMFLOAT3& camRight, const XMFLOAT3& camUp,
                               const XMFLOAT3& pos, float scale, float time,
                               int type, const XMFLOAT4& color)
{
    // Update PerFrame (ViewProj)
    {
        D3D11_MAPPED_SUBRESOURCE mapped;
        ctx->Map(cbPerFrame_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        XMFLOAT4X4 vp;
        XMStoreFloat4x4(&vp, viewProj);
        memcpy(mapped.pData, &vp, sizeof(vp));
        ctx->Unmap(cbPerFrame_.Get(), 0);
    }

    // Update EffectParams
    {
        EffectCB cb;
        cb.EffectPos = pos;
        cb.EffectTime = time;
        cb.CameraRight = camRight;
        cb.EffectScale = scale;
        cb.CameraUp = camUp;
        cb.EffectType = type;
        cb.EffectColor = color;

        D3D11_MAPPED_SUBRESOURCE mapped;
        ctx->Map(cbEffect_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        memcpy(mapped.pData, &cb, sizeof(cb));
        ctx->Unmap(cbEffect_.Get(), 0);
    }

    ctx->VSSetConstantBuffers(0, 1, cbPerFrame_.GetAddressOf());
    ctx->VSSetConstantBuffers(1, 1, cbEffect_.GetAddressOf());
    ctx->PSSetConstantBuffers(1, 1, cbEffect_.GetAddressOf());

    ctx->DrawIndexed(6, 0, 0);
}

void EffectRenderer::Render(ID3D11DeviceContext* ctx,
                             const XMMATRIX& viewProj,
                             const XMFLOAT3& camRight,
                             const XMFLOAT3& camUp,
                             const std::vector<Projectile>& projectiles,
                             const std::vector<AttackEffect>& effects,
                             const std::vector<HitEffect>& hitEffects)
{
    if (effects.empty() && projectiles.empty() && hitEffects.empty()) return;

    // Save current state
    ComPtr<ID3D11BlendState> prevBlend;
    FLOAT prevFactor[4];
    UINT prevMask;
    ctx->OMGetBlendState(&prevBlend, prevFactor, &prevMask);

    ComPtr<ID3D11DepthStencilState> prevDS;
    UINT prevRef;
    ctx->OMGetDepthStencilState(&prevDS, &prevRef);

    // Set effect pipeline
    ctx->VSSetShader(vs_.Get(), nullptr, 0);
    ctx->PSSetShader(ps_.Get(), nullptr, 0);
    ctx->IASetInputLayout(inputLayout_.Get());
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    UINT stride = sizeof(EffectVertex), offset = 0;
    ctx->IASetVertexBuffers(0, 1, quadVB_.GetAddressOf(), &stride, &offset);
    ctx->IASetIndexBuffer(quadIB_.Get(), DXGI_FORMAT_R16_UINT, 0);

    float blendFactor[] = { 0, 0, 0, 0 };
    ctx->OMSetBlendState(blendState_.Get(), blendFactor, 0xFFFFFFFF);
    ctx->OMSetDepthStencilState(dsState_.Get(), 0);

    // Draw attack effects (slash only)
    for (const auto& fx : effects) {
        float normTime = 1.0f - (fx.timer / 0.3f);
        XMFLOAT3 p = fx.position;
        p.y += 0.8f;
        DrawQuad(ctx, viewProj, camRight, camUp,
                 p, 0.4f, normTime, 1, {1, 1, 1, 1});
    }

    // Draw hit effects
    for (const auto& hit : hitEffects) {
        float normTime = 1.0f - (hit.timer / 0.3f);
        XMFLOAT4 color = {1.0f, 0.3f, 0.1f, 1.0f};
        DrawQuad(ctx, viewProj, camRight, camUp,
                 hit.position, 0.5f, normTime, 4, color);
    }

    // Draw projectile glow trails
    for (const auto& proj : projectiles) {
        XMFLOAT3 p = proj.position;
        p.y += 0.3f;
        float normTime = proj.traveled / proj.maxDistance;
        XMFLOAT4 color = {1.0f, 0.9f, 0.4f, 1.0f};  // warm bullet glow
        DrawQuad(ctx, viewProj, camRight, camUp,
                 p, 0.25f, normTime, 3, color);
    }

    // Restore previous state
    ctx->OMSetBlendState(prevBlend.Get(), prevFactor, prevMask);
    ctx->OMSetDepthStencilState(prevDS.Get(), prevRef);
}
