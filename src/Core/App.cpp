#include "Core/App.h"
#include "Core/EventLogger.h"
#include <cstdio>
#include "Scene/LoginScene.h"
#include "Scene/CharSelectScene.h"
#include "Scene/LobbyScene.h"
#include "Scene/GameScene.h"
#include "Auth.pb.h"
#include "Game.pb.h"

static std::string GetExeDir()
{
    char buf[MAX_PATH];
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    std::string path(buf);
    auto pos = path.find_last_of("\\/");
    return (pos != std::string::npos) ? path.substr(0, pos) : path;
}

std::string App::ResolveAssetsDir()
{
    std::string exeDir = GetExeDir();
    WIN32_FIND_DATAA fd;
    std::string candidates[] = {
        exeDir + "/../../assets",
        exeDir + "/../assets",
        exeDir + "/assets",
    };
    std::string result = candidates[0];
    for (auto& c : candidates) {
        HANDLE hFind = FindFirstFileA((c + "/shaders").c_str(), &fd);
        if (hFind != INVALID_HANDLE_VALUE) {
            FindClose(hFind);
            result = c;
            break;
        }
    }
    return result;
}

bool App::Init(HWND hwnd, HINSTANCE /*hInstance*/, int width, int height)
{
    // Demo-video event log. Written to the current working directory so that
    // launching with /D <dir> (e.g. via run_demo.bat) gives each client
    // instance its own output folder. PID is still included in the filename
    // so runs from the same working directory don't overwrite each other.
    // Start OBS recording at the same moment you launch the client so timestamps align.
    {
        char cwd[MAX_PATH];
        DWORD len = GetCurrentDirectoryA(MAX_PATH, cwd);
        std::string dir = (len > 0) ? std::string(cwd) : GetExeDir();
        char fname[64];
        std::snprintf(fname, sizeof(fname), "/events_%lu.jsonl",
                      static_cast<unsigned long>(GetCurrentProcessId()));
        EventLogger::Init(dir + fname);
    }

    // DX11 + ImGui
    if (!device_.Init(hwnd, width, height)) return false;
    if (!uiManager_.Init(hwnd, device_.GetDevice(), device_.GetContext())) return false;

    timer_.Init();

    // Wire engine context
    engineCtx_.device      = &device_;
    engineCtx_.pipeline    = &pipeline_;
    engineCtx_.camera      = &camera_;
    engineCtx_.meshCache   = &meshCache_;
    engineCtx_.materialMgr = &materialMgr_;
    engineCtx_.input       = &input_;
    engineCtx_.uiManager   = &uiManager_;
    engineCtx_.assetsDir   = &assetsDir_;

    // Wire network context
    netCtx_.tcp     = &tcp_;
    netCtx_.handler = &packetHandler_;
    netCtx_.builder = &packetBuilder_;

    // Network wiring (IOCP-based; HWND not needed)
    if (!tcp_.Init()) return false;

    tcp_.onData = [this](const uint8_t* data, int len) {
        framer_.OnRecv(data, len);
    };

    framer_.SetCallback([this](uint16_t msgId, const uint8_t* payload, int payloadLen) {
        packetHandler_.Dispatch(msgId, payload, payloadLen);
    });

    tcp_.onDisconnected = [this]() {
        framer_.Clear();
        session_.Reset();
        sceneManager_.ChangeScene(SceneId::Login);
    };

    packetBuilder_.Init(&tcp_);

    // App-level persistent handlers for room responses
    packetHandler_.Register(MsgId::S_CREATE_ROOM, [this](const uint8_t* data, int len) {
        OnRoomResponse(data, len, true);
    });
    packetHandler_.Register(MsgId::S_JOIN_ROOM, [this](const uint8_t* data, int len) {
        OnRoomResponse(data, len, false);
    });
    // Rendering
    assetsDir_ = ResolveAssetsDir();
    pipelineOk_ = pipeline_.Init(device_.GetDevice(), assetsDir_ + "/shaders/default.hlsl");
    engineCtx_.pipelineOk = pipelineOk_;
    camera_.Init((float)width / (float)height);
    meshCache_.Init(device_.GetDevice(), assetsDir_ + "/models", assetsDir_ + "/fbx");

    // Material system
    materialMgr_.Init(device_.GetDevice(), assetsDir_ + "/textures");

    // Assign materials to mesh groups
    for (auto& name : {"floor", "floor-detail", "wall", "wall-half", "wall-narrow", "wall-opening",
                        "barrel", "chest", "column", "rocks", "stones", "banner", "trap", "gate",
                        "stairs", "dirt", "wood-structure", "wood-support", "coin"})
        meshCache_.SetMeshMaterial(name, "M_Dungeon");
    meshCache_.SetMeshMaterial("character-human", "M_LocalPlayer");
    meshCache_.SetMeshMaterial("character-a", "M_RemotePlayer");
    meshCache_.SetMeshMaterial("bullet", "M_Bullet");

    // Register scenes
    sceneManager_.Register(SceneId::Login, [this]() {
        return std::make_unique<LoginScene>(engineCtx_, session_, netCtx_, sceneManager_);
    });
    sceneManager_.Register(SceneId::CharSelect, [this]() {
        return std::make_unique<CharSelectScene>(engineCtx_, session_, netCtx_, sceneManager_);
    });
    sceneManager_.Register(SceneId::Lobby, [this]() {
        return std::make_unique<LobbyScene>(engineCtx_, session_, netCtx_, sceneManager_);
    });
    sceneManager_.Register(SceneId::Game, [this]() {
        return std::make_unique<GameScene>(engineCtx_, session_, netCtx_, sceneManager_);
    });

    // Start at login
    sceneManager_.ChangeScene(SceneId::Login);

    return true;
}

void App::Shutdown()
{
    tcp_.Shutdown();
    uiManager_.Shutdown();
    device_.Shutdown();
    EventLogger::Shutdown();
}

void App::Update(float dt)
{
    if (device_.GetWidth() > 0 && device_.GetHeight() > 0) {
        camera_.SetAspect((float)device_.GetWidth() / (float)device_.GetHeight());
    }

    sceneManager_.Update(dt);
}

void App::Render()
{
    device_.BeginFrame(0.1f, 0.1f, 0.15f);

    sceneManager_.Render();

    // ImGui
    uiManager_.BeginFrame();
    sceneManager_.RenderUI();
    uiManager_.EndFrame();

    device_.EndFrame();
}

void App::PollNetwork()
{
    tcp_.Poll();
}

void App::OnRoomResponse(const uint8_t* data, int len, bool isCreate)
{
    if (isCreate) {
        game::S_CreateRoom msg;
        if (!msg.ParseFromArray(data, len)) {
            OutputDebugStringA("[App] Failed to parse S_CreateRoom\n");
            session_.pendingEnterGame = false;
            return;
        }
        if (!msg.success()) {
            session_.pendingEnterGame = false;
            return;
        }
        session_.FillLocalPlayerFromProto(msg.player());
        if (msg.has_map_data()) {
            session_.mapData = msg.map_data();
            session_.hasMapData = true;
        }
    } else {
        game::S_JoinRoom msg;
        if (!msg.ParseFromArray(data, len)) {
            OutputDebugStringA("[App] Failed to parse S_JoinRoom\n");
            session_.pendingEnterGame = false;
            return;
        }
        if (!msg.success()) {
            session_.pendingEnterGame = false;
            return;
        }
        session_.FillLocalPlayerFromProto(msg.player());
        if (msg.has_map_data()) {
            session_.mapData = msg.map_data();
            session_.hasMapData = true;
        }
    }

    session_.pendingEnterGame = false;
    sceneManager_.ChangeScene(SceneId::Game);
}
