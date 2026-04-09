#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>

using Microsoft::WRL::ComPtr;

struct Vertex {
    DirectX::XMFLOAT3 Position;
    DirectX::XMFLOAT3 Normal;
    DirectX::XMFLOAT2 UV;
};

struct Mesh {
    ComPtr<ID3D11Buffer> vertexBuffer;
    ComPtr<ID3D11Buffer> indexBuffer;
    UINT indexCount = 0;
    UINT stride = sizeof(Vertex);
};
