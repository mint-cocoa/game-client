#pragma once
#include <cstdint>
#include <array>

struct ItemInstance {
    int64_t instanceId = 0;
    int32_t itemDefId = 0;
    int32_t slot = -1;
    int32_t quantity = 0;
    int32_t durability = 0;
    bool empty() const { return instanceId == 0; }
};

struct InventoryData {
    static constexpr int kMaxSlots = 100;
    std::array<ItemInstance, kMaxSlots> slots{};
    void Clear() { slots.fill({}); }
    ItemInstance* GetSlot(int s) { return (s >= 0 && s < kMaxSlots) ? &slots[s] : nullptr; }
};
