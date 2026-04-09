#pragma once
#include "Data/PlayerData.h"
#include "Data/InventoryData.h"
#include "Data/CurrencyData.h"
#include "Data/ChatHistory.h"
#include "Common.pb.h"
#include <string>

struct SessionState {
    PlayerData    localPlayer;
    InventoryData inventory;
    CurrencyData  currency;
    ChatHistory   chatHistory;
    std::string   playerName;
    uint64_t      playerId         = 0;
    bool          isLocalPlayerDead = false;

    game::MapData mapData;
    bool          hasMapData        = false;
    bool          pendingEnterGame  = false;

    void FillLocalPlayerFromProto(const game::PlayerInfo& info)
    {
        localPlayer.playerId  = info.player_id();
        localPlayer.name      = info.name();
        localPlayer.position  = { info.position().x(), info.position().y(), info.position().z() };
        localPlayer.rotationY = info.rotation_y();
        localPlayer.hp        = info.hp();
        localPlayer.maxHp     = (info.max_hp() > 1) ? info.max_hp() : 100;
        localPlayer.level     = info.level();
        localPlayer.zoneId    = info.zone_id();
        localPlayer.partyId   = info.party_id();
    }

    void Reset()
    {
        localPlayer       = {};
        playerId          = 0;
        playerName.clear();
        hasMapData        = false;
        mapData.Clear();
        isLocalPlayerDead = false;
        pendingEnterGame  = false;
        inventory.Clear();
        currency          = {};
        chatHistory.Clear();
    }
};
