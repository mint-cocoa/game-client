#pragma once
#include "Scene/IScene.h"
#include <string>
#include <cstdint>

struct EngineContext;
struct SessionState;
struct NetworkContext;
class SceneManager;

class LoginScene : public IScene {
public:
    LoginScene(EngineContext& engine, SessionState& session,
               NetworkContext& net, SceneManager& scenes);
    void OnEnter() override;
    void OnExit() override;
    void OnUpdate(float dt) override;
    void OnRender() override;
    void OnUI() override;

private:
    void OnLoginResponse(const uint8_t* data, int len);
    void OnRegisterResponse(const uint8_t* data, int len);

    EngineContext&  engine_;
    SessionState&   session_;
    NetworkContext& net_;
    SceneManager&   scenes_;
    char serverAddr_[64] = "127.0.0.1";
    int  serverPort_ = 7777;
    char username_[64] = {};
    char password_[64] = {};
    std::string statusMsg_;
    bool isLoggingIn_ = false;
    bool isRegistering_ = false;
    bool connectAttempted_ = false;
    float elapsedTime_ = 0.0f;
    float loginStartTime_ = 0.0f;
    static constexpr float kTimeout = 10.0f;
};
