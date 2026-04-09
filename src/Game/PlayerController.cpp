#include "Game/PlayerController.h"
#include "Game/DungeonGenerator.h"
#include "Data/PlayerData.h"
#include "Core/Input.h"
#include "Renderer/Camera.h"
#include <cmath>

using namespace DirectX;

void PlayerController::Init(PlayerData* data)
{
    data_ = data;
    sendTimer_ = 0;
    sendPending_ = false;
    moveState_ = 0;
    velocity_ = {};
    lastSentPos_ = data_->position;
    lastSentRot_ = data_->rotationY;
}

void PlayerController::Update(float dt, const Input& input, const Camera& /*camera*/)
{
    if (!data_) return;

    // Screen-relative movement for isometric camera (yaw=45, pitch=30)
    // "Screen up" maps to world (-1, 0, +1) normalized, "Screen right" to (+1, 0, +1) normalized
    // This makes W=screen up, S=screen down, A=screen left, D=screen right
    constexpr float k = 0.70710678f; // 1/sqrt(2)
    XMFLOAT3 screenUp    = { -k, 0.0f,  k };  // W: screen up
    XMFLOAT3 screenRight = {  k, 0.0f,  k };  // D: screen right

    float inputX = 0.0f, inputZ = 0.0f;
    if (input.IsKeyDown('W') || input.IsKeyDown(VK_UP))    inputZ += 1.0f;
    if (input.IsKeyDown('S') || input.IsKeyDown(VK_DOWN))  inputZ -= 1.0f;
    if (input.IsKeyDown('D') || input.IsKeyDown(VK_RIGHT)) inputX += 1.0f;
    if (input.IsKeyDown('A') || input.IsKeyDown(VK_LEFT))  inputX -= 1.0f;

    XMFLOAT3 moveDir = {
        screenUp.x * inputZ + screenRight.x * inputX,
        0.0f,
        screenUp.z * inputZ + screenRight.z * inputX
    };

    // Normalize if non-zero
    float lenSq = moveDir.x * moveDir.x + moveDir.z * moveDir.z;
    bool moving = lenSq > 0.0001f;
    if (moving) {
        float invLen = 1.0f / sqrtf(lenSq);
        moveDir.x *= invLen;
        moveDir.z *= invLen;
    }

    // Run check
    bool running = input.IsKeyDown(VK_SHIFT);
    float speed = kMoveSpeed * (running ? kRunMultiplier : 1.0f);

    // Update move state
    if (!moving) {
        moveState_ = 0;
        velocity_ = { 0, 0, 0 };
    } else {
        moveState_ = running ? 2 : 1;
        velocity_ = { moveDir.x * speed, 0, moveDir.z * speed };

        // Update position with wall collision
        float newX = data_->position.x + moveDir.x * speed * dt;
        float newZ = data_->position.z + moveDir.z * speed * dt;

        if (dungeon_) {
            // Try full movement first
            if (dungeon_->IsWalkable(newX, newZ)) {
                data_->position.x = newX;
                data_->position.z = newZ;
            }
            // Slide along X axis
            else if (dungeon_->IsWalkable(newX, data_->position.z)) {
                data_->position.x = newX;
            }
            // Slide along Z axis
            else if (dungeon_->IsWalkable(data_->position.x, newZ)) {
                data_->position.z = newZ;
            }
            // Both blocked — don't move
        } else {
            data_->position.x = newX;
            data_->position.z = newZ;
        }
    }

    // Network send at 20Hz
    sendTimer_ += dt;
    if (sendTimer_ >= kSendRate) {
        sendTimer_ -= kSendRate;

        float dx = data_->position.x - lastSentPos_.x;
        float dy = data_->position.y - lastSentPos_.y;
        float dz = data_->position.z - lastSentPos_.z;
        float distSq = dx * dx + dy * dy + dz * dz;
        float rotDiff = fabsf(data_->rotationY - lastSentRot_);

        if (distSq > 0.0001f || rotDiff > 0.5f) {
            sendPending_ = true;
            lastSentPos_ = data_->position;
            lastSentRot_ = data_->rotationY;
        }
    }
}
