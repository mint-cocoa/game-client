#pragma once
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <string>
#include <vector>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

class DungeonGenerator;

struct MinimapEntity {
    float gridU, gridV;  // position in grid UV space
    float r, g, b;
};

class MinimapRenderer {
public:
    bool Init(ID3D11Device* device, const std::string& shaderPath);
    void BuildGridTexture(ID3D11Device* device, const DungeonGenerator& dungeon);

    // Render minimap at top-right corner
    void Render(ID3D11DeviceContext* ctx,
                float screenW, float screenH,
                float playerWorldX, float playerWorldZ,
                const DungeonGenerator& dungeon,
                const std::vector<MinimapEntity>& entities);

private:
    ComPtr<ID3D11VertexShader>       vs_;
    ComPtr<ID3D11PixelShader>        ps_;
    ComPtr<ID3D11Buffer>             cb_;
    ComPtr<ID3D11Texture2D>          gridTex_;
    ComPtr<ID3D11ShaderResourceView> gridSRV_;
    ComPtr<ID3D11SamplerState>       sampler_;
    ComPtr<ID3D11BlendState>         blendState_;
    ComPtr<ID3D11DepthStencilState>  dsState_;

    int gridW_ = 0, gridH_ = 0;

    struct alignas(16) MinimapCB {
        XMFLOAT2 MapOffset;
        XMFLOAT2 MapSize;
        XMFLOAT2 ScreenSize;
        XMFLOAT2 PlayerGridUV;
        XMFLOAT4 PlayerColor;
        XMFLOAT4 EntityPositions[16];
        XMFLOAT4 EntityColors[16];
        int      EntityCount;
        float    ViewRadius;
        XMFLOAT2 _pad;
    };
};
