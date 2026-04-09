#pragma once
#include "IScene.h"
#include <memory>
#include <unordered_map>
#include <functional>

class SceneManager {
public:
    using SceneFactory = std::function<std::unique_ptr<IScene>()>;

    void Register(SceneId id, SceneFactory factory);
    void ChangeScene(SceneId id);
    void Update(float dt);
    void Render();
    void RenderUI();
    SceneId GetCurrentId() const { return currentId_; }

private:
    std::unordered_map<SceneId, SceneFactory> factories_;
    std::unique_ptr<IScene> current_;
    SceneId currentId_ = SceneId::Login;
    SceneId pendingId_ = SceneId::Login;
    bool hasPending_ = false;
};
