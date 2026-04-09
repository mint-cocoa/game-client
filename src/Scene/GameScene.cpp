#include "Scene/GameScene.h"
#include "Core/EngineContext.h"
#include "Core/EventLogger.h"
#include "Game/SessionState.h"
#include "Network/NetworkContext.h"
#include "Network/PacketBuilder.h"
#include "Network/PacketHandler.h"
#include "Scene/SceneManager.h"
#include "Core/DX11Device.h"
#include "Core/Input.h"
#include "Renderer/Pipeline.h"
#include "Renderer/Camera.h"
#include "Renderer/MeshCache.h"
#include "Renderer/MaterialManager.h"
#include "Renderer/Mesh.h"
#include "imgui/imgui.h"
#include "Game.pb.h"
#include "Social.pb.h"
#include "Inventory.pb.h"
#include "Currency.pb.h"
#include <cmath>
#include <cstdio>
#include <algorithm>

using namespace DirectX;

GameScene::GameScene(EngineContext& engine, SessionState& session,
                     NetworkContext& net, SceneManager& scenes)
    : engine_(engine), session_(session), net_(net), scenes_(scenes) {}

// ---------------------------------------------------------------------------
// Helper: fill PlayerData from protobuf PlayerInfo
// ---------------------------------------------------------------------------
void GameScene::FillPlayerDataFromInfo(PlayerData& out, const game::PlayerInfo& info)
{
    out.playerId = info.player_id();
    out.name = info.name();
    out.position = { info.position().x(), info.position().y(), info.position().z() };
    out.rotationY = info.rotation_y();
    out.hp = info.hp();
    out.maxHp = (info.max_hp() > 1) ? info.max_hp() : 100;
    out.level = info.level();
    out.zoneId = info.zone_id();
    out.partyId = info.party_id();
}

// ---------------------------------------------------------------------------
// Scene lifecycle
// ---------------------------------------------------------------------------
void GameScene::OnEnter()
{
    EventLogger::Log("scene_enter", "game");

    // Build dungeon
    if (session_.hasMapData) {
        dungeon_.BuildFromMapData(session_.mapData);
        session_.localPlayer.position = dungeon_.GetSpawnPosition();
    } else {
        OutputDebugStringA("[GameScene] WARNING: no MapData!\n");
    }

    // Init player controller with dungeon collision
    playerCtrl_.Init(&session_.localPlayer);
    playerCtrl_.SetDungeon(&dungeon_);
    combatMgr_.SetDungeon(&dungeon_);

    // Snap camera to spawn position (no smoothing)
    engine_.camera->SnapTo(session_.localPlayer.position);

    // Init zone tree with root
    zoneTree_.clear();
    zonePath_.clear();
    currentZoneId_ = 1;
    zoneTree_.push_back({1, "Root", 0});
    zonePath_.push_back(1);

    // Reset UI state
    showInventory_ = false;
    showExitMenu_ = false;
    memset(chatInput_, 0, sizeof(chatInput_));
    currentChatChannel_ = 0;
    selectedSlot_ = -1;
    partyMembers_.clear();
    session_.isLocalPlayerDead = false;

    // Init effect renderer
    if (!effectRenderer_.Init(engine_.device->GetDevice(), *engine_.assetsDir + "/shaders/effect.hlsl")) {
        OutputDebugStringA("[GameScene] ERROR: EffectRenderer Init FAILED (shader compile?)\n");
    }

    // Init minimap renderer
    if (minimapRenderer_.Init(engine_.device->GetDevice(), *engine_.assetsDir + "/shaders/minimap.hlsl")) {
        minimapRenderer_.BuildGridTexture(engine_.device->GetDevice(), dungeon_);
    }

    // Instanced renderer setup
    instanceRenderer_.Init(engine_.device->GetDevice());
    RebuildInstanceBatches();

    // Register ALL game packet handlers
    auto& ph = *net_.handler;

    ph.Register(MsgId::S_MOVE, [this](const uint8_t* d, int l) { OnMoveResponse(d, l); });
    ph.Register(MsgId::S_SPAWN, [this](const uint8_t* d, int l) { OnSpawnResponse(d, l); });
    ph.Register(MsgId::S_DESPAWN, [this](const uint8_t* d, int l) { OnDespawnResponse(d, l); });
    ph.Register(MsgId::S_PLAYER_LIST, [this](const uint8_t* d, int l) { OnPlayerListResponse(d, l); });
    ph.Register(MsgId::S_ATTACK, [this](const uint8_t* d, int l) { OnAttackResponse(d, l); });
    ph.Register(MsgId::S_DAMAGE, [this](const uint8_t* d, int l) { OnDamageResponse(d, l); });
    ph.Register(MsgId::S_FIRE, [this](const uint8_t* d, int l) { OnFireResponse(d, l); });
    ph.Register(MsgId::S_RESPAWN, [this](const uint8_t* d, int l) { OnRespawnResponse(d, l); });
    ph.Register(MsgId::S_SCOREBOARD, [this](const uint8_t* d, int l) { OnScoreboardResponse(d, l); });
    ph.Register(MsgId::S_PORTAL, [this](const uint8_t* d, int l) { OnPortalResponse(d, l); });
    ph.Register(MsgId::S_GROUND_ITEM_SPAWN, [this](const uint8_t* d, int l) { OnGroundItemSpawn(d, l); });
    ph.Register(MsgId::S_GROUND_ITEM_DESPAWN, [this](const uint8_t* d, int l) { OnGroundItemDespawn(d, l); });
    ph.Register(MsgId::S_PICKUP, [this](const uint8_t* d, int l) { OnPickupResponse(d, l); });
    ph.Register(MsgId::S_CHAT, [this](const uint8_t* d, int l) { OnChatResponse(d, l); });
    ph.Register(MsgId::S_CREATE_PARTY, [this](const uint8_t* d, int l) { OnCreatePartyResponse(d, l); });
    ph.Register(MsgId::S_JOIN_PARTY, [this](const uint8_t* d, int l) { OnJoinPartyResponse(d, l); });
    ph.Register(MsgId::S_LEAVE_PARTY, [this](const uint8_t* d, int l) { OnLeavePartyResponse(d, l); });
    ph.Register(MsgId::S_PARTY_UPDATE, [this](const uint8_t* d, int l) { OnPartyUpdateResponse(d, l); });
    ph.Register(MsgId::S_INVENTORY_INIT, [this](const uint8_t* d, int l) { OnInventoryInitResponse(d, l); });
    ph.Register(MsgId::S_USE_ITEM, [this](const uint8_t* d, int l) { OnUseItemResponse(d, l); });
    ph.Register(MsgId::S_DROP_ITEM, [this](const uint8_t* d, int l) { OnDropItemResponse(d, l); });
    ph.Register(MsgId::S_MOVE_ITEM, [this](const uint8_t* d, int l) { OnMoveItemResponse(d, l); });
    ph.Register(MsgId::S_ITEM_ADD, [this](const uint8_t* d, int l) { OnItemAddResponse(d, l); });
    ph.Register(MsgId::S_ITEM_REMOVE, [this](const uint8_t* d, int l) { OnItemRemoveResponse(d, l); });
    ph.Register(MsgId::S_CURRENCY_UPDATE, [this](const uint8_t* d, int l) { OnCurrencyUpdateResponse(d, l); });
    ph.Register(MsgId::S_PURCHASE, [this](const uint8_t* d, int l) { OnPurchaseResponse(d, l); });

    // Notify server that all handlers are registered and we are ready
    // to receive game state (Server-Initiated Flow Control).
    net_.builder->SendEmpty(MsgId::C_SCENE_READY);
}

void GameScene::OnExit()
{
    entityMgr_.DespawnAll();
}

// ---------------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------------
void GameScene::OnUpdate(float dt)
{
    if (!session_.isLocalPlayerDead) {
        playerCtrl_.Update(dt, *engine_.input, *engine_.camera);

        if (playerCtrl_.ShouldSendMove()) {
            auto& pos = session_.localPlayer.position;
            auto vel = playerCtrl_.GetVelocity();
            net_.builder->SendMove(
                pos.x, pos.y, pos.z, session_.localPlayer.rotationY,
                vel.x, vel.y, vel.z, playerCtrl_.GetMoveState());
            playerCtrl_.ClearSendFlag();
        }
    }

    // Hit / cooldown timers
    if (localHitTimer_ > 0) localHitTimer_ -= dt;
    if (leftClickCooldown_ > 0) leftClickCooldown_ -= dt;

    // Kill feed decay
    for (auto& kf : killFeed_) kf.timer -= dt;
    killFeed_.erase(std::remove_if(killFeed_.begin(), killFeed_.end(),
        [](const KillFeedEntry& e) { return e.timer <= 0; }), killFeed_.end());


    // Entity interpolation
    entityMgr_.InterpolateAll(dt);

    // Camera follow
    engine_.camera->Update(dt, session_.localPlayer.position, engine_.input->GetMouse().scrollDelta);

    // Combat effects
    combatMgr_.Update(dt);

    // Compute mouse aim direction (shared by left-click and skill keys)
    float aimDirX = 0.0f, aimDirZ = 1.0f;
    {
        auto& mouse = engine_.input->GetMouse();
        float sw = (float)engine_.device->GetWidth();
        float sh = (float)engine_.device->GetHeight();
        XMFLOAT3 groundPt = engine_.camera->ScreenToGround(
            (float)mouse.x, (float)mouse.y, sw, sh, session_.localPlayer.position.y);
        float dx = groundPt.x - session_.localPlayer.position.x;
        float dz = groundPt.z - session_.localPlayer.position.z;
        float len = sqrtf(dx * dx + dz * dz);
        if (len > 0.001f) {
            aimDirX = dx / len;
            aimDirZ = dz / len;
        }
    }
    aimDirX_ = aimDirX;
    aimDirZ_ = aimDirZ;

    // Always face mouse cursor
    if (!session_.isLocalPlayerDead) {
        session_.localPlayer.rotationY = atan2f(aimDirX, aimDirZ) * (180.0f / 3.14159265f);
    }

    // Left click: projectile attack toward mouse (C_FIRE only, no C_ATTACK)
    if (!session_.isLocalPlayerDead && engine_.input->IsMousePressed(0) && leftClickCooldown_ <= 0) {
        leftClickCooldown_ = kLeftClickCooldownSec;

        auto& pos = session_.localPlayer.position;
        XMFLOAT3 firePos = { pos.x + aimDirX * 0.6f, pos.y + 0.9f, pos.z + aimDirZ * 0.6f };
        XMFLOAT3 fireDir = { aimDirX, 0.0f, aimDirZ };
        combatMgr_.HandleFire(session_.playerId, firePos, fireDir, true);
        net_.builder->SendFire(firePos.x, firePos.y, firePos.z,
                                         fireDir.x, fireDir.y, fireDir.z);
        OutputDebugStringA("[Combat] Left-click projectile fired\n");
    }

    // ESC for exit menu
    if (engine_.input->IsKeyPressed(VK_ESCAPE)) {
        showExitMenu_ = !showExitMenu_;
    }

    // Portal proximity check + F key
    if (!session_.isLocalPlayerDead) {
        for (const auto& portal : dungeon_.GetPortals()) {
            float dx = session_.localPlayer.position.x - portal.position.x;
            float dz = session_.localPlayer.position.z - portal.position.z;
            if (dx * dx + dz * dz < 4.0f) {  // 2 unit radius
                nearPortal_ = true;
                if (engine_.input->IsKeyPressed('F')) {
                    net_.builder->SendPortal(portal.portalId);
                }
                break;
            }
        }
        if (nearPortal_) {
            // Check if still near any portal
            bool still_near = false;
            for (const auto& portal : dungeon_.GetPortals()) {
                float dx = session_.localPlayer.position.x - portal.position.x;
                float dz = session_.localPlayer.position.z - portal.position.z;
                if (dx * dx + dz * dz < 4.0f) { still_near = true; break; }
            }
            nearPortal_ = still_near;
        }
    }

    // E key = pick up nearest ground item
    if (engine_.input->IsKeyPressed('E') && !session_.isLocalPlayerDead) {
        float bestDist = 9.0f;  // 3m radius squared
        uint64_t bestId = 0;
        for (const auto& [gid, gi] : groundItems_) {
            float dx = session_.localPlayer.position.x - gi.position.x;
            float dz = session_.localPlayer.position.z - gi.position.z;
            float d2 = dx * dx + dz * dz;
            if (d2 < bestDist) { bestDist = d2; bestId = gid; }
        }
        if (bestId != 0) {
            net_.builder->SendPickup(bestId);
        }
    }

    // Tab = hold to show leaderboard
    showLeaderboard_ = engine_.input->IsKeyDown(VK_TAB);

    // I for inventory
    if (engine_.input->IsKeyPressed('I')) {
        showInventory_ = !showInventory_;
    }
    // M for full map
    if (engine_.input->IsKeyPressed('M')) {
        showFullMap_ = !showFullMap_;
    }
}

// ---------------------------------------------------------------------------
// Render (3D)
// ---------------------------------------------------------------------------
void GameScene::RenderMeshInstance(const DungeonInstance& inst)
{
    const Mesh* mesh = engine_.meshCache->Get(inst.meshName);
    if (!mesh) return;

    auto* ctx = engine_.device->GetContext();

    PerObjectData pod;
    pod.World = inst.world;
    engine_.pipeline->UpdatePerObject(ctx, pod);

    UINT offset = 0;
    ID3D11Buffer* vb = mesh->vertexBuffer.Get();
    ctx->IASetVertexBuffers(0, 1, &vb, &mesh->stride, &offset);
    ctx->IASetIndexBuffer(mesh->indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
    ctx->DrawIndexed(mesh->indexCount, 0, 0);
}

void GameScene::RenderEntity(const XMFLOAT3& pos, float rotY, const std::string& meshName, float scale, float hitFlash)
{
    const Mesh* mesh = engine_.meshCache->Get(meshName);
    if (!mesh) return;

    auto* ctx = engine_.device->GetContext();

    // Hit shake offset
    XMFLOAT3 renderPos = pos;
    if (hitFlash > 0.0f) {
        float shake = sinf(hitFlash * 60.0f) * 0.08f * hitFlash;
        renderPos.x += shake;
    }

    XMMATRIX sc = XMMatrixScaling(scale, scale, scale);
    XMMATRIX rot = XMMatrixRotationY(XMConvertToRadians(rotY));
    XMMATRIX trans = XMMatrixTranslation(renderPos.x, renderPos.y, renderPos.z);
    XMMATRIX world = sc * rot * trans;

    PerObjectData pod;
    XMStoreFloat4x4(&pod.World, world);
    pod.HitFlash = hitFlash > 0.0f ? hitFlash / 0.25f : 0.0f;  // normalize to 0~1
    engine_.pipeline->UpdatePerObject(ctx, pod);

    UINT offset = 0;
    ID3D11Buffer* vb = mesh->vertexBuffer.Get();
    ctx->IASetVertexBuffers(0, 1, &vb, &mesh->stride, &offset);
    ctx->IASetIndexBuffer(mesh->indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
    ctx->DrawIndexed(mesh->indexCount, 0, 0);
}

void GameScene::OnRender()
{
    if (!engine_.pipelineOk) return;

    auto* ctx = engine_.device->GetContext();
    auto& pipeline = *engine_.pipeline;
    auto& matMgr = *engine_.materialMgr;

    // Bind pipeline
    pipeline.Bind(ctx);

    // Per-frame data
    PerFrameData pfd;
    XMStoreFloat4x4(&pfd.ViewProj, engine_.camera->GetViewProj());
    pfd.LightDir = { 0.3f, -0.8f, 0.4f };
    pfd.AmbientColor = { 0.4f, 0.35f, 0.45f };
    pipeline.UpdatePerFrame(ctx, pfd);

    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Render dungeon — all use M_Dungeon
    const Material* dungeonMat = matMgr.Get("M_Dungeon");
    matMgr.Bind(ctx, dungeonMat ? dungeonMat : matMgr.GetDefault());

    if (instancePrepared_) {
        RenderDungeonInstanced();
    }

    // Restore non-instanced VS for the rest of the scene (entities, etc.)
    pipeline.Bind(ctx);

    // Render local player — M_LocalPlayer
    if (!session_.isLocalPlayerDead) {
        const Material* playerMat = matMgr.Get("M_LocalPlayer");
        matMgr.Bind(ctx, playerMat ? playerMat : matMgr.GetDefault());
        RenderEntity(session_.localPlayer.position, session_.localPlayer.rotationY, "character-human", 0.667f, localHitTimer_);
    }

    // Render remote entities — M_RemotePlayer
    const Material* remoteMat = matMgr.Get("M_RemotePlayer");
    matMgr.Bind(ctx, remoteMat ? remoteMat : matMgr.GetDefault());
    for (const auto& [id, entity] : entityMgr_.GetEntities()) {
        if (!entity.isDead)
            RenderEntity(entity.data.position, entity.data.rotationY, "character-orc", 0.667f, entity.hitFlashTimer);
    }

    // Render projectiles — bullet sphere (matches Unity IsoBullet)
    const Material* bulletMat = matMgr.Get("M_Bullet");
    matMgr.Bind(ctx, bulletMat ? bulletMat : matMgr.GetDefault());
    for (const auto& proj : combatMgr_.GetProjectiles()) {
        XMFLOAT3 p = proj.position;
        p.y += 0.3f;
        RenderEntity(p, 0.0f, "bullet", 0.18f);
    }

    // Render portal markers (reuse column mesh with glow)
    for (const auto& portal : dungeon_.GetPortals()) {
        RenderEntity(portal.position, 0.0f, "column", 1.0f, 0.0f);
    }

    // Render ground items (dog tags etc.)
    for (const auto& [gid, gi] : groundItems_) {
        RenderEntity(gi.position, 0.0f, "coin", 0.8f, 0.0f);
    }

    // Render attack effects (shader billboard)
    effectRenderer_.Render(
        ctx,
        engine_.camera->GetViewProj(),
        engine_.camera->GetRight(),
        engine_.camera->GetUp(),
        combatMgr_.GetProjectiles(),
        combatMgr_.GetAttackEffects(),
        combatMgr_.GetHitEffects()
    );

    // Minimap (rendered after 3D, before ImGui)
    DrawMinimap();

    // Restore pipeline state for ImGui
    engine_.pipeline->Bind(ctx);
}

// ---------------------------------------------------------------------------
// UI (ImGui) - main dispatch
// ---------------------------------------------------------------------------
void GameScene::OnUI()
{
    float screenW = (float)engine_.device->GetWidth();
    float screenH = (float)engine_.device->GetHeight();

    DrawHPBar(screenW, screenH);
    DrawCrosshair(screenW, screenH);

    // Portal prompt
    if (nearPortal_) {
        const char* prompt = "Press [F] to enter portal";
        ImVec2 ts = ImGui::CalcTextSize(prompt);
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        float px = (screenW - ts.x) * 0.5f;
        float py = screenH * 0.7f;
        dl->AddRectFilled(ImVec2(px - 10, py - 5), ImVec2(px + ts.x + 10, py + ts.y + 5),
                          IM_COL32(0, 0, 0, 180), 5.0f);
        dl->AddText(ImVec2(px, py), IM_COL32(255, 220, 50, 255), prompt);
    }

    // Ground item pickup prompt + floating labels (nearby only, max 5)
    if (!session_.isLocalPlayerDead) {
        XMMATRIX vp = engine_.camera->GetViewProj();
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        float nearestDist = 9.0f;
        std::string nearestLabel;
        int labelCount = 0;
        for (const auto& [gid, gi] : groundItems_) {
            float dx = session_.localPlayer.position.x - gi.position.x;
            float dz = session_.localPlayer.position.z - gi.position.z;
            float d2 = dx * dx + dz * dz;

            // Pickup range check
            if (d2 < nearestDist) { nearestDist = d2; nearestLabel = gi.label; }

            // Only show labels within 10m, max 5
            if (d2 > 100.0f || labelCount >= 5) continue;

            XMVECTOR wpos = XMLoadFloat3(&gi.position);
            wpos = XMVectorSetY(wpos, XMVectorGetY(wpos) + 1.5f);
            XMVECTOR clip = XMVector3TransformCoord(wpos, vp);
            if (XMVectorGetZ(clip) > 0.0f && XMVectorGetZ(clip) < 1.0f) {
                float sx = (XMVectorGetX(clip) * 0.5f + 0.5f) * screenW;
                float sy = (1.0f - (XMVectorGetY(clip) * 0.5f + 0.5f)) * screenH;
                ImVec2 ts = ImGui::CalcTextSize(gi.label.c_str());
                dl->AddRectFilled(ImVec2(sx - ts.x * 0.5f - 4, sy - 2),
                                  ImVec2(sx + ts.x * 0.5f + 4, sy + ts.y + 2),
                                  IM_COL32(0, 0, 0, 160), 3.0f);
                dl->AddText(ImVec2(sx - ts.x * 0.5f, sy), IM_COL32(255, 200, 50, 255),
                            gi.label.c_str());
                labelCount++;
            }
        }
        if (nearestDist < 9.0f && !nearestLabel.empty()) {
            char buf[256];
            snprintf(buf, sizeof(buf), "Press [E] to pick up %s", nearestLabel.c_str());
            ImVec2 ts = ImGui::CalcTextSize(buf);
            float px = (screenW - ts.x) * 0.5f;
            float py = screenH * 0.75f;
            dl->AddRectFilled(ImVec2(px - 10, py - 5), ImVec2(px + ts.x + 10, py + ts.y + 5),
                              IM_COL32(0, 0, 0, 180), 5.0f);
            dl->AddText(ImVec2(px, py), IM_COL32(100, 255, 100, 255), buf);
        }
    }
    DrawFullMap(screenW, screenH);
    DrawChat(screenW, screenH);
    DrawInventory(screenW, screenH);
    DrawPartyPanel(screenW, screenH);
    DrawDamagePopups(screenW, screenH);
    DrawRemoteHPBars(screenW, screenH);
    DrawLeaderboard(screenW, screenH);
    DrawKillFeed(screenW, screenH);
    DrawDeathOverlay(screenW, screenH);
    DrawExitMenu(screenW, screenH);
}

// ---------------------------------------------------------------------------
// UI: HP Bar (top-left)
// ---------------------------------------------------------------------------
void GameScene::DrawHPBar(float /*screenW*/, float /*screenH*/)
{
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(250, 0), ImGuiCond_Always);
    ImGui::Begin("##HP", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoFocusOnAppearing);

    float hpRatio = (session_.localPlayer.maxHp > 0) ?
        (float)session_.localPlayer.hp / (float)session_.localPlayer.maxHp : 0.0f;
    ImGui::Text("%s  Lv.%d", session_.localPlayer.name.c_str(), session_.localPlayer.level);
    ImGui::ProgressBar(hpRatio, ImVec2(-1, 18));
    ImGui::Text("HP: %d / %d", session_.localPlayer.hp, session_.localPlayer.maxHp);
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "Gold: %lld", session_.currency.gold);
    ImGui::End();
}

// ---------------------------------------------------------------------------
// UI: Crosshair at mouse cursor
// ---------------------------------------------------------------------------
void GameScene::DrawCrosshair(float screenW, float screenH)
{
    if (session_.isLocalPlayerDead) return;

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    XMMATRIX vp = engine_.camera->GetViewProj();

    // --- Aim line: player → aim direction (LoL style) ---
    constexpr float kAimLen = 8.0f;       // world units
    constexpr int   kSegments = 16;
    auto& pos = session_.localPlayer.position;

    // Project world points along aim direction to screen
    ImVec2 prevScreen = {};
    bool prevValid = false;

    for (int i = 0; i <= kSegments; ++i) {
        float t = (float)i / (float)kSegments * kAimLen;
        float wx = pos.x + aimDirX_ * t;
        float wz = pos.z + aimDirZ_ * t;

        // Stop at walls
        if (i > 0 && dungeon_.GetGridW() > 0 && !dungeon_.IsWalkable(wx, wz)) break;

        XMVECTOR wp = XMVectorSet(wx, pos.y + 0.05f, wz, 1.0f);
        XMVECTOR cp = XMVector3TransformCoord(wp, vp);
        float cz = XMVectorGetZ(cp);
        if (cz < 0.0f || cz > 1.0f) { prevValid = false; continue; }

        float sx = (XMVectorGetX(cp) * 0.5f + 0.5f) * screenW;
        float sy = (1.0f - (XMVectorGetY(cp) * 0.5f + 0.5f)) * screenH;
        ImVec2 cur(sx, sy);

        if (prevValid) {
            // Fade alpha along distance
            float alpha = 1.0f - (t / kAimLen) * 0.7f;
            ImU32 lineCol = IM_COL32(255, 255, 255, (int)(alpha * 120));
            ImU32 lineShadow = IM_COL32(0, 0, 0, (int)(alpha * 60));
            dl->AddLine(ImVec2(prevScreen.x + 1, prevScreen.y + 1),
                        ImVec2(cur.x + 1, cur.y + 1), lineShadow, 2.0f);
            dl->AddLine(prevScreen, cur, lineCol, 2.0f);
        }
        prevScreen = cur;
        prevValid = true;
    }

    // --- Ground circle at mouse aim point ---
    auto& mouse = engine_.input->GetMouse();
    XMFLOAT3 groundPt = engine_.camera->ScreenToGround(
        (float)mouse.x, (float)mouse.y, screenW, screenH, pos.y);

    constexpr float kRadius = 0.25f;
    constexpr int kCircleSegs = 24;
    ImU32 circleCol = IM_COL32(255, 220, 50, 200);
    ImU32 circleShadow = IM_COL32(0, 0, 0, 80);
    constexpr float PI2 = 6.28318530f;

    ImVec2 circPts[kCircleSegs];
    bool circValid[kCircleSegs];
    for (int i = 0; i < kCircleSegs; ++i) {
        float angle = PI2 * (float)i / (float)kCircleSegs;
        float wx = groundPt.x + cosf(angle) * kRadius;
        float wz = groundPt.z + sinf(angle) * kRadius;

        XMVECTOR wp = XMVectorSet(wx, pos.y + 0.05f, wz, 1.0f);
        XMVECTOR cp = XMVector3TransformCoord(wp, vp);
        float cz = XMVectorGetZ(cp);
        if (cz < 0.0f || cz > 1.0f) { circValid[i] = false; continue; }

        circPts[i] = ImVec2(
            (XMVectorGetX(cp) * 0.5f + 0.5f) * screenW,
            (1.0f - (XMVectorGetY(cp) * 0.5f + 0.5f)) * screenH);
        circValid[i] = true;
    }

    for (int i = 0; i < kCircleSegs; ++i) {
        int j = (i + 1) % kCircleSegs;
        if (!circValid[i] || !circValid[j]) continue;
        dl->AddLine(ImVec2(circPts[i].x + 1, circPts[i].y + 1),
                    ImVec2(circPts[j].x + 1, circPts[j].y + 1), circleShadow, 2.0f);
        dl->AddLine(circPts[i], circPts[j], circleCol, 2.0f);
    }

    // Center dot
    XMVECTOR centerWp = XMVectorSet(groundPt.x, pos.y + 0.05f, groundPt.z, 1.0f);
    XMVECTOR centerCp = XMVector3TransformCoord(centerWp, vp);
    if (XMVectorGetZ(centerCp) >= 0.0f && XMVectorGetZ(centerCp) <= 1.0f) {
        float cx = (XMVectorGetX(centerCp) * 0.5f + 0.5f) * screenW;
        float cy = (1.0f - (XMVectorGetY(centerCp) * 0.5f + 0.5f)) * screenH;
        dl->AddCircleFilled(ImVec2(cx, cy), 2.0f, circleCol);
    }
}

// ---------------------------------------------------------------------------
// Minimap (replaces currency, rendered via shader)
// ---------------------------------------------------------------------------
void GameScene::DrawMinimap()
{
    float screenW = (float)engine_.device->GetWidth();
    float screenH = (float)engine_.device->GetHeight();
    auto* ctx = engine_.device->GetContext();

    // Build entity list for minimap
    std::vector<MinimapEntity> entities;
    float cs = dungeon_.GetCellSize();
    int gw = dungeon_.GetGridW();
    int gh = dungeon_.GetGridH();

    for (const auto& [id, e] : entityMgr_.GetEntities()) {
        if (e.isDead) continue;
        MinimapEntity me;
        me.gridU = (e.data.position.x / cs + gw / 2.0f) / (float)gw;
        me.gridV = (e.data.position.z / cs + gh / 2.0f) / (float)gh;
        me.r = 1.0f; me.g = 0.3f; me.b = 0.3f;  // red for enemies/other players
        entities.push_back(me);
    }

    // Portals (yellow)
    for (const auto& portal : dungeon_.GetPortals()) {
        MinimapEntity me;
        me.gridU = (portal.position.x / cs + gw / 2.0f) / (float)gw;
        me.gridV = (portal.position.z / cs + gh / 2.0f) / (float)gh;
        me.r = 1.0f; me.g = 0.9f; me.b = 0.1f;
        entities.push_back(me);
    }

    minimapRenderer_.Render(ctx, screenW, screenH,
        session_.localPlayer.position.x, session_.localPlayer.position.z,
        dungeon_, entities);
}

// ---------------------------------------------------------------------------
// UI: Full Map (M key toggle)
// ---------------------------------------------------------------------------
void GameScene::DrawFullMap(float screenW, float screenH)
{
    if (!showFullMap_) return;

    const auto& grid = dungeon_.GetGrid();
    int gw = dungeon_.GetGridW();
    int gh = dungeon_.GetGridH();
    if (gw == 0 || gh == 0 || grid.empty()) return;

    float cs = dungeon_.GetCellSize();
    float cellPx = 4.0f;  // pixels per cell
    float mapW = gw * cellPx;
    float mapH = gh * cellPx;
    float ox = (screenW - mapW) * 0.5f;
    float oy = (screenH - mapH) * 0.5f;

    ImDrawList* dl = ImGui::GetForegroundDrawList();

    // Background
    dl->AddRectFilled(ImVec2(ox - 5, oy - 20), ImVec2(ox + mapW + 5, oy + mapH + 5),
                      IM_COL32(0, 0, 0, 200), 5.0f);
    dl->AddText(ImVec2(ox, oy - 18), IM_COL32(255, 255, 255, 200), "Dungeon Map [M]");

    // Grid cells
    for (int gy2 = 0; gy2 < gh; ++gy2) {
        for (int gx = 0; gx < gw; ++gx) {
            int idx = gx * gh + gy2;
            bool isWall = (idx < (int)grid.size()) ? (grid[idx] != 0) : true;
            ImU32 col = isWall ? IM_COL32(60, 50, 70, 255) : IM_COL32(140, 130, 150, 255);
            float x = ox + gx * cellPx;
            float y = oy + gy2 * cellPx;
            dl->AddRectFilled(ImVec2(x, y), ImVec2(x + cellPx, y + cellPx), col);
        }
    }

    // Portals (yellow squares)
    for (const auto& portal : dungeon_.GetPortals()) {
        float gu = (portal.position.x / cs + gw / 2.0f);
        float gv = (portal.position.z / cs + gh / 2.0f);
        float px = ox + gu * cellPx;
        float py = oy + gv * cellPx;
        ImU32 pcol = (portal.targetName.find("Previous") != std::string::npos)
            ? IM_COL32(100, 200, 255, 255)   // blue = back
            : IM_COL32(255, 220, 50, 255);   // yellow = forward
        dl->AddRectFilled(ImVec2(px - 3, py - 3), ImVec2(px + 3, py + 3), pcol);

        // Label
        ImVec2 ts = ImGui::CalcTextSize(portal.targetName.c_str());
        dl->AddText(ImVec2(px - ts.x * 0.5f, py - 14), pcol, portal.targetName.c_str());
    }

    // Remote entities (red dots)
    for (const auto& [id, e] : entityMgr_.GetEntities()) {
        if (e.isDead) continue;
        float gu = (e.data.position.x / cs + gw / 2.0f);
        float gv = (e.data.position.z / cs + gh / 2.0f);
        float px = ox + gu * cellPx;
        float py = oy + gv * cellPx;
        dl->AddCircleFilled(ImVec2(px, py), 2.5f, IM_COL32(255, 80, 80, 200));
    }

    // Local player (cyan dot)
    {
        float gu = (session_.localPlayer.position.x / cs + gw / 2.0f);
        float gv = (session_.localPlayer.position.z / cs + gh / 2.0f);
        float px = ox + gu * cellPx;
        float py = oy + gv * cellPx;
        dl->AddCircleFilled(ImVec2(px, py), 4.0f, IM_COL32(0, 200, 255, 255));
    }

    // Ground items (orange dots)
    for (const auto& [gid, gi] : groundItems_) {
        float gu = (gi.position.x / cs + gw / 2.0f);
        float gv = (gi.position.z / cs + gh / 2.0f);
        float px = ox + gu * cellPx;
        float py = oy + gv * cellPx;
        dl->AddCircleFilled(ImVec2(px, py), 2.0f, IM_COL32(255, 160, 30, 200));
    }

}

// ---------------------------------------------------------------------------
// UI: Chat (bottom-left)
// ---------------------------------------------------------------------------
void GameScene::DrawChat(float /*screenW*/, float screenH)
{
    ImGui::SetNextWindowPos(ImVec2(10, screenH - 260), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(400, 250), ImGuiCond_Always);
    ImGui::Begin("Chat", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoFocusOnAppearing);

    // Channel selector
    const char* channels[] = { "All", "Zone", "Party", "Whisper" };
    ImGui::SetNextItemWidth(90);
    ImGui::Combo("##channel", &currentChatChannel_, channels, 4);
    ImGui::SameLine();
    ImGui::TextDisabled("(/w name msg)");

    // Message list
    ImGui::BeginChild("##ChatLog", ImVec2(0, -30), true);
    for (const auto& msg : session_.chatHistory.messages) {
        ImVec4 color;
        switch (msg.chatType) {
            case 0: color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); break;        // ALL = white
            case 1: color = ImVec4(0.4f, 1.0f, 0.4f, 1.0f); break;        // ZONE = green
            case 2: color = ImVec4(0.4f, 1.0f, 1.0f, 1.0f); break;        // PARTY = cyan
            case 3: color = ImVec4(1.0f, 0.4f, 1.0f, 1.0f); break;        // WHISPER = magenta
            default: color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        }

        // Format: [channel] sender: message
        const char* chTag = "";
        switch (msg.chatType) {
            case 1: chTag = "[Zone] "; break;
            case 2: chTag = "[Party] "; break;
            case 3: chTag = "[Whisper] "; break;
            default: break;
        }
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TextWrapped("%s[%s] %s", chTag, msg.senderName.c_str(), msg.message.c_str());
        ImGui::PopStyleColor();
    }
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();

    // Input
    ImGui::SetNextItemWidth(-60);
    bool sent = ImGui::InputText("##ChatIn", chatInput_, sizeof(chatInput_),
        ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    if (ImGui::Button("Send") || sent) {
        if (chatInput_[0] != '\0') {
            std::string text = chatInput_;
            std::string target;
            int type = currentChatChannel_;

            // /w playername message -> whisper
            if (text.size() > 3 && text[0] == '/' && text[1] == 'w' && text[2] == ' ') {
                type = 3; // WHISPER
                size_t spacePos = text.find(' ', 3);
                if (spacePos != std::string::npos) {
                    target = text.substr(3, spacePos - 3);
                    text = text.substr(spacePos + 1);
                }
            }

            net_.builder->SendChat(type, text, target);
            chatInput_[0] = '\0';
        }
        ImGui::SetKeyboardFocusHere(-1);
    }
    ImGui::End();
}

// ---------------------------------------------------------------------------
// UI: Inventory (Tab toggle)
// ---------------------------------------------------------------------------
static const char* GetItemName(int defId) {
    if (defId == 100) return "Dog Tag";
    static char buf[16];
    snprintf(buf, sizeof(buf), "Item #%d", defId);
    return buf;
}

void GameScene::DrawInventory(float /*screenW*/, float /*screenH*/)
{
    if (!showInventory_) return;

    ImGui::SetNextWindowSize(ImVec2(320, 400), ImGuiCond_FirstUseEver);
    ImGui::Begin("Inventory", &showInventory_);

    // Collect non-empty items
    struct InvEntry { int slot; const ItemInstance* item; };
    std::vector<InvEntry> items;
    for (int i = 0; i < 100; i++) {
        auto* it = session_.inventory.GetSlot(i);
        if (it && !it->empty()) items.push_back({i, it});
    }

    if (items.empty()) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Empty");
    } else {
        ImGui::Text("%d items", (int)items.size());
        ImGui::Separator();

        for (auto& [slot, item] : items) {
            ImGui::PushID(slot);

            bool isSelected = (selectedSlot_ == slot);
            if (isSelected)
                ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.3f, 0.5f, 0.8f, 1.0f));

            char label[64];
            snprintf(label, sizeof(label), "%-12s  x%d", GetItemName(item->itemDefId), item->quantity);
            if (ImGui::Selectable(label, isSelected)) {
                selectedSlot_ = slot;
            }

            if (isSelected)
                ImGui::PopStyleColor();

            // Right-click context menu
            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Use"))  net_.builder->SendUseItem(item->instanceId);
                if (ImGui::MenuItem("Drop")) net_.builder->SendDropItem(item->instanceId, 1);
                ImGui::EndPopup();
            }

            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s\nQty: %d\nDurability: %d\nRight-click to use/drop",
                    GetItemName(item->itemDefId), item->quantity, item->durability);
            }

            ImGui::PopID();
        }
    }

    ImGui::End();
}

// ---------------------------------------------------------------------------
// UI: Party panel (left side, visible when in party)
// ---------------------------------------------------------------------------
void GameScene::DrawPartyPanel(float /*screenW*/, float /*screenH*/)
{
    if (session_.localPlayer.partyId == 0) return;

    ImGui::SetNextWindowPos(ImVec2(10, 100), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(200, 0), ImGuiCond_FirstUseEver);
    ImGui::Begin("Party", nullptr, ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoFocusOnAppearing);

    for (auto& member : partyMembers_) {
        float hpRatio = (float)member.hp / (float)(std::max)(member.maxHp, 1);
        ImGui::Text("%s%s Lv.%d", member.isLeader ? "[L] " : "    ",
            member.name.c_str(), member.level);
        ImGui::ProgressBar(hpRatio, ImVec2(-1, 12));
        ImGui::Spacing();
    }

    ImGui::Separator();
    if (ImGui::Button("Leave Party", ImVec2(-1, 25))) {
        net_.builder->SendLeaveParty();
    }
    ImGui::End();
}

// ---------------------------------------------------------------------------
// UI: Death overlay
// ---------------------------------------------------------------------------
void GameScene::DrawDeathOverlay(float screenW, float screenH)
{
    if (!session_.isLocalPlayerDead) return;

    auto* drawList = ImGui::GetBackgroundDrawList();

    // Red tint overlay
    drawList->AddRectFilled(ImVec2(0, 0), ImVec2(screenW, screenH), IM_COL32(200, 0, 0, 80));

    // "YOU DIED" text centered
    const char* deathText = "YOU DIED";
    ImVec2 textSize = ImGui::CalcTextSize(deathText);
    drawList->AddText(ImVec2((screenW - textSize.x) / 2, (screenH - textSize.y) / 2 - 20),
                      IM_COL32(255, 50, 50, 255), deathText);

    const char* waitText = "Waiting for respawn...";
    ImVec2 waitSize = ImGui::CalcTextSize(waitText);
    drawList->AddText(ImVec2((screenW - waitSize.x) / 2, (screenH + waitSize.y) / 2 + 10),
                      IM_COL32(200, 200, 200, 200), waitText);
}

// ---------------------------------------------------------------------------
// UI: Damage Popups
// ---------------------------------------------------------------------------
void GameScene::DrawDamagePopups(float screenW, float screenH)
{
    XMMATRIX vp = engine_.camera->GetViewProj();
    for (const auto& popup : combatMgr_.GetPopups()) {
        XMVECTOR worldPos = XMLoadFloat3(&popup.position);
        worldPos = XMVectorSetY(worldPos, XMVectorGetY(worldPos) + popup.offsetY);
        XMVECTOR clipPos = XMVector3TransformCoord(worldPos, vp);
        float sx = (XMVectorGetX(clipPos) * 0.5f + 0.5f) * screenW;
        float sy = (1.0f - (XMVectorGetY(clipPos) * 0.5f + 0.5f)) * screenH;

        // Skip if behind camera
        if (XMVectorGetZ(clipPos) < 0.0f || XMVectorGetZ(clipPos) > 1.0f) continue;

        char dmgText[32];
        ImVec4 color;
        if (popup.damage < 0) {
            snprintf(dmgText, sizeof(dmgText), "+%d", -popup.damage);
            color = ImVec4(0.2f, 1.0f, 0.2f, popup.timer / 1.5f);
        } else {
            snprintf(dmgText, sizeof(dmgText), "-%d", popup.damage);
            color = ImVec4(1.0f, 0.2f, 0.2f, popup.timer / 1.5f);
        }

        ImGui::SetNextWindowPos(ImVec2(sx - 20, sy - 10), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.0f);
        char wndId[32];
        snprintf(wndId, sizeof(wndId), "##dmg%p", (void*)&popup);
        ImGui::Begin(wndId, nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoFocusOnAppearing);
        ImGui::TextColored(color, "%s", dmgText);
        ImGui::End();
    }
}

// ---------------------------------------------------------------------------
// UI: Remote player HP bars (shown briefly after taking damage)
// ---------------------------------------------------------------------------
void GameScene::DrawRemoteHPBars(float screenW, float screenH)
{
    XMMATRIX vp = engine_.camera->GetViewProj();
    constexpr float kBarW = 50.0f;
    constexpr float kBarH = 6.0f;
    ImDrawList* drawList = ImGui::GetForegroundDrawList();

    for (const auto& [id, entity] : entityMgr_.GetEntities()) {
        if (entity.isDead) continue;

        XMVECTOR worldPos = XMLoadFloat3(&entity.data.position);
        worldPos = XMVectorSetY(worldPos, XMVectorGetY(worldPos) + 2.0f);
        XMVECTOR clipPos = XMVector3TransformCoord(worldPos, vp);

        if (XMVectorGetZ(clipPos) < 0.0f || XMVectorGetZ(clipPos) > 1.0f) continue;

        float sx = (XMVectorGetX(clipPos) * 0.5f + 0.5f) * screenW;
        float sy = (1.0f - (XMVectorGetY(clipPos) * 0.5f + 0.5f)) * screenH;

        // Name — always visible
        const char* name = entity.data.name.c_str();
        ImVec2 textSize = ImGui::CalcTextSize(name);
        drawList->AddText(ImVec2(sx - textSize.x * 0.5f, sy - 20.0f),
                          IM_COL32(255, 255, 255, 200), name);

        // HP bar — only when recently hit
        if (entity.hpBarTimer > 0.0f) {
            float hpRatio = (entity.data.maxHp > 0)
                ? (float)entity.data.hp / (float)entity.data.maxHp : 0.0f;
            float alpha = (std::min)(entity.hpBarTimer / 0.5f, 1.0f);

            ImVec2 barMin(sx - kBarW * 0.5f, sy - kBarH * 0.5f);
            ImVec2 barMax(sx + kBarW * 0.5f, sy + kBarH * 0.5f);

            drawList->AddRectFilled(barMin, barMax, IM_COL32(0, 0, 0, (int)(180 * alpha)));
            ImU32 hpColor = IM_COL32(
                (int)((1.0f - hpRatio) * 255),
                (int)(hpRatio * 255),
                0, (int)(220 * alpha));
            drawList->AddRectFilled(barMin,
                ImVec2(barMin.x + kBarW * hpRatio, barMax.y), hpColor);
            drawList->AddRect(barMin, barMax, IM_COL32(255, 255, 255, (int)(120 * alpha)));
        }
    }
}

// ---------------------------------------------------------------------------
// Kill/Death tracking
// ---------------------------------------------------------------------------
GameScene::KDEntry& GameScene::GetOrCreateKD(uint64_t id, const std::string& name)
{
    auto& entry = kdTracker_[id];
    if (entry.playerId == 0) {
        entry.playerId = id;
    }
    if (!name.empty()) {
        entry.name = name;
    }
    // Try to resolve name from known sources
    if (entry.name.empty()) {
        if (id == session_.playerId) {
            entry.name = session_.localPlayer.name;
        } else {
            auto* e = entityMgr_.GetEntity(id);
            if (e) entry.name = e->data.name;
        }
    }
    return entry;
}

void GameScene::RecordKill(uint64_t attackerId, uint64_t targetId)
{
    GetOrCreateKD(attackerId).kills++;
    GetOrCreateKD(targetId).deaths++;
}

// ---------------------------------------------------------------------------
// UI: Leaderboard (Tab hold)
// ---------------------------------------------------------------------------
void GameScene::DrawLeaderboard(float screenW, float screenH)
{
    if (!showLeaderboard_) return;

    // Ensure local player is tracked
    GetOrCreateKD(session_.playerId, session_.localPlayer.name);

    // Sort by kills desc, then deaths asc
    std::vector<KDEntry*> sorted;
    for (auto& [id, entry] : kdTracker_) {
        sorted.push_back(&entry);
    }
    std::sort(sorted.begin(), sorted.end(), [](const KDEntry* a, const KDEntry* b) {
        if (a->kills != b->kills) return a->kills > b->kills;
        return a->deaths < b->deaths;
    });

    float w = 300.0f;
    float h = 40.0f + (float)sorted.size() * 22.0f + 10.0f;
    if (h > screenH * 0.6f) h = screenH * 0.6f;

    ImGui::SetNextWindowPos(ImVec2((screenW - w) * 0.5f, (screenH - h) * 0.3f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(w, h), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.85f);
    ImGui::Begin("##Leaderboard", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoFocusOnAppearing);

    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "  LEADERBOARD");
    ImGui::Separator();

    ImGui::Columns(4, "##lbcols", false);
    ImGui::SetColumnWidth(0, 30.0f);
    ImGui::SetColumnWidth(1, 140.0f);
    ImGui::SetColumnWidth(2, 60.0f);
    ImGui::SetColumnWidth(3, 60.0f);

    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "#");
    ImGui::NextColumn();
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Name");
    ImGui::NextColumn();
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Kills");
    ImGui::NextColumn();
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Deaths");
    ImGui::NextColumn();
    ImGui::Separator();

    int rank = 1;
    for (auto* entry : sorted) {
        bool isLocal = (entry->playerId == session_.playerId);
        ImVec4 textCol = isLocal ? ImVec4(0.3f, 0.9f, 1.0f, 1.0f) : ImVec4(1, 1, 1, 1);

        ImGui::TextColored(textCol, "%d", rank++);
        ImGui::NextColumn();
        ImGui::TextColored(textCol, "%s", entry->name.empty() ? "???" : entry->name.c_str());
        ImGui::NextColumn();
        ImGui::TextColored(textCol, "%d", entry->kills);
        ImGui::NextColumn();
        ImGui::TextColored(textCol, "%d", entry->deaths);
        ImGui::NextColumn();
    }

    ImGui::Columns(1);
    ImGui::End();
}

// ---------------------------------------------------------------------------
// UI: Kill Feed (top-right)
// ---------------------------------------------------------------------------
void GameScene::DrawKillFeed(float screenW, float /*screenH*/)
{
    if (killFeed_.empty()) return;
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    float y = 200.0f;  // below minimap
    for (const auto& kf : killFeed_) {
        float alpha = (std::min)(kf.timer / 0.5f, 1.0f);
        char text[128];
        snprintf(text, sizeof(text), "%s > %s", kf.killer.c_str(), kf.victim.c_str());
        ImVec2 textSize = ImGui::CalcTextSize(text);
        float x = screenW - textSize.x - 20.0f;
        dl->AddRectFilled(ImVec2(x - 5, y - 2), ImVec2(screenW - 10, y + textSize.y + 2),
                          IM_COL32(0, 0, 0, (int)(140 * alpha)), 3.0f);
        dl->AddText(ImVec2(x, y), IM_COL32(255, 80, 80, (int)(255 * alpha)), text);
        y += textSize.y + 8.0f;
    }
}

// ---------------------------------------------------------------------------
// UI: ESC Menu (modal popup)
// ---------------------------------------------------------------------------
void GameScene::DrawExitMenu(float screenW, float screenH)
{
    if (!showExitMenu_) return;

    ImGui::OpenPopup("Exit Game");
    ImVec2 center = ImVec2(screenW * 0.5f, screenH * 0.5f);
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Exit Game", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Disconnect and return to login?");
        ImGui::Spacing();

        if (ImGui::Button("Yes", ImVec2(120, 30))) {
            net_.Disconnect();
            scenes_.ChangeScene(SceneId::Login);
            showExitMenu_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("No", ImVec2(120, 30))) {
            showExitMenu_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

// ---------------------------------------------------------------------------
// Packet Handlers
// ---------------------------------------------------------------------------
void GameScene::OnMoveResponse(const uint8_t* data, int len)
{
    game::S_Move msg;
    if (!msg.ParseFromArray(data, len)) return;

    uint64_t pid = msg.player_id();
    if (pid == session_.playerId) return;

    XMFLOAT3 pos = { msg.position().x(), msg.position().y(), msg.position().z() };
    entityMgr_.UpdateRemotePosition(pid, pos, msg.rotation_y(), msg.state());
}

void GameScene::OnSpawnResponse(const uint8_t* data, int len)
{
    game::S_Spawn msg;
    if (!msg.ParseFromArray(data, len)) return;

    const auto& info = msg.player();
    if (info.player_id() == session_.playerId) return;

    PlayerData pd;
    FillPlayerDataFromInfo(pd, info);
    entityMgr_.SpawnRemote(pd);
}

void GameScene::OnDespawnResponse(const uint8_t* data, int len)
{
    game::S_Despawn msg;
    if (!msg.ParseFromArray(data, len)) return;

    entityMgr_.DespawnRemote(msg.player_id());
}

void GameScene::OnPlayerListResponse(const uint8_t* data, int len)
{
    game::S_PlayerList msg;
    if (!msg.ParseFromArray(data, len)) return;

    for (const auto& info : msg.players()) {
        if (info.player_id() == session_.playerId) continue;

        PlayerData pd;
        FillPlayerDataFromInfo(pd, info);
        entityMgr_.SpawnRemote(pd);
    }
}

void GameScene::OnAttackResponse(const uint8_t* data, int len)
{
    game::S_Attack msg;
    if (!msg.ParseFromArray(data, len)) return;

    XMFLOAT3 firePos = { msg.fire_pos().x(), msg.fire_pos().y(), msg.fire_pos().z() };
    XMFLOAT3 fireDir = { msg.fire_dir().x(), msg.fire_dir().y(), msg.fire_dir().z() };
    combatMgr_.HandleAttack(msg.attacker_id(), msg.target_id(), firePos, fireDir);
}

void GameScene::OnDamageResponse(const uint8_t* data, int len)
{
    game::S_Damage msg;
    if (!msg.ParseFromArray(data, len)) return;

    uint64_t targetId = msg.target_id();
    int damage = msg.damage();
    int remainingHp = msg.remaining_hp();
    bool isDead = msg.is_dead();
    {
        char dbg[128];
        snprintf(dbg, sizeof(dbg), "[Combat] S_DAMAGE target=%llu dmg=%d hp=%d dead=%d\n",
                 targetId, damage, remainingHp, isDead);
        OutputDebugStringA(dbg);
    }

    XMFLOAT3 targetPos = {};
    if (targetId == session_.playerId) {
        targetPos = session_.localPlayer.position;
        session_.localPlayer.hp = remainingHp;
        localHitTimer_ = 0.25f;
        if (isDead) {
            session_.isLocalPlayerDead = true;
        }
    } else {
        auto* entity = entityMgr_.GetEntity(targetId);
        if (entity) {
            targetPos = entity->data.position;
            entity->hitFlashTimer = 0.25f;
            entity->hpBarTimer = 3.0f;
        }
        entityMgr_.UpdateRemoteHP(targetId, remainingHp,
            entityMgr_.GetEntity(targetId) ? entityMgr_.GetEntity(targetId)->data.maxHp : 100, isDead);
    }

    combatMgr_.RemoveProjectile(msg.attacker_id());
    combatMgr_.HandleDamage(targetId, msg.attacker_id(), damage, remainingHp, isDead, targetPos);

    // Track kills/deaths + kill feed
    if (isDead) {
        RecordKill(msg.attacker_id(), targetId);

        KillFeedEntry kf;
        if (msg.attacker_id() == session_.playerId) kf.killer = session_.localPlayer.name;
        else { auto* e = entityMgr_.GetEntity(msg.attacker_id()); kf.killer = e ? e->data.name : "???"; }
        if (targetId == session_.playerId) kf.victim = session_.localPlayer.name;
        else { auto* e = entityMgr_.GetEntity(targetId); kf.victim = e ? e->data.name : "???"; }
        killFeed_.push_back(kf);
        if (killFeed_.size() > 5) killFeed_.erase(killFeed_.begin());
    }
}

void GameScene::OnGroundItemSpawn(const uint8_t* data, int len)
{
    game::S_GroundItemSpawn msg;
    if (!msg.ParseFromArray(data, len)) return;
    ClientGroundItem gi;
    gi.groundId = msg.ground_id();
    gi.itemDefId = msg.item_def_id();
    gi.position = { msg.position().x(), msg.position().y(), msg.position().z() };
    gi.label = msg.label();
    groundItems_[gi.groundId] = gi;
}

void GameScene::OnGroundItemDespawn(const uint8_t* data, int len)
{
    game::S_GroundItemDespawn msg;
    if (!msg.ParseFromArray(data, len)) return;
    groundItems_.erase(msg.ground_id());
}

void GameScene::OnPickupResponse(const uint8_t* data, int len)
{
    game::S_Pickup msg;
    if (!msg.ParseFromArray(data, len)) return;
    // Item already added via S_ITEM_ADD, ground item removed via S_GroundItemDespawn
}

void GameScene::OnPortalResponse(const uint8_t* data, int len)
{
    game::S_Portal msg;
    if (!msg.ParseFromArray(data, len)) return;
    if (!msg.success()) return;

    {
        char note[32];
        std::snprintf(note, sizeof(note), "zone=%u", msg.zone_id());
        EventLogger::Log("portal", note);
    }

    // Rebuild dungeon with new map data
    session_.mapData = msg.map_data();
    dungeon_.BuildFromMapData(session_.mapData);

    // Reset player position
    if (msg.has_player()) {
        session_.localPlayer.position = {
            msg.player().position().x(),
            msg.player().position().y(),
            msg.player().position().z()
        };
    } else {
        session_.localPlayer.position = dungeon_.GetSpawnPosition();
    }

    // Reinitialize systems
    playerCtrl_.Init(&session_.localPlayer);
    playerCtrl_.SetDungeon(&dungeon_);
    combatMgr_.SetDungeon(&dungeon_);
    entityMgr_.DespawnAll();
    minimapRenderer_.BuildGridTexture(engine_.device->GetDevice(), dungeon_);
    RebuildInstanceBatches();  // resync InstanceRenderer with new dungeon
    engine_.camera->SnapTo(session_.localPlayer.position);
    nearPortal_ = false;
    kdTracker_.clear();
    killFeed_.clear();
    groundItems_.clear();

    // Update zone tree
    uint32_t newZoneId = msg.zone_id();
    bool isBack = false;
    // Check if going back (new zone is in our path)
    for (int i = 0; i < (int)zonePath_.size(); ++i) {
        if (zonePath_[i] == newZoneId) {
            zonePath_.resize(i + 1);
            isBack = true;
            break;
        }
    }
    if (!isBack) {
        // Add new zone to tree if not seen before
        bool found = false;
        for (auto& z : zoneTree_) {
            if (z.zoneId == newZoneId) { found = true; break; }
        }
        if (!found) {
            char name[32];
            snprintf(name, sizeof(name), "Zone %u", newZoneId);
            zoneTree_.push_back({newZoneId, name, currentZoneId_});
        }
        zonePath_.push_back(newZoneId);
    }
    currentZoneId_ = newZoneId;

    // Server-Initiated Flow Control: 포털로 새 Room 에 들어오면 서버 측
    // PlayerState 는 scene_ready=false 로 재생성되므로, dungeon rebuild 가
    // 끝난 이 시점에 C_SCENE_READY 를 다시 보내 브로드캐스트 수신을 재개한다.
    net_.builder->SendEmpty(MsgId::C_SCENE_READY);
}

void GameScene::OnScoreboardResponse(const uint8_t* data, int len)
{
    game::S_Scoreboard msg;
    if (!msg.ParseFromArray(data, len)) return;
    kdTracker_.clear();
    for (const auto& e : msg.entries()) {
        KDEntry& kd = kdTracker_[e.player_id()];
        kd.playerId = e.player_id();
        kd.name = e.name();
        kd.kills = e.kills();
        kd.deaths = e.deaths();
    }
}

void GameScene::OnFireResponse(const uint8_t* data, int len)
{
    game::S_Fire msg;
    if (!msg.ParseFromArray(data, len)) return;

    if (msg.player_id() == session_.playerId) return;  // skip own projectile

    XMFLOAT3 firePos = { msg.fire_pos().x(), msg.fire_pos().y(), msg.fire_pos().z() };
    XMFLOAT3 fireDir = { msg.fire_dir().x(), msg.fire_dir().y(), msg.fire_dir().z() };
    combatMgr_.HandleFire(msg.player_id(), firePos, fireDir);
}

void GameScene::OnRespawnResponse(const uint8_t* data, int len)
{
    game::S_Respawn msg;
    if (!msg.ParseFromArray(data, len)) return;

    const auto& info = msg.player();
    if (info.player_id() == session_.playerId) {
        FillPlayerDataFromInfo(session_.localPlayer, info);
        session_.isLocalPlayerDead = false;
        playerCtrl_.Init(&session_.localPlayer);
    } else {
        PlayerData pd;
        FillPlayerDataFromInfo(pd, info);
        entityMgr_.SpawnRemote(pd);
        entityMgr_.UpdateRemoteHP(pd.playerId, pd.hp, pd.maxHp, false);
    }
}

void GameScene::OnChatResponse(const uint8_t* data, int len)
{
    game::S_Chat msg;
    if (!msg.ParseFromArray(data, len)) return;

    ChatMessage cm;
    cm.chatType = (int)msg.chat_type();
    cm.senderId = msg.sender_id();
    cm.senderName = msg.sender_name();
    cm.message = msg.message();
    cm.targetName = msg.target_name();
    session_.chatHistory.Add(std::move(cm));
}

void GameScene::OnCreatePartyResponse(const uint8_t* data, int len)
{
    game::S_CreateParty msg;
    if (!msg.ParseFromArray(data, len)) return;

    if (msg.success()) {
        session_.localPlayer.partyId = msg.party_id();
    }
}

void GameScene::OnJoinPartyResponse(const uint8_t* data, int len)
{
    game::S_JoinParty msg;
    if (!msg.ParseFromArray(data, len)) return;

    if (msg.success()) {
        session_.localPlayer.partyId = msg.party_id();
    }
}

void GameScene::OnLeavePartyResponse(const uint8_t* data, int len)
{
    game::S_LeaveParty msg;
    if (!msg.ParseFromArray(data, len)) return;

    if (msg.success()) {
        session_.localPlayer.partyId = 0;
        partyMembers_.clear();
    }
}

void GameScene::OnPartyUpdateResponse(const uint8_t* data, int len)
{
    game::S_PartyUpdate msg;
    if (!msg.ParseFromArray(data, len)) return;

    session_.localPlayer.partyId = msg.party_id();

    partyMembers_.clear();
    for (const auto& m : msg.members()) {
        PartyMember pm;
        pm.id = m.player_id();
        pm.name = m.player_name();
        pm.hp = m.hp();
        pm.maxHp = (m.max_hp() > 1) ? m.max_hp() : 100;
        pm.level = m.level();
        pm.isLeader = m.is_leader();
        partyMembers_.push_back(std::move(pm));
    }

    // If party_id is 0, party was disbanded
    if (msg.party_id() == 0) {
        partyMembers_.clear();
    }
}

void GameScene::OnInventoryInitResponse(const uint8_t* data, int len)
{
    game::S_InventoryInit msg;
    if (!msg.ParseFromArray(data, len)) return;

    session_.inventory.Clear();
    for (const auto& item : msg.items()) {
        int slot = item.slot();
        auto* s = session_.inventory.GetSlot(slot);
        if (s) {
            s->instanceId = item.instance_id();
            s->itemDefId = item.item_def_id();
            s->slot = item.slot();
            s->quantity = item.quantity();
            s->durability = item.durability();
        }
    }
}

void GameScene::OnUseItemResponse(const uint8_t* data, int len)
{
    game::S_UseItem msg;
    if (!msg.ParseFromArray(data, len)) return;
}

void GameScene::OnDropItemResponse(const uint8_t* data, int len)
{
    game::S_DropItem msg;
    if (!msg.ParseFromArray(data, len)) return;
}

void GameScene::OnMoveItemResponse(const uint8_t* data, int len)
{
    game::S_MoveItem msg;
    if (!msg.ParseFromArray(data, len)) return;

    if (msg.success()) {
        int64_t instId = msg.instance_id();
        int newSlot = msg.new_slot();
        for (auto& slot : session_.inventory.slots) {
            if (slot.instanceId == instId) {
                int oldSlot = slot.slot;
                auto* dst = session_.inventory.GetSlot(newSlot);
                if (dst) {
                    auto* src = session_.inventory.GetSlot(oldSlot);
                    if (src) std::swap(*src, *dst);
                    src->slot = oldSlot;
                    dst->slot = newSlot;
                }
                break;
            }
        }
    }
}

void GameScene::OnItemAddResponse(const uint8_t* data, int len)
{
    game::S_ItemAdd msg;
    if (!msg.ParseFromArray(data, len)) return;

    const auto& item = msg.item();
    auto* s = session_.inventory.GetSlot(item.slot());
    if (s) {
        s->instanceId = item.instance_id();
        s->itemDefId = item.item_def_id();
        s->slot = item.slot();
        s->quantity = item.quantity();
        s->durability = item.durability();
    }
}

void GameScene::OnItemRemoveResponse(const uint8_t* data, int len)
{
    game::S_ItemRemove msg;
    if (!msg.ParseFromArray(data, len)) return;

    int64_t instId = msg.instance_id();
    for (auto& slot : session_.inventory.slots) {
        if (slot.instanceId == instId) {
            slot = {};
            break;
        }
    }
}

void GameScene::OnCurrencyUpdateResponse(const uint8_t* data, int len)
{
    game::S_CurrencyUpdate msg;
    if (!msg.ParseFromArray(data, len)) return;

    session_.currency.gold = msg.gold();
}

void GameScene::OnPurchaseResponse(const uint8_t* data, int len)
{
    game::S_Purchase msg;
    if (!msg.ParseFromArray(data, len)) return;
}

// ============================================================================
// Dungeon instanced rendering
// ============================================================================

void GameScene::RebuildInstanceBatches()
{
    std::vector<std::pair<std::string, XMFLOAT4X4>> staticInsts;
    const auto& insts = dungeon_.GetInstances();
    staticInsts.reserve(insts.size());
    for (const auto& di : insts) {
        staticInsts.emplace_back(di.meshName, di.world);
    }
    instanceRenderer_.Prepare(engine_.device->GetDevice(),
                              engine_.device->GetContext(),
                              staticInsts);
    instancePrepared_ = !insts.empty();

    char buf[128];
    sprintf_s(buf, "[GameScene] RebuildInstanceBatches: %zu instances, %zu batches\n",
              insts.size(), instanceRenderer_.GetBatches().size());
    OutputDebugStringA(buf);
}

void GameScene::RenderDungeonInstanced()
{
    // One DrawIndexedInstanced per mesh type
    auto* ctx = engine_.device->GetContext();
    engine_.pipeline->BindInstanced(ctx);
    instanceRenderer_.Bind(ctx);  // bind StructuredBuffer SRV @ t1

    const auto& batches = instanceRenderer_.GetBatches();
    for (const auto& [meshName, batch] : batches) {
        const Mesh* mesh = engine_.meshCache->Get(meshName);
        if (!mesh) continue;

        // Tell the shader where this batch starts in the SRV
        // (SV_InstanceID does not include StartInstanceLocation, so we pass it via CB)
        PerObjectData pod;
        XMStoreFloat4x4(&pod.World, XMMatrixIdentity()); // unused by VSMainInstanced
        pod.HitFlash = 0.0f;
        pod.InstanceOffset = batch.startIndex;
        engine_.pipeline->UpdatePerObject(ctx, pod);

        UINT offset = 0;
        ID3D11Buffer* vb = mesh->vertexBuffer.Get();
        ctx->IASetVertexBuffers(0, 1, &vb, &mesh->stride, &offset);
        ctx->IASetIndexBuffer(mesh->indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);

        ctx->DrawIndexedInstanced(mesh->indexCount, batch.count, 0, 0, 0);
    }

    // Unbind SRV @ t1 so it doesn't leak into other passes
    ID3D11ShaderResourceView* nullSRV = nullptr;
    ctx->VSSetShaderResources(1, 1, &nullSRV);
}
