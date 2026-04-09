#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>          // LPFN_CONNECTEX, WSAID_CONNECTEX
#include <functional>
#include <cstdint>
#include <string>
#include <vector>
#include <deque>

// Single-connection async TCP client built on Overlapped I/O + IOCP.
// Drives all completions on the main thread via Poll(); no extra threads,
// no window messages, no busy-wait sends.
class TcpClient {
public:
    using OnConnectedFn    = std::function<void()>;
    using OnDisconnectedFn = std::function<void()>;
    using OnDataFn         = std::function<void(const uint8_t*, int)>;

    bool Init();
    void Shutdown();

    bool Connect(const char* host, uint16_t port);
    void Disconnect();

    // Queues a payload for asynchronous WSASend. Returns false only if not
    // connected; otherwise the data is owned by the queue until completion.
    bool Send(const uint8_t* data, int len);

    // Drains all completed I/O for this frame. Must be called once per
    // frame from the main loop. Fires onConnected/onData/onDisconnected
    // callbacks synchronously on the calling thread.
    void Poll();

    bool               IsConnected()  const { return connected_; }
    const std::string& GetLastError() const { return lastError_; }

    OnConnectedFn    onConnected;
    OnDisconnectedFn onDisconnected;
    OnDataFn         onData;

private:
    enum class OpType { Connect, Recv, Send };

    struct IoContext {
        OVERLAPPED        ov{};        // MUST be first; we cast back via CONTAINING_RECORD
        OpType            op;
        WSABUF            buf{};
        std::vector<char> storage;     // backing store for buf.buf
    };

    bool LoadConnectEx();
    void PostRecv();                   // (re)post the perpetual recv
    void TrySubmitNextSend();          // dispatch one queued send if idle
    void OnIoCompletion(IoContext* ctx, DWORD bytes, BOOL ok, DWORD err);
    void DoDisconnect(bool fireCallback);
    void CleanupAllIo();

    SOCKET sock_       = INVALID_SOCKET;
    HANDLE iocp_       = nullptr;
    bool   connected_  = false;
    bool   wsaStarted_ = false;
    LPFN_CONNECTEX connectExPtr_ = nullptr;

    IoContext* recvCtx_    = nullptr;  // single perpetual recv
    IoContext* connectCtx_ = nullptr;
    IoContext* sendCtx_    = nullptr;  // current in-flight send (one at a time)

    std::deque<std::vector<char>> sendQueue_;  // FIFO of pending payloads

    std::string lastError_;
    static constexpr int kRecvBufSize = 65536;
};
