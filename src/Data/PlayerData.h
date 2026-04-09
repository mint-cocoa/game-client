#pragma once
#include <DirectXMath.h>
#include <string>
#include <cstdint>

struct PlayerData {
    uint64_t playerId = 0;
    std::string name;
    DirectX::XMFLOAT3 position = { 0, 0, 0 };
    float rotationY = 0;
    int hp = 100;
    int maxHp = 100;
    int level = 1;
    uint32_t zoneId = 0;
    uint64_t partyId = 0;
};
