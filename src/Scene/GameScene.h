#pragma once
#include "Scene/IScene.h"
#include "Game/DungeonGenerator.h"
#include "Game/PlayerController.h"
#include "Game/EntityManager.h"
#include "Game/CombatManager.h"
#include "Renderer/InstanceRenderer.h"
#include "Renderer/EffectRenderer.h"
#include "Renderer/MinimapRenderer.h"
#include <d3d11.h>
#include <wrl/client.h>
#include <cstdint>
#include <string>
#include <vector>

struct EngineContext;
struct SessionState;
struct NetworkContext;
class SceneManager;

struct PartyMember {
    uint64_t id = 0;
    std::string name;
    int hp = 0;
    int maxHp = 100;
    int level = 1;
    bool isLeader = false;
};

class GameScene : public IScene {
public:
    GameScene(EngineContext& engine, SessionState& session,
              NetworkContext& net, SceneManager& scenes);
    void OnEnter() override;
    void OnExit() override;
    void OnUpdate(float dt) override;
    void OnRender() override;
    void OnUI() override;

private:
    // Packet handlers
    void OnMoveResponse(const uint8_t* data, int len);
    void OnSpawnResponse(const uint8_t* data, int len);
    void OnDespawnResponse(const uint8_t* data, int len);
    void OnPlayerListResponse(const uint8_t* data, int len);
    void OnAttackResponse(const uint8_t* data, int len);
    void OnDamageResponse(const uint8_t* data, int len);
    void OnFireResponse(const uint8_t* data, int len);
    void OnRespawnResponse(const uint8_t* data, int len);
    void OnChatResponse(const uint8_t* data, int len);
    void OnPartyUpdateResponse(const uint8_t* data, int len);
    void OnCreatePartyResponse(const uint8_t* data, int len);
    void OnJoinPartyResponse(const uint8_t* data, int len);
    void OnLeavePartyResponse(const uint8_t* data, int len);
    void OnInventoryInitResponse(const uint8_t* data, int len);
    void OnUseItemResponse(const uint8_t* data, int len);
    void OnDropItemResponse(const uint8_t* data, int len);
    void OnMoveItemResponse(const uint8_t* data, int len);
    void OnItemAddResponse(const uint8_t* data, int len);
    void OnItemRemoveResponse(const uint8_t* data, int len);
    void OnCurrencyUpdateResponse(const uint8_t* data, int len);
    void OnPurchaseResponse(const uint8_t* data, int len);
    void OnScoreboardResponse(const uint8_t* data, int len);
    void OnPortalResponse(const uint8_t* data, int len);

    // UI sub-sections
    void DrawHPBar(float screenW, float screenH);
    void DrawCrosshair(float screenW, float screenH);
    void DrawMinimap();
    void DrawFullMap(float screenW, float screenH);
    void DrawChat(float screenW, float screenH);
    void DrawInventory(float screenW, float screenH);
    void DrawPartyPanel(float screenW, float screenH);
    void DrawDeathOverlay(float screenW, float screenH);
    void DrawDamagePopups(float screenW, float screenH);
    void DrawRemoteHPBars(float screenW, float screenH);
    void DrawLeaderboard(float screenW, float screenH);
    void DrawExitMenu(float screenW, float screenH);

    void RenderMeshInstance(const DungeonInstance& inst);
    void RenderEntity(const DirectX::XMFLOAT3& pos, float rotY, const std::string& meshName, float scale = 1.0f, float hitFlash = 0.0f);
    void FillPlayerDataFromInfo(PlayerData& out, const game::PlayerInfo& info);

    // --- Dungeon rendering ---
    void RenderDungeonInstanced();
    void RebuildInstanceBatches();   // call after dungeon_.BuildFromMapData()

    EngineContext&  engine_;
    SessionState&   session_;
    NetworkContext& net_;
    SceneManager&   scenes_;

    DungeonGenerator dungeon_;
    PlayerController playerCtrl_;
    EntityManager entityMgr_;
    CombatManager combatMgr_;
    InstanceRenderer instanceRenderer_;
    EffectRenderer effectRenderer_;
    MinimapRenderer minimapRenderer_;

    // Combat
    float localHitTimer_ = 0.0f;
    float leftClickCooldown_ = 0.0f;
    static constexpr float kLeftClickCooldownSec = 0.4f;
    float aimDirX_ = 0.0f, aimDirZ_ = 1.0f;  // current aim direction

    // UI state
    bool showInventory_ = false;
    bool showExitMenu_ = false;
    bool showLeaderboard_ = false;
    bool showFullMap_ = false;
    bool nearPortal_ = false;

    // Zone tree tracking
    struct ZoneNode {
        uint32_t zoneId = 0;
        std::string name;
        uint32_t parentZoneId = 0;
    };
    std::vector<ZoneNode> zoneTree_;       // all visited zones
    uint32_t currentZoneId_ = 0;
    std::vector<uint32_t> zonePath_;       // current path from root
    char chatInput_[256] = {};
    int currentChatChannel_ = 0;
    int selectedSlot_ = -1;

    // Kill feed
    struct KillFeedEntry { std::string killer; std::string victim; float timer = 5.0f; };
    std::vector<KillFeedEntry> killFeed_;
    void DrawKillFeed(float screenW, float screenH);

    // Kill/Death tracking
    struct KDEntry {
        uint64_t playerId = 0;
        std::string name;
        int kills = 0;
        int deaths = 0;
    };
    std::unordered_map<uint64_t, KDEntry> kdTracker_;
    void RecordKill(uint64_t attackerId, uint64_t targetId);
    KDEntry& GetOrCreateKD(uint64_t id, const std::string& name = "");

    // Ground items (loot)
    struct ClientGroundItem {
        uint64_t groundId = 0;
        int32_t itemDefId = 0;
        DirectX::XMFLOAT3 position = {};
        std::string label;
    };
    std::unordered_map<uint64_t, ClientGroundItem> groundItems_;
    void OnGroundItemSpawn(const uint8_t* data, int len);
    void OnGroundItemDespawn(const uint8_t* data, int len);
    void OnPickupResponse(const uint8_t* data, int len);

    // Party
    std::vector<PartyMember> partyMembers_;

    // Instanced rendering state
    bool instancePrepared_ = false;  // Prepare() was called for current dungeon
};
