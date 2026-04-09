#pragma once
#include "Scene/IScene.h"
#include <string>
#include <vector>
#include <cstdint>

struct EngineContext;
struct SessionState;
struct NetworkContext;
class SceneManager;

class LobbyScene : public IScene {
public:
    LobbyScene(EngineContext& engine, SessionState& session,
               NetworkContext& net, SceneManager& scenes);
    void OnEnter() override;
    void OnExit() override;
    void OnUpdate(float dt) override;
    void OnRender() override;
    void OnUI() override;

private:
    void OnRoomListResponse(const uint8_t* data, int len);

    EngineContext&  engine_;
    SessionState&   session_;
    NetworkContext& net_;
    SceneManager&   scenes_;

    struct RoomInfo {
        uint32_t zoneId;
        std::string name;
        uint32_t playerCount;
        uint32_t maxPlayers;
    };
    std::vector<RoomInfo> rooms_;
    int selectedRoom_ = -1;
    char newRoomName_[64] = {};
    std::string statusMsg_;
    bool waitingForRoom_ = false;
};
