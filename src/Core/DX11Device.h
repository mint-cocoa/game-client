#pragma once
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

class DX11Device {
public:
    bool Init(HWND hwnd, int width, int height);
    void Shutdown();
    void BeginFrame(float r, float g, float b);
    void EndFrame();
    void Resize(int width, int height);

    ID3D11Device* GetDevice() const { return device_.Get(); }
    ID3D11DeviceContext* GetContext() const { return context_.Get(); }
    IDXGISwapChain* GetSwapChain() const { return swapChain_.Get(); }
    int GetWidth() const { return width_; }
    int GetHeight() const { return height_; }

private:
    void CreateRenderTarget();
    void CleanupRenderTarget();

    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext> context_;
    ComPtr<IDXGISwapChain> swapChain_;
    ComPtr<ID3D11RenderTargetView> rtv_;
    ComPtr<ID3D11DepthStencilView> dsv_;
    ComPtr<ID3D11Texture2D> depthBuffer_;
    int width_ = 0;
    int height_ = 0;
};
