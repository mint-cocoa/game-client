#pragma once
#include "RecvBuffer.h"
#include <functional>

class PacketFramer {
public:
    using PacketCallback = std::function<void(uint16_t msgId, const uint8_t* payload, int payloadLen)>;

    void SetCallback(PacketCallback cb) { callback_ = std::move(cb); }

    void OnRecv(const uint8_t* data, int len) {
        buffer_.Append(data, len);
        ProcessBuffer();
    }

    void Clear() { buffer_.Clear(); }

private:
    void ProcessBuffer() {
        while (buffer_.ReadableSize() >= 4) {
            const uint8_t* ptr = buffer_.ReadPtr();
            uint16_t size = ptr[0] | (ptr[1] << 8);
            if (size < 4) { buffer_.Clear(); break; }
            if (buffer_.ReadableSize() < size) break;
            uint16_t msgId = ptr[2] | (ptr[3] << 8);
            if (callback_) callback_(msgId, ptr + 4, size - 4);
            buffer_.Consume(size);
        }
    }

    RecvBuffer buffer_;
    PacketCallback callback_;
};
