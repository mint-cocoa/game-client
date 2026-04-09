#pragma once
#include <DirectXMath.h>
#include <vector>
#include <cstdint>

struct DamagePopup {
    DirectX::XMFLOAT3 position;
    int damage;
    float timer;    // countdown from 1.5s
    float offsetY;  // rises over time
};

struct Projectile {
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT3 direction;
    float speed = 80.0f;       // 서버와 동일
    float maxDistance = 50.0f;  // 서버와 동일
    float traveled = 0.0f;
    uint64_t ownerId = 0;
    bool isLocal = false;      // 자기 투사체 여부
};

struct AttackEffect {
    DirectX::XMFLOAT3 position;
    int skillId = 0;
    float timer = 0.4f;  // 이펙트 지속시간
    float scale = 1.0f;
};

struct HitEffect {
    DirectX::XMFLOAT3 position;
    float timer = 0.3f;
};

class DungeonGenerator;

class CombatManager {
public:
    void SetDungeon(const DungeonGenerator* dungeon) { dungeon_ = dungeon; }
    void HandleAttack(uint64_t attackerId, uint64_t targetId,
                      DirectX::XMFLOAT3 firePos, DirectX::XMFLOAT3 fireDir);
    void HandleFire(uint64_t playerId, DirectX::XMFLOAT3 firePos, DirectX::XMFLOAT3 fireDir, bool isLocal = false);
    void HandleDamage(uint64_t targetId, uint64_t attackerId, int damage,
                      int remainingHp, bool isDead, DirectX::XMFLOAT3 targetPosition);
    void SpawnAttackEffect(DirectX::XMFLOAT3 position);
    void RemoveProjectile(uint64_t ownerId);
    void Update(float dt);
    const std::vector<DamagePopup>& GetPopups() const { return popups_; }
    const std::vector<Projectile>& GetProjectiles() const { return projectiles_; }
    const std::vector<AttackEffect>& GetAttackEffects() const { return attackEffects_; }
    const std::vector<HitEffect>& GetHitEffects() const { return hitEffects_; }

private:
    const DungeonGenerator* dungeon_ = nullptr;
    std::vector<DamagePopup> popups_;
    std::vector<Projectile> projectiles_;
    std::vector<AttackEffect> attackEffects_;
    std::vector<HitEffect> hitEffects_;
};
