#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"
#include "Renderer/TextureLoader.h"

ComPtr<ID3D11ShaderResourceView> TextureLoader::LoadFromFile(ID3D11Device* device, const std::string& path)
{
    int w, h, channels;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &channels, 4);
    if (!data) return nullptr;

    D3D11_TEXTURE2D_DESC td = {};
    td.Width = (UINT)w;
    td.Height = (UINT)h;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_IMMUTABLE;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA sd = {};
    sd.pSysMem = data;
    sd.SysMemPitch = (UINT)(w * 4);

    ComPtr<ID3D11Texture2D> tex;
    HRESULT hr = device->CreateTexture2D(&td, &sd, &tex);
    stbi_image_free(data);
    if (FAILED(hr)) return nullptr;

    ComPtr<ID3D11ShaderResourceView> srv;
    D3D11_SHADER_RESOURCE_VIEW_DESC srvd = {};
    srvd.Format = td.Format;
    srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvd.Texture2D.MipLevels = 1;
    device->CreateShaderResourceView(tex.Get(), &srvd, &srv);

    return srv;
}

ComPtr<ID3D11ShaderResourceView> TextureLoader::CreateFallback(ID3D11Device* device)
{
    uint32_t magenta = 0xFFFF00FF; // RGBA: magenta

    D3D11_TEXTURE2D_DESC td = {};
    td.Width = 1;
    td.Height = 1;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_IMMUTABLE;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA sd = {};
    sd.pSysMem = &magenta;
    sd.SysMemPitch = 4;

    ComPtr<ID3D11Texture2D> tex;
    device->CreateTexture2D(&td, &sd, &tex);

    ComPtr<ID3D11ShaderResourceView> srv;
    D3D11_SHADER_RESOURCE_VIEW_DESC srvd = {};
    srvd.Format = td.Format;
    srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvd.Texture2D.MipLevels = 1;
    device->CreateShaderResourceView(tex.Get(), &srvd, &srv);

    return srv;
}
