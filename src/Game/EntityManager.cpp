#include "Game/EntityManager.h"
#include "Core/EventLogger.h"
#include <cmath>
#include <algorithm>

using namespace DirectX;

void EntityManager::SpawnRemote(const PlayerData& data)
{
    RemoteEntity& e = entities_[data.playerId];
    e.data = data;
    e.targetPos = data.position;
    e.targetRotY = data.rotationY;
    e.moveState = 0;
    e.isDead = false;

    EventLogger::LogOnce("remote_spawn", "");
}

void EntityManager::DespawnRemote(uint64_t playerId)
{
    entities_.erase(playerId);
}

void EntityManager::UpdateRemotePosition(uint64_t playerId, XMFLOAT3 pos, float rotY, int moveState)
{
    auto it = entities_.find(playerId);
    if (it == entities_.end()) return;

    it->second.targetPos = pos;
    it->second.targetRotY = rotY;
    it->second.moveState = moveState;
}

void EntityManager::UpdateRemoteHP(uint64_t playerId, int hp, int maxHp, bool isDead)
{
    auto it = entities_.find(playerId);
    if (it == entities_.end()) return;

    it->second.data.hp = hp;
    it->second.data.maxHp = (maxHp > 1) ? maxHp : 100;
    it->second.isDead = isDead;
}

void EntityManager::InterpolateAll(float dt)
{
    float t = std::min(dt * kLerpSpeed, 1.0f);

    for (auto& [id, e] : entities_) {
        // Lerp position
        e.data.position.x += (e.targetPos.x - e.data.position.x) * t;
        e.data.position.y += (e.targetPos.y - e.data.position.y) * t;
        e.data.position.z += (e.targetPos.z - e.data.position.z) * t;

        // Timers
        if (e.hitFlashTimer > 0) e.hitFlashTimer -= dt;
        if (e.hpBarTimer > 0) e.hpBarTimer -= dt;

        // Lerp rotation (shortest path)
        float diff = e.targetRotY - e.data.rotationY;
        // Normalize to [-180, 180]
        while (diff > 180.0f) diff -= 360.0f;
        while (diff < -180.0f) diff += 360.0f;
        e.data.rotationY += diff * t;
    }
}   

void EntityManager::DespawnAll()
{
    entities_.clear();
}

RemoteEntity* EntityManager::GetEntity(uint64_t id)
{
    auto it = entities_.find(id);
    return (it != entities_.end()) ? &it->second : nullptr;
}
