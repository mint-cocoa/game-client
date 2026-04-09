#include "SceneManager.h"

void SceneManager::Register(SceneId id, SceneFactory factory) {
    factories_[id] = std::move(factory);
}

void SceneManager::ChangeScene(SceneId id) {
    pendingId_ = id;
    hasPending_ = true;
}

void SceneManager::Update(float dt) {
    if (hasPending_) {
        hasPending_ = false;
        if (current_)
            current_->OnExit();

        auto it = factories_.find(pendingId_);
        if (it != factories_.end()) {
            current_ = it->second();
            currentId_ = pendingId_;
            if (current_)
                current_->OnEnter();
        }
    }
    if (current_)
        current_->OnUpdate(dt);
}

void SceneManager::Render() {
    if (current_)
        current_->OnRender();
}

void SceneManager::RenderUI() {
    if (current_)
        current_->OnUI();
}
