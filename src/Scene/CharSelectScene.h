#pragma once
#include "Scene/IScene.h"
#include <string>
#include <vector>
#include <cstdint>

struct EngineContext;
struct SessionState;
struct NetworkContext;
class SceneManager;

class CharSelectScene : public IScene {
public:
    CharSelectScene(EngineContext& engine, SessionState& session,
                    NetworkContext& net, SceneManager& scenes);
    void OnEnter() override;
    void OnExit() override;
    void OnUpdate(float dt) override;
    void OnRender() override;
    void OnUI() override;

private:
    void OnCharListResponse(const uint8_t* data, int len);
    void OnCreateCharResponse(const uint8_t* data, int len);
    void OnSelectCharResponse(const uint8_t* data, int len);
    void OnInventoryInitResponse(const uint8_t* data, int len);
    void OnCurrencyInitResponse(const uint8_t* data, int len);
    void TryFinishSelection();

    EngineContext&  engine_;
    SessionState&   session_;
    NetworkContext& net_;
    SceneManager&   scenes_;

    struct CharInfo {
        uint64_t charId;
        std::string name;
        int level;
    };
    std::vector<CharInfo> characters_;
    int selectedIndex_ = -1;
    char newCharName_[32] = {};
    std::string statusMsg_;

    // 3-flag synchronization
    bool waitingForInit_ = false;
    bool gotSelectChar_ = false;
    bool gotInventoryInit_ = false;
    bool gotCurrencyInit_ = false;
};
