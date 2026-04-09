#include "Input.h"
#include <cstring>

void Input::NewFrame()
{
    memcpy(prevKeys_, keys_, sizeof(keys_));
    prevMouseLeft_ = mouse_.left;
    prevMouseRight_ = mouse_.right;
    prevMouseMiddle_ = mouse_.middle;
    mouse_.deltaX = mouse_.x - prevMouseX_;
    mouse_.deltaY = mouse_.y - prevMouseY_;
    prevMouseX_ = mouse_.x;
    prevMouseY_ = mouse_.y;
    mouse_.scrollDelta = 0.0f;
}

void Input::OnKeyDown(WPARAM key) { if (key < 256) keys_[key] = true; }
void Input::OnKeyUp(WPARAM key) { if (key < 256) keys_[key] = false; }
void Input::OnMouseMove(int x, int y) { mouse_.x = x; mouse_.y = y; }
void Input::OnMouseButton(int button, bool down) {
    if (button == 0) mouse_.left = down;
    else if (button == 1) mouse_.right = down;
    else if (button == 2) mouse_.middle = down;
}
void Input::OnMouseWheel(float delta) { mouse_.scrollDelta += delta; }

bool Input::IsMousePressed(int button) const {
    if (button == 0) return mouse_.left && !prevMouseLeft_;
    if (button == 1) return mouse_.right && !prevMouseRight_;
    if (button == 2) return mouse_.middle && !prevMouseMiddle_;
    return false;
}
