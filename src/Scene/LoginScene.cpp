#include "Scene/LoginScene.h"
#include "Core/EngineContext.h"
#include "Game/SessionState.h"
#include "Network/NetworkContext.h"
#include "Network/PacketBuilder.h"
#include "Network/PacketHandler.h"
#include "Network/TcpClient.h"
#include "Scene/SceneManager.h"
#include "imgui/imgui.h"
#include "Auth.pb.h"

LoginScene::LoginScene(EngineContext& engine, SessionState& session,
                       NetworkContext& net, SceneManager& scenes)
    : engine_(engine), session_(session), net_(net), scenes_(scenes) {}

void LoginScene::OnEnter()
{
    // Register packet handlers
    net_.handler->Register(MsgId::S_LOGIN, [this](const uint8_t* data, int len) {
        OnLoginResponse(data, len);
    });
    net_.handler->Register(MsgId::S_REGISTER, [this](const uint8_t* data, int len) {
        OnRegisterResponse(data, len);
    });

    // Reset UI state
    memset(username_, 0, sizeof(username_));
    memset(password_, 0, sizeof(password_));
    statusMsg_ = "Connecting to server...";
    isLoggingIn_ = false;
    isRegistering_ = false;
    elapsedTime_ = 0.0f;
    connectAttempted_ = false;

    // Start connection immediately
    net_.tcp->onConnected = [this]() {
        statusMsg_ = "Connected. Enter credentials.";
    };
    net_.tcp->onDisconnected = [this]() {
        if (!isLoggingIn_ && !isRegistering_) {
            statusMsg_ = "Disconnected.";
            connectAttempted_ = false;
        }
    };
}

void LoginScene::OnExit()
{
}

void LoginScene::OnUpdate(float dt)
{
    elapsedTime_ += dt;

    // Timeout check for login/register
    if ((isLoggingIn_ || isRegistering_) && (elapsedTime_ - loginStartTime_) > kTimeout) {
        statusMsg_ = "Request timed out. Please try again.";
        isLoggingIn_ = false;
        isRegistering_ = false;
    }
}

void LoginScene::OnRender()
{
}

void LoginScene::OnUI()
{
    ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    ImVec2 winSize(350, 360);
    ImGui::SetNextWindowPos(ImVec2((displaySize.x - winSize.x) * 0.5f,
                                    (displaySize.y - winSize.y) * 0.5f),
                            ImGuiCond_Always);
    ImGui::SetNextWindowSize(winSize, ImGuiCond_Always);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;
    if (ImGui::Begin("Isometric Client", nullptr, flags)) {

        bool connected = net_.IsConnected();

        // Server address
        ImGui::Text("Server");
        if (connected) ImGui::BeginDisabled();
        ImGui::SetNextItemWidth(-80);
        ImGui::InputText("##addr", serverAddr_, sizeof(serverAddr_));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1);
        ImGui::InputInt("##port", &serverPort_, 0, 0);
        if (connected) ImGui::EndDisabled();

        if (!connected) {
            if (ImGui::Button("Connect", ImVec2(-1, 24))) {
                if (net_.Connect(serverAddr_, (uint16_t)serverPort_)) {
                    statusMsg_ = "Connecting...";
                    connectAttempted_ = true;
                } else {
                    statusMsg_ = net_.tcp->GetLastError();
                }
            }
        } else {
            if (ImGui::Button("Disconnect", ImVec2(-1, 24))) {
                net_.Disconnect();
                statusMsg_ = "Disconnected.";
            }
        }

        ImGui::Spacing();
        ImGui::Separator();

        ImGui::Spacing();
        ImGui::Text("Username");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##username", username_, sizeof(username_));

        ImGui::Spacing();
        ImGui::Text("Password");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##password", password_, sizeof(password_), ImGuiInputTextFlags_Password);

        ImGui::Spacing();
        ImGui::Spacing();

        bool busy = isLoggingIn_ || isRegistering_;
        bool emptyFields = (strlen(username_) == 0 || strlen(password_) == 0);
        bool canAct = connected && !busy && !emptyFields;

        // Login button
        if (!canAct) ImGui::BeginDisabled();
        if (ImGui::Button("Login", ImVec2(-1, 30))) {
            statusMsg_ = "Logging in...";
            isLoggingIn_ = true;
            isRegistering_ = false;
            loginStartTime_ = elapsedTime_;
            net_.builder->SendLogin(username_, password_);
        }
        if (!canAct) ImGui::EndDisabled();

        ImGui::Spacing();

        // Register button
        if (!canAct) ImGui::BeginDisabled();
        if (ImGui::Button("Register", ImVec2(-1, 30))) {
            statusMsg_ = "Registering...";
            isRegistering_ = true;
            isLoggingIn_ = false;
            loginStartTime_ = elapsedTime_;
            net_.builder->SendRegister(username_, password_);
        }
        if (!canAct) ImGui::EndDisabled();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (!statusMsg_.empty()) {
            ImGui::TextWrapped("%s", statusMsg_.c_str());
        }
    }
    ImGui::End();
}

void LoginScene::OnLoginResponse(const uint8_t* data, int len)
{
    game::S_Login msg;
    if (!msg.ParseFromArray(data, len)) {
        statusMsg_ = "Failed to parse login response.";
        isLoggingIn_ = false;
        return;
    }

    isLoggingIn_ = false;

    if (msg.success()) {
        session_.playerId = msg.player_id();
        session_.playerName = username_;
        scenes_.ChangeScene(SceneId::CharSelect);
    } else {
        statusMsg_ = "Login failed. Check your credentials.";
    }
}

void LoginScene::OnRegisterResponse(const uint8_t* data, int len)
{
    game::S_Register msg;
    if (!msg.ParseFromArray(data, len)) {
        statusMsg_ = "Failed to parse register response.";
        isRegistering_ = false;
        return;
    }

    isRegistering_ = false;

    if (msg.success()) {
        statusMsg_ = "Registration successful! You can now login.";
    } else {
        statusMsg_ = "Register failed: " + msg.error();
    }
}
