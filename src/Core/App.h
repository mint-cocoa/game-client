#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <windows.h>
#include "DX11Device.h"
#include "Timer.h"
#include "Input.h"
#include "UI/UIManager.h"
#include "Scene/SceneManager.h"
#include "Network/TcpClient.h"
#include "Network/PacketFramer.h"
#include "Network/PacketHandler.h"
#include "Network/PacketBuilder.h"
#include "Network/NetworkContext.h"
#include "Renderer/Pipeline.h"
#include "Renderer/Camera.h"
#include "Renderer/MeshCache.h"
#include "Renderer/MaterialManager.h"
#include "Core/EngineContext.h"
#include "Game/SessionState.h"
#include <string>
#include <vector>
#include <functional>

class App {
public:
    bool Init(HWND hwnd, HINSTANCE hInstance, int width, int height);
    void Shutdown();
    void Update(float dt);
    void Render();
    void PollNetwork();   // drains IOCP completions; called once per frame

    EngineContext&  Engine()  { return engineCtx_; }
    SessionState&   Session() { return session_; }
    NetworkContext& Net()     { return netCtx_; }
    SceneManager&   Scenes()  { return sceneManager_; }

private:
    std::string ResolveAssetsDir();
    void OnRoomResponse(const uint8_t* data, int len, bool isCreate);

    // Engine subsystems
    DX11Device device_;
    UIManager uiManager_;
    Timer timer_;
    Input input_;
    Pipeline pipeline_;
    Camera camera_;
    MeshCache meshCache_;
    MaterialManager materialMgr_;
    bool pipelineOk_ = false;
    std::string assetsDir_;

    // Network subsystems
    TcpClient tcp_;
    PacketFramer framer_;
    PacketHandler packetHandler_;
    PacketBuilder packetBuilder_;

    // Game state & scene
    SessionState session_;
    SceneManager sceneManager_;

    // Assembled contexts
    EngineContext engineCtx_;
    NetworkContext netCtx_;
};
