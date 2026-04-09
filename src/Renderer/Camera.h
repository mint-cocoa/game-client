#pragma once
#include <DirectXMath.h>

using namespace DirectX;

class Camera {
public:
    void Init(float aspectRatio);
    void SetAspect(float aspectRatio) { aspectRatio_ = aspectRatio; }
    void Update(float dt, XMFLOAT3 targetPos, float scrollDelta);
    void SnapTo(XMFLOAT3 targetPos);  // 즉시 이동 (스무딩 없음)
    XMMATRIX GetViewProj() const;
    XMFLOAT3 GetPosition() const { return position_; }
    XMFLOAT3 GetRight() const;
    XMFLOAT3 GetUp() const;
    XMFLOAT3 ScreenToGround(float mouseX, float mouseY, float screenW, float screenH, float groundY) const;

private:
    static constexpr float kPitch = 30.0f;
    static constexpr float kYaw = 45.0f;
    static constexpr float kDistance = 20.0f;
    static constexpr float kDefaultSize = 15.0f;
    static constexpr float kMinZoom = 5.0f;
    static constexpr float kMaxZoom = 40.0f;
    static constexpr float kZoomSpeed = 2.0f;
    static constexpr float kSmoothTime = 0.15f;

    float orthoSize_ = kDefaultSize;
    float aspectRatio_ = 16.0f / 9.0f;
    XMFLOAT3 position_ = {};
    XMFLOAT3 currentTarget_ = {};
    XMFLOAT3 velocity_ = {};
};
