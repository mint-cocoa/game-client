#pragma once
#include "MsgId.h"
#include <functional>
#include <unordered_map>

class PacketHandler {
public:
    using Handler = std::function<void(const uint8_t*, int)>;

    void Register(MsgId id, Handler handler) {
        handlers_[static_cast<uint16_t>(id)] = std::move(handler);
    }

    void Dispatch(uint16_t rawId, const uint8_t* payload, int len) {
        auto it = handlers_.find(rawId);
        if (it != handlers_.end()) {
            it->second(payload, len);
        } else {
            char buf[128];
            snprintf(buf, sizeof(buf), "[PKT] NO HANDLER msgId=%u len=%d\n", rawId, len);
            OutputDebugStringA(buf);
        }
    }

private:
    std::unordered_map<uint16_t, Handler> handlers_;
};
