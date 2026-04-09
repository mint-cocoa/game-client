#pragma once
#include <cstdint>
#include <string>

class TcpClient;
class PacketHandler;
class PacketBuilder;

struct NetworkContext {
    TcpClient*     tcp     = nullptr;
    PacketHandler* handler = nullptr;
    PacketBuilder* builder = nullptr;

    bool IsConnected() const;
    bool Connect(const std::string& host = "127.0.0.1", uint16_t port = 7777);
    void Disconnect();
};
