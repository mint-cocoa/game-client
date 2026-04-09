#include "Network/NetworkContext.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include "Network/PacketHandler.h"
#include "Network/TcpClient.h"

bool NetworkContext::IsConnected() const { return tcp && tcp->IsConnected(); }
bool NetworkContext::Connect(const std::string& host, uint16_t port) { return tcp->Connect(host.c_str(), port); }
void NetworkContext::Disconnect() { tcp->Disconnect(); }
