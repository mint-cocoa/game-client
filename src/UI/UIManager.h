#pragma once
#include <d3d11.h>

struct HWND__;
typedef HWND__* HWND;

class UIManager {
public:
    bool Init(HWND hwnd, ID3D11Device* device, ID3D11DeviceContext* context);
    void Shutdown();
    void BeginFrame();
    void EndFrame();
    bool WantCaptureMouse() const;
    bool WantCaptureKeyboard() const;
};
