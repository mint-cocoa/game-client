#include "Scene/LobbyScene.h"
#include "Core/EngineContext.h"
#include "Game/SessionState.h"
#include "Network/NetworkContext.h"
#include "Network/PacketBuilder.h"
#include "Network/PacketHandler.h"
#include "Scene/SceneManager.h"
#include "imgui/imgui.h"
#include "Auth.pb.h"

LobbyScene::LobbyScene(EngineContext& engine, SessionState& session,
                        NetworkContext& net, SceneManager& scenes)
    : engine_(engine), session_(session), net_(net), scenes_(scenes) {}

void LobbyScene::OnEnter()
{
    net_.handler->Register(MsgId::S_ROOM_LIST, [this](const uint8_t* data, int len) {
        OnRoomListResponse(data, len);
    });

    // Reset state
    rooms_.clear();
    selectedRoom_ = -1;
    memset(newRoomName_, 0, sizeof(newRoomName_));
    statusMsg_.clear();
    waitingForRoom_ = false;

    // Request room list
    net_.builder->SendRoomList();
}

void LobbyScene::OnExit() {}

void LobbyScene::OnUpdate(float /*dt*/) {}

void LobbyScene::OnRender()
{
    // No 3D rendering in lobby
}

void LobbyScene::OnUI()
{
    ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    ImVec2 winSize(500, 400);
    ImGui::SetNextWindowPos(ImVec2((displaySize.x - winSize.x) * 0.5f,
                                    (displaySize.y - winSize.y) * 0.5f),
                            ImGuiCond_Always);
    ImGui::SetNextWindowSize(winSize, ImGuiCond_Always);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;
    if (ImGui::Begin("Lobby", nullptr, flags)) {

        ImGui::Text("Welcome, %s!", session_.playerName.c_str());
        ImGui::Spacing();

        // Room list table
        ImGui::Text("Rooms");
        if (ImGui::BeginTable("##rooms", 4,
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
                ImVec2(-1, 180))) {

            ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 50);
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Players", ImGuiTableColumnFlags_WidthFixed, 60);
            ImGui::TableSetupColumn("Max", ImGuiTableColumnFlags_WidthFixed, 50);
            ImGui::TableHeadersRow();

            for (int i = 0; i < (int)rooms_.size(); i++) {
                ImGui::TableNextRow();

                bool selected = (selectedRoom_ == i);

                ImGui::TableNextColumn();
                char rowId[32];
                snprintf(rowId, sizeof(rowId), "%u", rooms_[i].zoneId);
                if (ImGui::Selectable(rowId, selected,
                        ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick)) {
                    selectedRoom_ = i;
                    if (ImGui::IsMouseDoubleClicked(0)) {
                        statusMsg_ = "Joining room...";
                        net_.builder->SendJoinRoom(rooms_[i].zoneId);
                    }
                }

                ImGui::TableNextColumn();
                ImGui::TextUnformatted(rooms_[i].name.c_str());

                ImGui::TableNextColumn();
                ImGui::Text("%u", rooms_[i].playerCount);

                ImGui::TableNextColumn();
                ImGui::Text("%u", rooms_[i].maxPlayers);
            }

            ImGui::EndTable();
        }

        // Buttons row
        bool canJoin = (selectedRoom_ >= 0 && selectedRoom_ < (int)rooms_.size() && !waitingForRoom_);
        if (!canJoin) ImGui::BeginDisabled();
        if (ImGui::Button("Join", ImVec2(100, 28))) {
            waitingForRoom_ = true;
            session_.pendingEnterGame = true;
            statusMsg_ = "Joining room...";
            net_.builder->SendJoinRoom(rooms_[selectedRoom_].zoneId);
        }
        if (!canJoin) ImGui::EndDisabled();

        ImGui::SameLine();
        if (waitingForRoom_) ImGui::BeginDisabled();
        if (ImGui::Button("Refresh", ImVec2(100, 28))) {
            statusMsg_ = "Refreshing...";
            net_.builder->SendRoomList();
        }
        if (waitingForRoom_) ImGui::EndDisabled();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Create room
        ImGui::Text("Create New Room");
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 110);
        ImGui::InputText("##newroom", newRoomName_, sizeof(newRoomName_));
        ImGui::SameLine();
        bool canCreate = (strlen(newRoomName_) > 0 && !waitingForRoom_);
        if (!canCreate) ImGui::BeginDisabled();
        if (ImGui::Button("Create Room", ImVec2(-1, 0))) {
            waitingForRoom_ = true;
            session_.pendingEnterGame = true;
            statusMsg_ = "Creating room...";
            net_.builder->SendCreateRoom(newRoomName_);
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

void LobbyScene::OnRoomListResponse(const uint8_t* data, int len)
{
    game::S_RoomList msg;
    if (!msg.ParseFromArray(data, len)) {
        statusMsg_ = "Failed to parse room list.";
        return;
    }

    rooms_.clear();
    for (const auto& r : msg.rooms()) {
        RoomInfo ri;
        ri.zoneId = r.zone_id();
        ri.name = r.room_name();
        ri.playerCount = r.player_count();
        ri.maxPlayers = r.max_players();
        rooms_.push_back(std::move(ri));
    }

    selectedRoom_ = -1;
    statusMsg_.clear();
}
