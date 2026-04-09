#include "Renderer/MinimapRenderer.h"
#include "Game/DungeonGenerator.h"
#include <vector>

bool MinimapRenderer::Init(ID3D11Device* device, const std::string& shaderPath)
{
    // Compile shaders
    ComPtr<ID3DBlob> vsBlob, psBlob, errBlob;
    std::wstring wpath(shaderPath.begin(), shaderPath.end());

    HRESULT hr = D3DCompileFromFile(wpath.c_str(), nullptr, nullptr,
        "VSMain", "vs_5_0", 0, 0, &vsBlob, &errBlob);
    if (FAILED(hr)) {
        if (errBlob) OutputDebugStringA((char*)errBlob->GetBufferPointer());
        return false;
    }
    hr = D3DCompileFromFile(wpath.c_str(), nullptr, nullptr,
        "PSMain", "ps_5_0", 0, 0, &psBlob, &errBlob);
    if (FAILED(hr)) {
        if (errBlob) OutputDebugStringA((char*)errBlob->GetBufferPointer());
        return false;
    }

    device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vs_);
    device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &ps_);

    // Constant buffer
    D3D11_BUFFER_DESC cbd = {};
    cbd.ByteWidth = (sizeof(MinimapCB) + 15) & ~15;
    cbd.Usage = D3D11_USAGE_DEFAULT;
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    device->CreateBuffer(&cbd, nullptr, &cb_);

    // Point sampler
    D3D11_SAMPLER_DESC sd = {};
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    device->CreateSamplerState(&sd, &sampler_);

    // Alpha blend state
    D3D11_BLEND_DESC bd = {};
    bd.RenderTarget[0].BlendEnable = TRUE;
    bd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    bd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    device->CreateBlendState(&bd, &blendState_);

    // No depth
    D3D11_DEPTH_STENCIL_DESC dsd = {};
    dsd.DepthEnable = FALSE;
    device->CreateDepthStencilState(&dsd, &dsState_);

    return true;
}

void MinimapRenderer::BuildGridTexture(ID3D11Device* device, const DungeonGenerator& dungeon)
{
    gridW_ = dungeon.GetGridW();
    gridH_ = dungeon.GetGridH();
    if (gridW_ <= 0 || gridH_ <= 0) return;

    const std::string& grid = dungeon.GetGrid();

    // Create R8_UNORM texture: 0 = floor, 255 = wall
    std::vector<uint8_t> texData(gridW_ * gridH_);
    for (int x = 0; x < gridW_; ++x) {
        for (int z = 0; z < gridH_; ++z) {
            int srcIdx = x * gridH_ + z;  // column-major (server format)
            int dstIdx = z * gridW_ + x;  // row-major (texture format)
            uint8_t cell = (srcIdx < (int)grid.size()) ? (uint8_t)grid[srcIdx] : 1;
            texData[dstIdx] = (cell == 0) ? 0 : 255;
        }
    }

    D3D11_TEXTURE2D_DESC td = {};
    td.Width = gridW_;
    td.Height = gridH_;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_IMMUTABLE;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA srd = {};
    srd.pSysMem = texData.data();
    srd.SysMemPitch = gridW_;

    gridTex_.Reset();
    gridSRV_.Reset();
    device->CreateTexture2D(&td, &srd, &gridTex_);
    device->CreateShaderResourceView(gridTex_.Get(), nullptr, &gridSRV_);
}

void MinimapRenderer::Render(ID3D11DeviceContext* ctx,
                              float screenW, float screenH,
                              float playerWorldX, float playerWorldZ,
                              const DungeonGenerator& dungeon,
                              const std::vector<MinimapEntity>& entities)
{
    if (!vs_ || !ps_ || !gridSRV_) return;

    float mapSize = 180.0f;
    float margin = 10.0f;

    // Player position in grid UV (0~1)
    float cs = dungeon.GetCellSize();
    int gw = dungeon.GetGridW();
    int gh = dungeon.GetGridH();
    float playerU = (playerWorldX / cs + gw / 2.0f) / (float)gw;
    float playerV = (playerWorldZ / cs + gh / 2.0f) / (float)gh;

    MinimapCB cb = {};
    cb.MapOffset = { screenW - mapSize - margin, margin };
    cb.MapSize = { mapSize, mapSize };
    cb.ScreenSize = { screenW, screenH };
    cb.PlayerGridUV = { playerU, playerV };
    cb.PlayerColor = { 0.2f, 0.8f, 1.0f, 1.0f };
    cb.ViewRadius = 0.15f;

    int count = (int)entities.size();
    if (count > 16) count = 16;
    cb.EntityCount = count;
    for (int i = 0; i < count; ++i) {
        cb.EntityPositions[i] = { entities[i].gridU, entities[i].gridV, 0, 0 };
        cb.EntityColors[i] = { entities[i].r, entities[i].g, entities[i].b, 1.0f };
    }

    ctx->UpdateSubresource(cb_.Get(), 0, nullptr, &cb, 0, 0);

    // Save state
    ComPtr<ID3D11BlendState> oldBlend;
    FLOAT oldBlendFactor[4];
    UINT oldSampleMask;
    ComPtr<ID3D11DepthStencilState> oldDS;
    UINT oldStencilRef;
    ctx->OMGetBlendState(&oldBlend, oldBlendFactor, &oldSampleMask);
    ctx->OMGetDepthStencilState(&oldDS, &oldStencilRef);

    // Set state
    float blendFactor[4] = { 0, 0, 0, 0 };
    ctx->OMSetBlendState(blendState_.Get(), blendFactor, 0xFFFFFFFF);
    ctx->OMSetDepthStencilState(dsState_.Get(), 0);

    ctx->VSSetShader(vs_.Get(), nullptr, 0);
    ctx->PSSetShader(ps_.Get(), nullptr, 0);
    ctx->VSSetConstantBuffers(0, 1, cb_.GetAddressOf());
    ctx->PSSetConstantBuffers(0, 1, cb_.GetAddressOf());
    ctx->PSSetShaderResources(0, 1, gridSRV_.GetAddressOf());
    ctx->PSSetSamplers(0, 1, sampler_.GetAddressOf());

    ctx->IASetInputLayout(nullptr);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    ctx->Draw(4, 0);

    // Restore state
    ctx->OMSetBlendState(oldBlend.Get(), oldBlendFactor, oldSampleMask);
    ctx->OMSetDepthStencilState(oldDS.Get(), oldStencilRef);

    // Unbind SRV
    ID3D11ShaderResourceView* nullSRV = nullptr;
    ctx->PSSetShaderResources(0, 1, &nullSRV);
}
