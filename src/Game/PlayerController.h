#pragma once
#include <DirectXMath.h>

struct PlayerData;
class Input;
class Camera;
class DungeonGenerator;

class PlayerController {
public:
    static constexpr float kMoveSpeed = 5.0f;
    static constexpr float kRunMultiplier = 1.6f;
    static constexpr float kSendRate = 0.05f; // 20Hz
    static constexpr float kCollisionRadius = 0.3f;

    void Init(PlayerData* data);
    void SetDungeon(const DungeonGenerator* dungeon) { dungeon_ = dungeon; }
    void Update(float dt, const Input& input, const Camera& camera);
    bool ShouldSendMove() const { return sendPending_; }
    void ClearSendFlag() { sendPending_ = false; }
    int GetMoveState() const { return moveState_; }
    DirectX::XMFLOAT3 GetVelocity() const { return velocity_; }

private:
    PlayerData* data_ = nullptr;
    const DungeonGenerator* dungeon_ = nullptr;
    float sendTimer_ = 0;
    bool sendPending_ = false;
    int moveState_ = 0; // 0=idle, 1=walk, 2=run
    DirectX::XMFLOAT3 velocity_ = {};
    DirectX::XMFLOAT3 lastSentPos_ = {};
    float lastSentRot_ = 0;
};
