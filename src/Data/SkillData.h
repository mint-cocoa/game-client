#pragma once
#include <string>
#include <vector>
#include <cstdint>

struct SkillInfo {
    uint32_t id = 0;
    std::string name;
    int damage = 0;
    float range = 0;
    float cooldownSec = 0;
    bool isSelfTarget = false;
};

struct SkillData {
    std::vector<SkillInfo> skills;
    std::vector<float> cooldownTimers;

    void ApplyFromServer(const std::vector<SkillInfo>& serverSkills) {
        skills = serverSkills;
        cooldownTimers.assign(skills.size(), 0.0f);
    }

    void UpdateCooldowns(float dt) {
        for (auto& t : cooldownTimers)
            if (t > 0) t -= dt;
    }

    bool TryUseSkill(int index) {
        if (index < 0 || index >= (int)skills.size()) return false;
        if (cooldownTimers[index] > 0) return false;
        cooldownTimers[index] = skills[index].cooldownSec;
        return true;
    }

    float GetCooldownRatio(int index) const {
        if (index < 0 || index >= (int)skills.size()) return 0;
        if (skills[index].cooldownSec <= 0) return 0;
        return cooldownTimers[index] / skills[index].cooldownSec;
    }
};
