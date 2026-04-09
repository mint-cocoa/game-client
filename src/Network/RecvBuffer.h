#pragma once
#include <cstdint>
#include <cstring>

class RecvBuffer {
public:
    static constexpr int kCapacity = 65536;

    void Append(const uint8_t* data, int len) {
        if (writePos_ + len > kCapacity) Compact();
        if (writePos_ + len > kCapacity) return; // overflow
        memcpy(buf_ + writePos_, data, len);
        writePos_ += len;
    }

    int ReadableSize() const { return writePos_ - readPos_; }
    const uint8_t* ReadPtr() const { return buf_ + readPos_; }
    void Consume(int len) { readPos_ += len; }

    void Compact() {
        int readable = ReadableSize();
        if (readable > 0 && readPos_ > 0)
            memmove(buf_, buf_ + readPos_, readable);
        readPos_ = 0;
        writePos_ = readable;
    }

    void Clear() { readPos_ = writePos_ = 0; }

private:
    uint8_t buf_[kCapacity]{};
    int readPos_ = 0;
    int writePos_ = 0;
};
