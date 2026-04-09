#pragma once

enum class SceneId { Login, CharSelect, Lobby, Game };

class IScene {
public:
    virtual ~IScene() = default;
    virtual void OnEnter() = 0;
    virtual void OnExit() = 0;
    virtual void OnUpdate(float dt) = 0;
    virtual void OnRender() = 0;
    virtual void OnUI() = 0;
};
