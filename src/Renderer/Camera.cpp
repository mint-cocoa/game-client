#include "Renderer/Camera.h"
#include <algorithm>
#include <cmath>

void Camera::Init(float aspectRatio)
{
    aspectRatio_ = aspectRatio;
    orthoSize_ = kDefaultSize;
    currentTarget_ = { 0, 0, 0 };
    position_ = { 0, 0, 0 };
}

void Camera::SnapTo(XMFLOAT3 targetPos)
{
    currentTarget_ = targetPos;
    // Immediately compute position
    float pitchRad = XMConvertToRadians(kPitch);
    float yawRad = XMConvertToRadians(kYaw);
    float cosP = cosf(pitchRad), sinP = sinf(pitchRad);
    float cosY = cosf(yawRad), sinY = sinf(yawRad);
    position_.x = currentTarget_.x + kDistance * cosP * sinY;
    position_.y = currentTarget_.y + kDistance * sinP;
    position_.z = currentTarget_.z - kDistance * cosP * cosY;
}

void Camera::Update(float dt, XMFLOAT3 targetPos, float scrollDelta)
{
    // Zoom
    orthoSize_ = std::clamp(orthoSize_ - scrollDelta * kZoomSpeed, kMinZoom, kMaxZoom);

    // Exponential smoothing toward target
    float t = std::min(dt / kSmoothTime, 1.0f);
    currentTarget_.x += (targetPos.x - currentTarget_.x) * t;
    currentTarget_.y += (targetPos.y - currentTarget_.y) * t;
    currentTarget_.z += (targetPos.z - currentTarget_.z) * t;

    // Calculate camera offset from pitch/yaw at kDistance
    float pitchRad = XMConvertToRadians(kPitch);
    float yawRad = XMConvertToRadians(kYaw);

    float cosP = cosf(pitchRad);
    float sinP = sinf(pitchRad);
    float cosY = cosf(yawRad);
    float sinY = sinf(yawRad);

    // Offset: spherical coords (distance, pitch from horizontal, yaw)
    float offX = kDistance * cosP * sinY;
    float offY = kDistance * sinP;
    float offZ = -kDistance * cosP * cosY;

    position_.x = currentTarget_.x + offX;
    position_.y = currentTarget_.y + offY;
    position_.z = currentTarget_.z + offZ;
}

XMFLOAT3 Camera::GetRight() const
{
    XMVECTOR eye = XMLoadFloat3(&position_);
    XMVECTOR target = XMLoadFloat3(&currentTarget_);
    XMVECTOR up = XMVectorSet(0, 1, 0, 0);
    XMVECTOR forward = XMVector3Normalize(XMVectorSubtract(target, eye));
    XMVECTOR right = XMVector3Normalize(XMVector3Cross(up, forward));
    XMFLOAT3 r;
    XMStoreFloat3(&r, right);
    return r;
}

XMFLOAT3 Camera::GetUp() const
{
    XMVECTOR eye = XMLoadFloat3(&position_);
    XMVECTOR target = XMLoadFloat3(&currentTarget_);
    XMVECTOR worldUp = XMVectorSet(0, 1, 0, 0);
    XMVECTOR forward = XMVector3Normalize(XMVectorSubtract(target, eye));
    XMVECTOR right = XMVector3Normalize(XMVector3Cross(worldUp, forward));
    XMVECTOR camUp = XMVector3Cross(forward, right);
    XMFLOAT3 u;
    XMStoreFloat3(&u, camUp);
    return u;
}

XMFLOAT3 Camera::ScreenToGround(float mouseX, float mouseY, float screenW, float screenH, float groundY) const
{
    // NDC
    float ndcX = 2.0f * mouseX / screenW - 1.0f;
    float ndcY = 1.0f - 2.0f * mouseY / screenH;

    XMMATRIX vp = GetViewProj();
    XMMATRIX invVP = XMMatrixInverse(nullptr, vp);

    // Unproject near/far points
    XMVECTOR nearPt = XMVector3TransformCoord(XMVectorSet(ndcX, ndcY, 0.0f, 1.0f), invVP);
    XMVECTOR farPt  = XMVector3TransformCoord(XMVectorSet(ndcX, ndcY, 1.0f, 1.0f), invVP);

    // Ray direction
    XMVECTOR rayDir = XMVectorSubtract(farPt, nearPt);

    // Intersect with y = groundY plane
    float originY = XMVectorGetY(nearPt);
    float dirY = XMVectorGetY(rayDir);
    float t = (dirY != 0.0f) ? (groundY - originY) / dirY : 0.0f;

    XMVECTOR hit = XMVectorAdd(nearPt, XMVectorScale(rayDir, t));
    XMFLOAT3 result;
    XMStoreFloat3(&result, hit);
    return result;
}

XMMATRIX Camera::GetViewProj() const
{
    XMVECTOR eye = XMLoadFloat3(&position_);
    XMVECTOR target = XMLoadFloat3(&currentTarget_);
    XMVECTOR up = XMVectorSet(0, 1, 0, 0);

    XMMATRIX view = XMMatrixLookAtLH(eye, target, up);
    XMMATRIX proj = XMMatrixOrthographicLH(
        orthoSize_ * aspectRatio_ * 2.0f, orthoSize_ * 2.0f, -200.0f, 500.0f);

    return view * proj;
}
