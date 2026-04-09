#pragma once
#include <string>
#include <deque>
#include <cstdint>

struct ChatMessage {
    int chatType = 0;
    uint64_t senderId = 0;
    std::string senderName;
    std::string message;
    std::string targetName;
};

struct ChatHistory {
    static constexpr int kMaxMessages = 200;
    std::deque<ChatMessage> messages;
    void Add(ChatMessage msg) {
        messages.push_back(std::move(msg));
        if ((int)messages.size() > kMaxMessages) messages.pop_front();
    }
    void Clear() { messages.clear(); }
};
