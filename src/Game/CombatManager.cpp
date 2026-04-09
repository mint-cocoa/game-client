#include "Game/CombatManager.h"
#include "Game/DungeonGenerator.h"
#include "Core/EventLogger.h"
#include <algorithm>

using namespace DirectX;

void CombatManager::HandleAttack(uint64_t /*attackerId*/, uint64_t /*targetId*/,
                                  XMFLOAT3 firePos, XMFLOAT3 /*fireDir*/)
{
    SpawnAttackEffect(firePos);
}

void CombatManager::HandleFire(uint64_t playerId, XMFLOAT3 firePos, XMFLOAT3 fireDir, bool isLocal)
{
    Projectile proj;
    proj.position = firePos;
    proj.direction = fireDir;
    proj.speed = 80.0f;       // 서버와 동일
    proj.maxDistance = 50.0f;
    proj.traveled = 0.0f;
    proj.ownerId = playerId;
    proj.isLocal = isLocal;
    projectiles_.push_back(proj);

    if (isLocal) EventLogger::LogOnce("first_fire", "");
}

void CombatManager::SpawnAttackEffect(XMFLOAT3 position)
{
    AttackEffect fx;
    fx.position = position;
    fx.timer = 0.3f;
    fx.scale = 1.0f;
    attackEffects_.push_back(fx);
}

void CombatManager::HandleDamage(uint64_t /*targetId*/, uint64_t /*attackerId*/, int damage,
                                  int /*remainingHp*/, bool isDead, XMFLOAT3 targetPosition)
{
    DamagePopup popup;
    popup.position = targetPosition;
    popup.damage = damage;
    popup.timer = 1.5f;
    popup.offsetY = 0.0f;
    popups_.push_back(popup);

    // Hit effect at target position
    HitEffect hit;
    hit.position = { targetPosition.x, targetPosition.y + 0.9f, targetPosition.z };
    hit.timer = 0.3f;
    hitEffects_.push_back(hit);

    EventLogger::LogOnce("first_hit", "");
    if (isDead) EventLogger::LogOnce("first_kill", "");
}

void CombatManager::RemoveProjectile(uint64_t ownerId)
{
    for (auto& p : projectiles_) {
        if (p.ownerId == ownerId) {
            p.traveled = p.maxDistance;  // mark for removal
            break;  // one projectile per hit
        }
    }
}

void CombatManager::Update(float dt)
{
    // Popups
    for (auto& p : popups_) {
        p.timer -= dt;
        p.offsetY += dt * 2.0f;
    }
    popups_.erase(
        std::remove_if(popups_.begin(), popups_.end(),
            [](const DamagePopup& p) { return p.timer <= 0.0f; }),
        popups_.end());

    // Projectiles — 거리 기반 + 벽 충돌 소멸
    for (auto& proj : projectiles_) {
        float move = proj.speed * dt;
        proj.position.x += proj.direction.x * move;
        proj.position.y += proj.direction.y * move;
        proj.position.z += proj.direction.z * move;
        proj.traveled += move;

        // Wall collision check
        if (dungeon_ && !dungeon_->IsWalkable(proj.position.x, proj.position.z)) {
            proj.traveled = proj.maxDistance;  // mark for removal
        }
    }
    projectiles_.erase(
        std::remove_if(projectiles_.begin(), projectiles_.end(),
            [](const Projectile& p) { return p.traveled >= p.maxDistance; }),
        projectiles_.end());

    // Attack effects
    for (auto& fx : attackEffects_) {
        fx.timer -= dt;
        fx.scale = 1.0f + (0.3f - fx.timer) * 2.0f;  // 확대 후 축소
        if (fx.scale > 1.5f) fx.scale = 1.5f;
    }
    attackEffects_.erase(
        std::remove_if(attackEffects_.begin(), attackEffects_.end(),
            [](const AttackEffect& e) { return e.timer <= 0.0f; }),
        attackEffects_.end());

    // Hit effects
    for (auto& h : hitEffects_)
        h.timer -= dt;
    hitEffects_.erase(
        std::remove_if(hitEffects_.begin(), hitEffects_.end(),
            [](const HitEffect& h) { return h.timer <= 0.0f; }),
        hitEffects_.end());
}
