#pragma once
#include <DirectXMath.h>
#include <vector>
#include <string>
#include "Common.pb.h"

struct DungeonInstance {
    std::string meshName;
    DirectX::XMFLOAT4X4 world;
};

struct PointLight {
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT3 color;
    float intensity;
    float range;
};

class DungeonGenerator {
public:
    void BuildFromMapData(const game::MapData& mapData);
    const std::vector<DungeonInstance>& GetInstances() const { return instances_; }
    const std::vector<PointLight>& GetLights() const { return lights_; }
    DirectX::XMFLOAT3 GetSpawnPosition() const { return spawnPos_; }

    struct Portal {
        DirectX::XMFLOAT3 position;
        uint32_t portalId;
        std::string targetName;
    };
    const std::vector<Portal>& GetPortals() const { return portals_; }

    // Collision: returns true if the world position is walkable (floor cell)
    bool IsWalkable(float worldX, float worldZ) const;
    // Collision: returns true if the line segment from (ax,az) to (bx,bz) hits a wall
    bool LineHitsWall(float ax, float az, float bx, float bz) const;

    // Grid accessors for minimap
    const std::string& GetGrid() const { return grid_; }
    int GetGridW() const { return gridW_; }
    int GetGridH() const { return gridH_; }
    float GetCellSize() const { return cellSize_; }

private:
    // Grid data retained for collision queries
    std::string grid_;
    int gridW_ = 0, gridH_ = 0;
    float cellSize_ = 1.0f;

    // Convert world position to grid coordinates
    void WorldToGrid(float wx, float wz, int& gx, int& gz) const;
    bool IsFloorCell(int gx, int gz) const;

    std::vector<DungeonInstance> instances_;
    std::vector<PointLight> lights_;
    std::vector<Portal> portals_;
    DirectX::XMFLOAT3 spawnPos_ = {};
};
