#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <string>

using Microsoft::WRL::ComPtr;

struct Material {
    std::string name;
    ComPtr<ID3D11ShaderResourceView> texture;
    // Could add metallic, smoothness later
};
