#include "Core/DX11Device.h"

bool DX11Device::Init(HWND hwnd, int width, int height)
{
    width_ = width;
    height_ = height;

    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = width;
    sd.BufferDesc.Height = height;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    UINT createFlags = 0;
#ifdef _DEBUG
    createFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevel;
    D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0 };

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        createFlags,
        featureLevels, 1,
        D3D11_SDK_VERSION,
        &sd,
        swapChain_.GetAddressOf(),
        device_.GetAddressOf(),
        &featureLevel,
        context_.GetAddressOf());

    if (FAILED(hr))
        return false;

    CreateRenderTarget();
    return true;
}

void DX11Device::CreateRenderTarget()
{
    ComPtr<ID3D11Texture2D> backBuffer;
    swapChain_->GetBuffer(0, IID_PPV_ARGS(backBuffer.GetAddressOf()));
    device_->CreateRenderTargetView(backBuffer.Get(), nullptr, rtv_.ReleaseAndGetAddressOf());

    D3D11_TEXTURE2D_DESC depthDesc = {};
    depthDesc.Width = width_;
    depthDesc.Height = height_;
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.SampleDesc.Quality = 0;
    depthDesc.Usage = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    device_->CreateTexture2D(&depthDesc, nullptr, depthBuffer_.ReleaseAndGetAddressOf());
    device_->CreateDepthStencilView(depthBuffer_.Get(), nullptr, dsv_.ReleaseAndGetAddressOf());
}

void DX11Device::CleanupRenderTarget()
{
    rtv_.Reset();
    dsv_.Reset();
    depthBuffer_.Reset();
}

void DX11Device::BeginFrame(float r, float g, float b)
{
    float clearColor[4] = { r, g, b, 1.0f };
    context_->ClearRenderTargetView(rtv_.Get(), clearColor);
    context_->ClearDepthStencilView(dsv_.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    D3D11_VIEWPORT vp = {};
    vp.Width = static_cast<float>(width_);
    vp.Height = static_cast<float>(height_);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    context_->RSSetViewports(1, &vp);

    ID3D11RenderTargetView* rtvs[] = { rtv_.Get() };
    context_->OMSetRenderTargets(1, rtvs, dsv_.Get());
}

void DX11Device::EndFrame()
{
    swapChain_->Present(1, 0);
}

void DX11Device::Resize(int width, int height)
{
    if (width == 0 || height == 0)
        return;

    width_ = width;
    height_ = height;

    if (!swapChain_)
        return;

    CleanupRenderTarget();
    context_->OMSetRenderTargets(0, nullptr, nullptr);
    swapChain_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    CreateRenderTarget();
}

void DX11Device::Shutdown()
{
    CleanupRenderTarget();
    swapChain_.Reset();
    context_.Reset();
    device_.Reset();
}
