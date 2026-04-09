#pragma once
#include <windows.h>
#include <cstdint>

struct MouseState {
    int x = 0, y = 0;
    int deltaX = 0, deltaY = 0;
    float scrollDelta = 0.0f;
    bool left = false, right = false, middle = false;
};

class Input {
public:
    void NewFrame();
    void OnKeyDown(WPARAM key);
    void OnKeyUp(WPARAM key);
    void OnMouseMove(int x, int y);
    void OnMouseButton(int button, bool down);
    void OnMouseWheel(float delta);

    bool IsKeyDown(int vk) const { return keys_[vk & 0xFF]; }
    bool IsKeyPressed(int vk) const { return keys_[vk & 0xFF] && !prevKeys_[vk & 0xFF]; }
    bool IsMousePressed(int button) const;
    const MouseState& GetMouse() const { return mouse_; }

private:
    bool keys_[256] = {};
    bool prevKeys_[256] = {};
    MouseState mouse_{};
    bool prevMouseLeft_ = false, prevMouseRight_ = false, prevMouseMiddle_ = false;
    int prevMouseX_ = 0, prevMouseY_ = 0;
};
