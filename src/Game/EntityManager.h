#pragma once
#include "Data/PlayerData.h"
#include <DirectXMath.h>
#include <unordered_map>
#include <cstdint>

struct RemoteEntity {
    PlayerData data;
    DirectX::XMFLOAT3 targetPos = {};
    float targetRotY = 0;
    int moveState = 0;
    bool isDead = false;
    float hitFlashTimer = 0.0f;  // shader red flash (~0.25s)
    float hpBarTimer = 0.0f;     // HP bar display (~3s)
};

class EntityManager {
public:
    void SpawnRemote(const PlayerData& data);
    void DespawnRemote(uint64_t playerId);
    void UpdateRemotePosition(uint64_t playerId, DirectX::XMFLOAT3 pos, float rotY, int moveState);
    void UpdateRemoteHP(uint64_t playerId, int hp, int maxHp, bool isDead);
    void InterpolateAll(float dt);
    void DespawnAll();
    const std::unordered_map<uint64_t, RemoteEntity>& GetEntities() const { return entities_; }
    RemoteEntity* GetEntity(uint64_t id);

private:
    std::unordered_map<uint64_t, RemoteEntity> entities_;
    static constexpr float kLerpSpeed = 10.0f;
};
