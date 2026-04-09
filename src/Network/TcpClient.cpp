#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#include "TcpClient.h"

// IOCP completion-key tag for our single socket. Any non-zero value works;
// we just need to distinguish "our completions" from accidental zeros and
// from the shutdown wakeup if we ever post one.
static constexpr ULONG_PTR kCompletionKey = 0xC11E;

bool TcpClient::Init() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        return false;
    wsaStarted_ = true;

    iocp_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
    if (!iocp_) {
        lastError_ = "CreateIoCompletionPort failed";
        WSACleanup();
        wsaStarted_ = false;
        return false;
    }
    return true;
}

void TcpClient::Shutdown() {
    Disconnect();
    if (iocp_) {
        CloseHandle(iocp_);
        iocp_ = nullptr;
    }
    if (wsaStarted_) {
        WSACleanup();
        wsaStarted_ = false;
    }
}

bool TcpClient::LoadConnectEx() {
    GUID guid = WSAID_CONNECTEX;
    DWORD bytes = 0;
    int rc = WSAIoctl(sock_, SIO_GET_EXTENSION_FUNCTION_POINTER,
                      &guid, sizeof(guid),
                      &connectExPtr_, sizeof(connectExPtr_),
                      &bytes, nullptr, nullptr);
    if (rc != 0) {
        lastError_ = "WSAIoctl(SIO_GET_EXTENSION_FUNCTION_POINTER) failed: "
                     + std::to_string(WSAGetLastError());
        return false;
    }
    return connectExPtr_ != nullptr;
}

bool TcpClient::Connect(const char* host, uint16_t port) {
    if (connected_ || sock_ != INVALID_SOCKET) Disconnect();

    // Resolve hostname (supports both IP and domain names).
    addrinfo hints = {};
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    char portStr[8];
    snprintf(portStr, sizeof(portStr), "%u", port);

    addrinfo* result = nullptr;
    if (getaddrinfo(host, portStr, &hints, &result) != 0 || !result) {
        lastError_ = "Failed to resolve: " + std::string(host);
        return false;
    }

    // Overlapped socket — required for IOCP and ConnectEx.
    sock_ = WSASocketW(result->ai_family, result->ai_socktype, result->ai_protocol,
                       nullptr, 0, WSA_FLAG_OVERLAPPED);
    if (sock_ == INVALID_SOCKET) {
        lastError_ = "WSASocketW failed: " + std::to_string(WSAGetLastError());
        freeaddrinfo(result);
        return false;
    }

    // ConnectEx requires the socket to be explicitly bound first.
    sockaddr_in localAddr = {};
    localAddr.sin_family      = AF_INET;
    localAddr.sin_addr.s_addr = INADDR_ANY;
    localAddr.sin_port        = 0;
    if (bind(sock_, (sockaddr*)&localAddr, sizeof(localAddr)) == SOCKET_ERROR) {
        lastError_ = "bind failed: " + std::to_string(WSAGetLastError());
        closesocket(sock_);
        sock_ = INVALID_SOCKET;
        freeaddrinfo(result);
        return false;
    }

    // Associate with our IOCP. From this point all I/O on sock_ delivers
    // completions to iocp_.
    if (!CreateIoCompletionPort((HANDLE)sock_, iocp_, kCompletionKey, 0)) {
        lastError_ = "CreateIoCompletionPort(socket) failed: "
                     + std::to_string(::GetLastError());
        closesocket(sock_);
        sock_ = INVALID_SOCKET;
        freeaddrinfo(result);
        return false;
    }

    if (!LoadConnectEx()) {
        closesocket(sock_);
        sock_ = INVALID_SOCKET;
        freeaddrinfo(result);
        return false;
    }

    // Post the async ConnectEx. The completion is delivered through Poll().
    connectCtx_     = new IoContext();
    connectCtx_->op = OpType::Connect;

    BOOL ok = connectExPtr_(sock_, result->ai_addr, (int)result->ai_addrlen,
                            nullptr, 0, nullptr, &connectCtx_->ov);
    int  err = WSAGetLastError();
    freeaddrinfo(result);

    if (!ok && err != ERROR_IO_PENDING) {
        lastError_ = "ConnectEx failed: " + std::to_string(err);
        delete connectCtx_;
        connectCtx_ = nullptr;
        closesocket(sock_);
        sock_ = INVALID_SOCKET;
        return false;
    }

    lastError_.clear();
    return true;
}

void TcpClient::PostRecv() {
    if (sock_ == INVALID_SOCKET) return;

    if (!recvCtx_) {
        recvCtx_         = new IoContext();
        recvCtx_->op     = OpType::Recv;
        recvCtx_->storage.resize(kRecvBufSize);
    }
    // Reset OVERLAPPED for re-use across multiple posts.
    ZeroMemory(&recvCtx_->ov, sizeof(recvCtx_->ov));
    recvCtx_->buf.buf = recvCtx_->storage.data();
    recvCtx_->buf.len = (ULONG)recvCtx_->storage.size();

    DWORD flags = 0;
    DWORD bytes = 0;
    int rc = WSARecv(sock_, &recvCtx_->buf, 1, &bytes, &flags, &recvCtx_->ov, nullptr);
    if (rc == SOCKET_ERROR) {
        int err = WSAGetLastError();
        if (err != WSA_IO_PENDING) {
            // Recv setup failed — drop and clean up. The completion handler
            // path won't fire, so do it inline.
            lastError_ = "WSARecv failed: " + std::to_string(err);
            DoDisconnect(true);
        }
    }
    // On synchronous success WSARecv still posts a completion to IOCP, so we
    // don't dispatch inline — Poll() will pick it up.
}

bool TcpClient::Send(const uint8_t* data, int len) {
    if (!connected_ || sock_ == INVALID_SOCKET || len <= 0) return false;

    sendQueue_.emplace_back(data, data + len);
    TrySubmitNextSend();
    return true;
}

void TcpClient::TrySubmitNextSend() {
    if (sendCtx_) return;                  // already in-flight
    if (sendQueue_.empty()) return;
    if (sock_ == INVALID_SOCKET) return;

    sendCtx_         = new IoContext();
    sendCtx_->op     = OpType::Send;
    sendCtx_->storage = std::move(sendQueue_.front());
    sendQueue_.pop_front();
    sendCtx_->buf.buf = sendCtx_->storage.data();
    sendCtx_->buf.len = (ULONG)sendCtx_->storage.size();

    DWORD bytes = 0;
    int rc = WSASend(sock_, &sendCtx_->buf, 1, &bytes, 0, &sendCtx_->ov, nullptr);
    if (rc == SOCKET_ERROR) {
        int err = WSAGetLastError();
        if (err != WSA_IO_PENDING) {
            lastError_ = "WSASend failed: " + std::to_string(err);
            delete sendCtx_;
            sendCtx_ = nullptr;
            DoDisconnect(true);
        }
    }
}

void TcpClient::Poll() {
    if (!iocp_) return;

    // Drain everything that's already complete this frame. Timeout 0 ms.
    for (;;) {
        DWORD       bytes = 0;
        ULONG_PTR   key   = 0;
        OVERLAPPED* ov    = nullptr;
        BOOL ok = GetQueuedCompletionStatus(iocp_, &bytes, &key, &ov, 0);
        if (!ov) {
            if (!ok) {
                // No completion ready (WAIT_TIMEOUT) — done for this frame.
                break;
            }
            // ok==TRUE with null OVERLAPPED shouldn't happen for our usage.
            continue;
        }

        DWORD err = ok ? 0 : ::GetLastError();
        IoContext* ctx = CONTAINING_RECORD(ov, IoContext, ov);
        OnIoCompletion(ctx, bytes, ok, err);
    }
}

void TcpClient::OnIoCompletion(IoContext* ctx, DWORD bytes, BOOL ok, DWORD err) {
    switch (ctx->op) {
    case OpType::Connect: {
        // After ConnectEx the socket needs SO_UPDATE_CONNECT_CONTEXT to make
        // shutdown(), getpeername() and friends work properly.
        if (ok) {
            setsockopt(sock_, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, nullptr, 0);
            connected_ = true;
            delete connectCtx_;
            connectCtx_ = nullptr;
            if (onConnected) onConnected();
            PostRecv();   // arm the perpetual recv
        } else {
            lastError_ = "ConnectEx completion failed: " + std::to_string(err);
            delete connectCtx_;
            connectCtx_ = nullptr;
            DoDisconnect(true);
        }
        break;
    }
    case OpType::Recv: {
        if (!ok || bytes == 0) {
            // bytes==0 = peer closed gracefully; !ok = error.
            DoDisconnect(true);
        } else {
            if (onData) onData(reinterpret_cast<const uint8_t*>(ctx->buf.buf), (int)bytes);
            // Re-arm the perpetual recv if we are still alive.
            if (connected_) PostRecv();
        }
        break;
    }
    case OpType::Send: {
        delete sendCtx_;
        sendCtx_ = nullptr;
        if (!ok) {
            lastError_ = "WSASend completion failed: " + std::to_string(err);
            DoDisconnect(true);
        } else {
            // Drain the next queued send. TCP guarantees in-order delivery
            // because we serialize one in-flight send at a time.
            TrySubmitNextSend();
        }
        break;
    }
    }
}

void TcpClient::Disconnect() {
    DoDisconnect(false);
}

void TcpClient::DoDisconnect(bool fireCallback) {
    bool wasConnected = connected_;
    connected_ = false;

    if (sock_ != INVALID_SOCKET) {
        // Cancel anything in flight (recv, send, possibly connect). The
        // canceled ops will surface as failed completions; we drain them.
        CancelIoEx((HANDLE)sock_, nullptr);
        closesocket(sock_);
        sock_ = INVALID_SOCKET;
    }

    CleanupAllIo();
    sendQueue_.clear();
    connectExPtr_ = nullptr;

    if (fireCallback && wasConnected && onDisconnected) {
        onDisconnected();
    }
}

void TcpClient::CleanupAllIo() {
    // Drain any straggler completions for the (now-closed) socket so we
    // can free their IoContexts. After CancelIoEx + closesocket the kernel
    // posts the canceled completions; pull them all out.
    if (iocp_) {
        for (;;) {
            DWORD       bytes = 0;
            ULONG_PTR   key   = 0;
            OVERLAPPED* ov    = nullptr;
            BOOL ok = GetQueuedCompletionStatus(iocp_, &bytes, &key, &ov, 0);
            if (!ov) break;  // no more pending
            IoContext* ctx = CONTAINING_RECORD(ov, IoContext, ov);
            // Match against our three slots so we don't double-free.
            if (ctx == recvCtx_)    { delete recvCtx_;    recvCtx_    = nullptr; }
            else if (ctx == sendCtx_)    { delete sendCtx_;    sendCtx_    = nullptr; }
            else if (ctx == connectCtx_) { delete connectCtx_; connectCtx_ = nullptr; }
            else                          { delete ctx; }
            (void)ok; (void)bytes; (void)key;
        }
    }
    // Whatever wasn't surfaced as a completion (e.g. before any IO was
    // posted) gets freed directly.
    if (recvCtx_)    { delete recvCtx_;    recvCtx_    = nullptr; }
    if (sendCtx_)    { delete sendCtx_;    sendCtx_    = nullptr; }
    if (connectCtx_) { delete connectCtx_; connectCtx_ = nullptr; }
}
