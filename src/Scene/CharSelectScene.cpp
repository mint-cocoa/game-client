#include "Scene/CharSelectScene.h"
#include "Core/EngineContext.h"
#include "Game/SessionState.h"
#include "Network/NetworkContext.h"
#include "Network/PacketBuilder.h"
#include "Network/PacketHandler.h"
#include "Scene/SceneManager.h"
#include "imgui/imgui.h"
#include "Auth.pb.h"
#include "Inventory.pb.h"
#include "Currency.pb.h"

CharSelectScene::CharSelectScene(EngineContext& engine, SessionState& session,
                                 NetworkContext& net, SceneManager& scenes)
    : engine_(engine), session_(session), net_(net), scenes_(scenes) {}

void CharSelectScene::OnEnter()
{
    // Register handlers
    net_.handler->Register(MsgId::S_CHAR_LIST, [this](const uint8_t* data, int len) {
        OnCharListResponse(data, len);
    });
    net_.handler->Register(MsgId::S_CREATE_CHAR, [this](const uint8_t* data, int len) {
        OnCreateCharResponse(data, len);
    });
    net_.handler->Register(MsgId::S_SELECT_CHAR, [this](const uint8_t* data, int len) {
        OnSelectCharResponse(data, len);
    });
    net_.handler->Register(MsgId::S_INVENTORY_INIT, [this](const uint8_t* data, int len) {
        OnInventoryInitResponse(data, len);
    });
    net_.handler->Register(MsgId::S_CURRENCY_INIT, [this](const uint8_t* data, int len) {
        OnCurrencyInitResponse(data, len);
    });

    // Reset state
    characters_.clear();
    selectedIndex_ = -1;
    memset(newCharName_, 0, sizeof(newCharName_));
    statusMsg_.clear();
    waitingForInit_ = false;
    gotSelectChar_ = false;
    gotInventoryInit_ = false;
    gotCurrencyInit_ = false;

    // Request character list
    net_.builder->SendCharList();
}

void CharSelectScene::OnExit() {}

void CharSelectScene::OnUpdate(float /*dt*/) {}

void CharSelectScene::OnRender()
{
    // No 3D rendering in char select
}

void CharSelectScene::OnUI()
{
    ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    ImVec2 winSize(400, 400);
    ImGui::SetNextWindowPos(ImVec2((displaySize.x - winSize.x) * 0.5f,
                                    (displaySize.y - winSize.y) * 0.5f),
                            ImGuiCond_Always);
    ImGui::SetNextWindowSize(winSize, ImGuiCond_Always);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;
    if (ImGui::Begin("Character Select", nullptr, flags)) {

        // Character list
        ImGui::Text("Characters");
        if (ImGui::BeginListBox("##charlist", ImVec2(-1, 150))) {
            for (int i = 0; i < (int)characters_.size(); i++) {
                char label[128];
                snprintf(label, sizeof(label), "%s (Lv.%d)", characters_[i].name.c_str(), characters_[i].level);
                bool selected = (selectedIndex_ == i);
                if (ImGui::Selectable(label, selected))
                    selectedIndex_ = i;
            }
            ImGui::EndListBox();
        }

        // Select button
        bool canSelect = (selectedIndex_ >= 0 && selectedIndex_ < (int)characters_.size() && !waitingForInit_);
        if (!canSelect) ImGui::BeginDisabled();
        if (ImGui::Button("Select Character", ImVec2(-1, 28))) {
            waitingForInit_ = true;
            gotSelectChar_ = false;
            gotInventoryInit_ = false;
            gotCurrencyInit_ = false;
            statusMsg_ = "Selecting character...";
            net_.builder->SendSelectChar(characters_[selectedIndex_].charId);
        }
        if (!canSelect) ImGui::EndDisabled();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Create new character
        ImGui::Text("Create New Character");
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 100);
        ImGui::InputText("##newchar", newCharName_, sizeof(newCharName_));
        ImGui::SameLine();
        bool canCreate = (strlen(newCharName_) > 0 && !waitingForInit_);
        if (!canCreate) ImGui::BeginDisabled();
        if (ImGui::Button("Create", ImVec2(-1, 0))) {
            statusMsg_ = "Creating character...";
            net_.builder->SendCreateChar(newCharName_);
        }
        if (!canCreate) ImGui::EndDisabled();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (!statusMsg_.empty()) {
            ImGui::TextWrapped("%s", statusMsg_.c_str());
        }
    }
    ImGui::End();
}

void CharSelectScene::OnCharListResponse(const uint8_t* data, int len)
{
    game::S_CharList msg;
    if (!msg.ParseFromArray(data, len)) {
        statusMsg_ = "Failed to parse character list.";
        return;
    }

    characters_.clear();
    for (const auto& c : msg.characters()) {
        CharInfo ci;
        ci.charId = c.char_id();
        ci.name = c.name();
        ci.level = c.level();
        characters_.push_back(std::move(ci));
    }

    if (characters_.empty()) {
        statusMsg_ = "No characters found. Create one below.";
    } else {
        statusMsg_.clear();
    }
}

void CharSelectScene::OnCreateCharResponse(const uint8_t* data, int len)
{
    game::S_CreateChar msg;
    if (!msg.ParseFromArray(data, len)) {
        statusMsg_ = "Failed to parse create character response.";
        return;
    }

    if (msg.success()) {
        // Add to list
        const auto& c = msg.character();
        CharInfo ci;
        ci.charId = c.char_id();
        ci.name = c.name();
        ci.level = c.level();
        characters_.push_back(std::move(ci));
        memset(newCharName_, 0, sizeof(newCharName_));
        statusMsg_ = "Character created!";
    } else {
        statusMsg_ = "Create failed: " + msg.error();
    }
}

void CharSelectScene::OnSelectCharResponse(const uint8_t* data, int len)
{
    game::S_SelectChar msg;
    if (!msg.ParseFromArray(data, len)) {
        statusMsg_ = "Failed to parse select character response.";
        waitingForInit_ = false;
        return;
    }

    if (msg.success()) {
        session_.playerId = msg.player_id();
        session_.playerName = msg.name();
        session_.localPlayer.playerId = msg.player_id();
        session_.localPlayer.name = msg.name();
        session_.localPlayer.level = msg.level();
        gotSelectChar_ = true;
        TryFinishSelection();
    } else {
        statusMsg_ = "Select character failed.";
        waitingForInit_ = false;
    }
}

void CharSelectScene::OnInventoryInitResponse(const uint8_t* data, int len)
{
    game::S_InventoryInit msg;
    if (!msg.ParseFromArray(data, len)) {
        statusMsg_ = "Failed to parse inventory data.";
        return;
    }

    session_.inventory.Clear();
    for (const auto& item : msg.items()) {
        int slot = item.slot();
        auto* slotPtr = session_.inventory.GetSlot(slot);
        if (slotPtr) {
            slotPtr->instanceId = item.instance_id();
            slotPtr->itemDefId = item.item_def_id();
            slotPtr->slot = item.slot();
            slotPtr->quantity = item.quantity();
            slotPtr->durability = item.durability();
        }
    }

    gotInventoryInit_ = true;
    TryFinishSelection();
}

void CharSelectScene::OnCurrencyInitResponse(const uint8_t* data, int len)
{
    game::S_CurrencyInit msg;
    if (!msg.ParseFromArray(data, len)) {
        statusMsg_ = "Failed to parse currency data.";
        return;
    }

    session_.currency.gold = msg.gold();

    gotCurrencyInit_ = true;
    TryFinishSelection();
}

void CharSelectScene::TryFinishSelection()
{
    if (gotSelectChar_ && gotInventoryInit_ && gotCurrencyInit_) {
        scenes_.ChangeScene(SceneId::Lobby);
    }
}
