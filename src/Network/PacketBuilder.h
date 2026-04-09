#pragma once
#include "MsgId.h"
#include "TcpClient.h"
#include <google/protobuf/message.h>
#include <string>

class PacketBuilder {
public:
    void Init(TcpClient* client) { client_ = client; }
    bool Send(MsgId id, const google::protobuf::Message& msg);
    bool SendEmpty(MsgId id);

    void SendLogin(const std::string& username, const std::string& password);
    void SendRegister(const std::string& username, const std::string& password);
    void SendCharList();
    void SendCreateChar(const std::string& name);
    void SendSelectChar(uint64_t charId);
    void SendRoomList();
    void SendCreateRoom(const std::string& name);
    void SendJoinRoom(uint32_t zoneId);
    void SendMove(float x, float y, float z, float rotY, float vx, float vy, float vz, int state);
    void SendAttack(uint64_t targetId, int skillId, float fx = 0, float fy = 0, float fz = 0, float dx = 0, float dy = 0, float dz = 0);
    void SendFire(float fx, float fy, float fz, float dx, float dy, float dz);
    void SendPortal(uint32_t portalId);
    void SendPickup(uint64_t groundId);
    void SendChat(int chatType, const std::string& message, const std::string& target = "");
    void SendCreateParty();
    void SendJoinParty(uint64_t partyId);
    void SendLeaveParty();
    void SendUseItem(int64_t instanceId);
    void SendDropItem(int64_t instanceId, int qty);
    void SendMoveItem(int64_t instanceId, int newSlot);
    void SendPurchase(int itemDefId, int qty);

private:
    TcpClient* client_ = nullptr;
};
