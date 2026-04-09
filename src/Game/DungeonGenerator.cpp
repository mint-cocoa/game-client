#include "Game/DungeonGenerator.h"
#include "Core/EventLogger.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>

using namespace DirectX;

static void StoreWorldMatrix(XMFLOAT4X4& out, XMMATRIX m)
{
    XMStoreFloat4x4(&out, m);
}

// Server stores grid as grid_[gx][gy] → gridBytes[gx * GRID_HEIGHT + gy] (column-major)
static bool IsFloor(const std::string& grid, int w, int h, int x, int z)
{
    if (x < 0 || x >= w || z < 0 || z >= h) return false;
    int idx = x * h + z;  // column-major: x * height + z
    if (idx < 0 || idx >= (int)grid.size()) return false;
    return grid[idx] == 0;
}

void DungeonGenerator::WorldToGrid(float wx, float wz, int& gx, int& gz) const
{
    gx = (int)floorf(wx / cellSize_ + gridW_ / 2.0f);
    gz = (int)floorf(wz / cellSize_ + gridH_ / 2.0f);
}

bool DungeonGenerator::IsFloorCell(int gx, int gz) const
{
    if (gx < 0 || gx >= gridW_ || gz < 0 || gz >= gridH_) return false;
    int idx = gx * gridH_ + gz;
    if (idx < 0 || idx >= (int)grid_.size()) return false;
    return grid_[idx] == 0;
}

bool DungeonGenerator::IsWalkable(float worldX, float worldZ) const
{
    int gx, gz;
    WorldToGrid(worldX, worldZ, gx, gz);
    return IsFloorCell(gx, gz);
}

bool DungeonGenerator::LineHitsWall(float ax, float az, float bx, float bz) const
{
    // Step along the line in small increments, check each cell
    float dx = bx - ax, dz = bz - az;
    float dist = sqrtf(dx * dx + dz * dz);
    if (dist < 0.001f) return !IsWalkable(ax, az);

    float step = cellSize_ * 0.4f;  // sub-cell resolution
    int steps = (int)(dist / step) + 1;
    for (int i = 0; i <= steps; ++i) {
        float t = (float)i / (float)steps;
        float wx = ax + dx * t;
        float wz = az + dz * t;
        if (!IsWalkable(wx, wz)) return true;
    }
    return false;
}

void DungeonGenerator::BuildFromMapData(const game::MapData& mapData)
{
    instances_.clear();
    lights_.clear();
    portals_.clear();

    int gw = mapData.grid_width();
    int gh = mapData.grid_height();
    float cs = mapData.cell_size();
    const std::string& grid = mapData.grid();

    // Retain grid data for collision queries
    grid_ = grid;
    gridW_ = gw;
    gridH_ = gh;
    cellSize_ = cs;

    // Spawn position
    if (mapData.has_spawn_position()) {
        spawnPos_ = { mapData.spawn_position().x(),
                      mapData.spawn_position().y(),
                      mapData.spawn_position().z() };
    } else {
        spawnPos_ = { gw * cs * 0.5f, 0.0f, gh * cs * 0.5f };
    }

    // Simple hash for deterministic per-cell randomness
    auto cellHash = [](int x, int z) -> uint32_t {
        uint32_t h = (uint32_t)(x * 73856093) ^ (uint32_t)(z * 19349663);
        h ^= h >> 16; h *= 0x45d9f3b; h ^= h >> 16;
        return h;
    };

    // Grid cells — server uses column-major: gridBytes[x * height + z]
    for (int z = 0; z < gh; ++z) {
        for (int x = 0; x < gw; ++x) {
            int idx = x * gh + z;
            if (idx >= (int)grid.size()) continue;
            uint8_t cell = static_cast<uint8_t>(grid[idx]);

            float wx = (x - gw / 2.0f) * cs;
            float wz = (z - gh / 2.0f) * cs;
            uint32_t h = cellHash(x, z);

            if (cell == 0) {
                // Floor tile with random 90° rotation for variety
                DungeonInstance inst;
                inst.meshName = ((h % 10) == 0) ? "floor-detail" : "floor";
                float floorRot = (float)((h >> 4) % 4) * 90.0f;
                StoreWorldMatrix(inst.world,
                    XMMatrixRotationY(XMConvertToRadians(floorRot)) *
                    XMMatrixTranslation(wx, 0.0f, wz));
                instances_.push_back(std::move(inst));

            } else if (cell == 1) {
                // --- Walls ---
                bool adjN = IsFloor(grid, gw, gh, x, z - 1);
                bool adjS = IsFloor(grid, gw, gh, x, z + 1);
                bool adjE = IsFloor(grid, gw, gh, x + 1, z);
                bool adjW = IsFloor(grid, gw, gh, x - 1, z);

                if (adjN || adjS || adjE || adjW) {
                    // Place wall for each adjacent floor direction
                    float rotY = 0.0f;
                    if (adjN)      rotY = 0.0f;
                    else if (adjE) rotY = 90.0f;
                    else if (adjS) rotY = 180.0f;
                    else if (adjW) rotY = 270.0f;

                    DungeonInstance inst;
                    inst.meshName = "wall";
                    XMMATRIX rot = XMMatrixRotationY(XMConvertToRadians(rotY));
                    XMMATRIX trans = XMMatrixTranslation(wx, 0.0f, wz);
                    StoreWorldMatrix(inst.world, rot * trans);
                    instances_.push_back(std::move(inst));
                }
            }
        }
    }

    // Props
    static const char* kPropNames[] = { "barrel", "chest", "rocks", "banner", "trap" };
    for (int i = 0; i < mapData.props_size(); ++i) {
        const auto& prop = mapData.props(i);
        DungeonInstance inst;
        int pt = prop.prop_type();
        inst.meshName = (pt >= 0 && pt < 5) ? kPropNames[pt] : "barrel";
        XMMATRIX rot = XMMatrixRotationY(XMConvertToRadians(prop.rotation_y()));
        XMMATRIX trans = XMMatrixTranslation(prop.x(), 0.0f, prop.z());
        StoreWorldMatrix(inst.world, rot * trans);
        instances_.push_back(std::move(inst));
    }

    // Lights
    for (int i = 0; i < mapData.lights_size(); ++i) {
        const auto& l = mapData.lights(i);
        PointLight pl;
        pl.position = { l.x(), 1.5f, l.z() };
        pl.color = { l.r(), l.g(), l.b() };
        pl.intensity = l.intensity();
        pl.range = l.range();
        lights_.push_back(pl);
    }

    // Portals
    for (int i = 0; i < mapData.portals_size(); ++i) {
        const auto& p = mapData.portals(i);
        Portal portal;
        portal.position = { p.x(), 0.0f, p.z() };
        portal.portalId = p.portal_id();
        portal.targetName = p.target_name();
        portals_.push_back(portal);
    }

    // Event log: dungeon built — note holds instance count so demo video
    // captions can reference "N instances / 8 draw calls".
    char note[64];
    std::snprintf(note, sizeof(note), "%zu", instances_.size());
    EventLogger::Log("dungeon_built", note);
}
