#include "PacketBuilder.h"
#include "Auth.pb.h"
#include "Game.pb.h"
#include "Social.pb.h"
#include "Inventory.pb.h"
#include "Currency.pb.h"
#include <vector>

bool PacketBuilder::Send(MsgId id, const google::protobuf::Message& msg) {
    if (!client_) return false;
    int payloadSize = (int)msg.ByteSizeLong();
    uint16_t totalSize = (uint16_t)(4 + payloadSize);
    uint16_t msgId = static_cast<uint16_t>(id);
    std::vector<uint8_t> buf(totalSize);
    buf[0] = totalSize & 0xFF;
    buf[1] = (totalSize >> 8) & 0xFF;
    buf[2] = msgId & 0xFF;
    buf[3] = (msgId >> 8) & 0xFF;
    msg.SerializeToArray(buf.data() + 4, payloadSize);
    return client_->Send(buf.data(), totalSize);
}

bool PacketBuilder::SendEmpty(MsgId id) {
    if (!client_) return false;
    uint16_t totalSize = 4;
    uint16_t msgId = static_cast<uint16_t>(id);
    uint8_t buf[4];
    buf[0] = totalSize & 0xFF;
    buf[1] = (totalSize >> 8) & 0xFF;
    buf[2] = msgId & 0xFF;
    buf[3] = (msgId >> 8) & 0xFF;
    return client_->Send(buf, totalSize);
}

void PacketBuilder::SendLogin(const std::string& username, const std::string& password) {
    game::C_Login pkt;
    pkt.set_username(username);
    pkt.set_password(password);
    Send(MsgId::C_LOGIN, pkt);
}

void PacketBuilder::SendRegister(const std::string& username, const std::string& password) {
    game::C_Register pkt;
    pkt.set_username(username);
    pkt.set_password(password);
    Send(MsgId::C_REGISTER, pkt);
}

void PacketBuilder::SendCharList() {
    game::C_CharList pkt;
    Send(MsgId::C_CHAR_LIST, pkt);
}

void PacketBuilder::SendCreateChar(const std::string& name) {
    game::C_CreateChar pkt;
    pkt.set_name(name);
    Send(MsgId::C_CREATE_CHAR, pkt);
}

void PacketBuilder::SendSelectChar(uint64_t charId) {
    game::C_SelectChar pkt;
    pkt.set_char_id(charId);
    Send(MsgId::C_SELECT_CHAR, pkt);
}

void PacketBuilder::SendRoomList() {
    game::C_RoomList pkt;
    Send(MsgId::C_ROOM_LIST, pkt);
}

void PacketBuilder::SendCreateRoom(const std::string& name) {
    game::C_CreateRoom pkt;
    pkt.set_room_name(name);
    Send(MsgId::C_CREATE_ROOM, pkt);
}

void PacketBuilder::SendJoinRoom(uint32_t zoneId) {
    game::C_JoinRoom pkt;
    pkt.set_zone_id(zoneId);
    Send(MsgId::C_JOIN_ROOM, pkt);
}

void PacketBuilder::SendMove(float x, float y, float z, float rotY, float vx, float vy, float vz, int state) {
    game::C_Move pkt;
    auto* pos = pkt.mutable_position();
    pos->set_x(x); pos->set_y(y); pos->set_z(z);
    pkt.set_rotation_y(rotY);
    auto* vel = pkt.mutable_velocity();
    vel->set_x(vx); vel->set_y(vy); vel->set_z(vz);
    pkt.set_state(state);
    Send(MsgId::C_MOVE, pkt);
}

void PacketBuilder::SendAttack(uint64_t targetId, int skillId, float fx, float fy, float fz, float dx, float dy, float dz) {
    game::C_Attack pkt;
    pkt.set_target_id(targetId);
    pkt.set_skill_id(skillId);
    auto* fp = pkt.mutable_fire_pos();
    fp->set_x(fx); fp->set_y(fy); fp->set_z(fz);
    auto* fd = pkt.mutable_fire_dir();
    fd->set_x(dx); fd->set_y(dy); fd->set_z(dz);
    Send(MsgId::C_ATTACK, pkt);
}

void PacketBuilder::SendFire(float fx, float fy, float fz, float dx, float dy, float dz) {
    game::C_Fire pkt;
    auto* fp = pkt.mutable_fire_pos();
    fp->set_x(fx); fp->set_y(fy); fp->set_z(fz);
    auto* fd = pkt.mutable_fire_dir();
    fd->set_x(dx); fd->set_y(dy); fd->set_z(dz);
    Send(MsgId::C_FIRE, pkt);
}

void PacketBuilder::SendPortal(uint32_t portalId) {
    game::C_Portal pkt;
    pkt.set_portal_id(portalId);
    Send(MsgId::C_PORTAL, pkt);
}

void PacketBuilder::SendPickup(uint64_t groundId) {
    game::C_Pickup pkt;
    pkt.set_ground_id(groundId);
    Send(MsgId::C_PICKUP, pkt);
}

void PacketBuilder::SendChat(int chatType, const std::string& message, const std::string& target) {
    game::C_Chat pkt;
    pkt.set_chat_type(static_cast<game::ChatType>(chatType));
    pkt.set_message(message);
    if (!target.empty())
        pkt.set_target_name(target);
    Send(MsgId::C_CHAT, pkt);
}

void PacketBuilder::SendCreateParty() {
    game::C_CreateParty pkt;
    Send(MsgId::C_CREATE_PARTY, pkt);
}

void PacketBuilder::SendJoinParty(uint64_t partyId) {
    game::C_JoinParty pkt;
    pkt.set_party_id(partyId);
    Send(MsgId::C_JOIN_PARTY, pkt);
}

void PacketBuilder::SendLeaveParty() {
    game::C_LeaveParty pkt;
    Send(MsgId::C_LEAVE_PARTY, pkt);
}

void PacketBuilder::SendUseItem(int64_t instanceId) {
    game::C_UseItem pkt;
    pkt.set_instance_id(instanceId);
    Send(MsgId::C_USE_ITEM, pkt);
}

void PacketBuilder::SendDropItem(int64_t instanceId, int qty) {
    game::C_DropItem pkt;
    pkt.set_instance_id(instanceId);
    pkt.set_quantity(qty);
    Send(MsgId::C_DROP_ITEM, pkt);
}

void PacketBuilder::SendMoveItem(int64_t instanceId, int newSlot) {
    game::C_MoveItem pkt;
    pkt.set_instance_id(instanceId);
    pkt.set_new_slot(newSlot);
    Send(MsgId::C_MOVE_ITEM, pkt);
}

void PacketBuilder::SendPurchase(int itemDefId, int qty) {
    game::C_Purchase pkt;
    pkt.set_item_def_id(itemDefId);
    pkt.set_quantity(qty);
    Send(MsgId::C_PURCHASE, pkt);
}
