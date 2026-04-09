#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <string>

using Microsoft::WRL::ComPtr;

class TextureLoader {
public:
    static ComPtr<ID3D11ShaderResourceView> LoadFromFile(ID3D11Device* device, const std::string& path);
    static ComPtr<ID3D11ShaderResourceView> CreateFallback(ID3D11Device* device);
};
