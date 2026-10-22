#pragma once

#include "raylib.h"
#include "weapons/auto_shooter.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace progression {

// Experience orb dropped by a defeated enemy.
struct ExpOrb {
    Vector2 position;
    float radius = 8.0f;
    int value = 1;
};

// type: 0 = unlock Charge, 1 = Revolver damage, 2 = attack speed/charge time, 3 = Revolver pierce, 4 = Charge projectile size
struct UpgradeOption {
    int type = 0;
    std::string weaponName;
    std::string title;
    std::string detail;
    std::string category;
};

class LevelUpSystem {
public:
    // Drop one orb where an enemy died. Game::Wire calls this on EnemyKilled.
    void OnEnemyKilled(Vector2 position);

    // Pull nearby orbs into the player. Does not look at the enemy list.
    void UpdateExperience(Vector2 playerPosition, float playerRadius);

    void StartLevelUpIfReady(const weapons::AutoShooter& shooter);
    void HandleInput(weapons::AutoShooter& shooter);
    void DrawOrbs() const;
    void DrawHud() const;
    void DrawMenu() const;

    bool IsActive() const { return levelUpActive; }
    int Level() const { return level; }
    int Experience() const { return exp; }
    int ExperienceToNextLevel() const { return expToNextLevel; }

private:
    int level = 1;
    int exp = 0;
    int expToNextLevel = 5;
    bool levelUpActive = false;
    std::vector<ExpOrb> expOrbs;
    std::vector<UpgradeOption> upgradeOptions;
};

} // namespace progression

namespace progression {
namespace {

std::string FormatStat(float value) {
    std::ostringstream output;
    output << std::fixed << std::setprecision(2) << value;
    return output.str();
}

// Draw text inside a fixed-width card, wrapping at word boundaries.
void DrawWrappedText(const std::string& text, int x, int y, int fontSize,
                     int maxWidth, Color color, int maxLines) {
    std::istringstream words(text);
    std::string word;
    std::string line;
    int lineCount = 0;
    const int lineHeight = fontSize + 4;

    while (words >> word) {
        const std::string candidate = line.empty() ? word : line + " " + word;
        if (!line.empty() && MeasureText(candidate.c_str(), fontSize) > maxWidth) {
            DrawText(line.c_str(), x, y + lineCount * lineHeight, fontSize, color);
            ++lineCount;
            if (lineCount >= maxLines) return;
            line = word;
        } else {
            line = candidate;
        }
    }

    if (!line.empty() && lineCount < maxLines) {
        DrawText(line.c_str(), x, y + lineCount * lineHeight, fontSize, color);
    }
}

weapons::Weapon MakeChargeWeapon() {
    weapons::Weapon charge;
    charge.name = "Charge";
    charge.cooldown = 0.0f;
    charge.chargeTime = 1.35f;
    charge.range = 480.0f;
    charge.speed = 640.0f;
    charge.shotRadius = 12.0f;
    charge.damage = 100;
    charge.life = 0.95f;
    charge.pierce = 8;
    charge.color = {214, 92, 42, 255};
    return charge;
}

bool IsRevolver(const weapons::Weapon& weapon) {
    // Accept the old name too, in case only this header was copied into the project.
    return std::string(weapon.name) == "Revolver" || std::string(weapon.name) == "Oatmeal";
}

bool HasWeapon(const weapons::AutoShooter& shooter, const char* name) {
    for (const weapons::Weapon& weapon : shooter.weapons) {
        if (std::string(weapon.name) == name) return true;
    }
    return false;
}

std::vector<UpgradeOption> BuildUpgradeOptions(const weapons::AutoShooter& shooter) {
    std::vector<UpgradeOption> options;
    std::vector<UpgradeOption> availableUpgrades;
    const bool chargeUnlocked = HasWeapon(shooter, "Charge");

    if (!chargeUnlocked) {
        // Charge unlock is one possible card, not a guaranteed choice.
        availableUpgrades.push_back({0, "Charge", "Unlock Charge", "Unlocks the Charge weapon", "NEW WEAPON"});
    }

    for (const weapons::Weapon& weapon : shooter.weapons) {
        if (IsRevolver(weapon)) {
            availableUpgrades.push_back({1, weapon.name, "Revolver Damage", "+34 damage (" + std::to_string(weapon.damage) + " -> " + std::to_string(weapon.damage + 34) + ")", "WEAPON UPGRADE"});

            const float newCooldown = weapon.cooldown * 0.85f;
            availableUpgrades.push_back({2, weapon.name, "Revolver Attack Speed", "Cooldown: " + FormatStat(weapon.cooldown) + "s -> " + FormatStat(newCooldown) + "s", "WEAPON UPGRADE"});

            if (weapon.pierce < 3) {
                availableUpgrades.push_back({3, weapon.name, "Revolver Pierce", "Enemies hit per shot: " + std::to_string(weapon.pierce) + " -> " + std::to_string(weapon.pierce + 1) + " (max 3)", "WEAPON UPGRADE"});
            }
        } else if (std::string(weapon.name) == "Charge") {
            const float newChargeTime = weapon.chargeTime * 0.85f;
            availableUpgrades.push_back({2, weapon.name, "Charge Time", "Charge time: " + FormatStat(weapon.chargeTime) + "s -> " + FormatStat(newChargeTime) + "s", "WEAPON UPGRADE"});

            const float newShotRadius = weapon.shotRadius + 2.0f;
            availableUpgrades.push_back({4, weapon.name, "Charge Size", "Projectile size: " + FormatStat(weapon.shotRadius) + " -> " + FormatStat(newShotRadius), "WEAPON UPGRADE"});
        }
    }

    // Shuffle available upgrades so the choices vary between level-ups.
    for (int i = static_cast<int>(availableUpgrades.size()) - 1; i > 0; --i) {
        const int j = std::rand() % (i + 1);
        std::swap(availableUpgrades[i], availableUpgrades[j]);
    }

    // Always offer exactly three distinct choices, randomly selected from
    // all currently available weapon unlocks and upgrades.
    const std::size_t desiredChoices = 3;
    for (std::size_t i = 0; i < desiredChoices && i < availableUpgrades.size(); ++i) {
        options.push_back(availableUpgrades[i]);
    }
    return options;
}

void ApplyUpgrade(const UpgradeOption& option, weapons::AutoShooter& shooter) {
    if (option.type == 0) {
        if (!HasWeapon(shooter, "Charge")) shooter.weapons.push_back(MakeChargeWeapon());
        return;
    }

    for (weapons::Weapon& weapon : shooter.weapons) {
        if (option.weaponName != weapon.name) continue;

        if (option.type == 1 && IsRevolver(weapon)) {
            weapon.damage += 34;
        } else if (option.type == 2) {
            if (IsRevolver(weapon)) {
                weapon.cooldown *= 0.85f;
            } else if (std::string(weapon.name) == "Charge") {
                weapon.chargeTime *= 0.85f;
            }
        } else if (option.type == 3 && IsRevolver(weapon)) {
            weapon.pierce = std::min(3, weapon.pierce + 1);
        } else if (option.type == 4 && std::string(weapon.name) == "Charge") {
            weapon.shotRadius += 2.0f;
        }
        return;
    }
}

} // namespace

inline void LevelUpSystem::OnEnemyKilled(Vector2 position) {
    expOrbs.push_back({position, 8.0f, 1});
}

inline void LevelUpSystem::UpdateExperience(Vector2 playerPosition, float playerRadius) {
    // Move and collect experience orbs. Orbs were added by OnEnemyKilled.
    for (ExpOrb& orb : expOrbs) {
        if (orb.radius > 0.0f) {
            const float dx = orb.position.x - playerPosition.x;
            const float dy = orb.position.y - playerPosition.y;
            if (std::sqrt(dx * dx + dy * dy) <= playerRadius + orb.radius) {
                exp += orb.value;
                orb.radius = 0.0f;
            }
        }
    }
    expOrbs.erase(
        std::remove_if(expOrbs.begin(), expOrbs.end(),
                       [](const ExpOrb& orb) { return orb.radius <= 0.0f; }),
        expOrbs.end()
    );
}

inline void LevelUpSystem::StartLevelUpIfReady(const weapons::AutoShooter& shooter) {
    if (!levelUpActive && exp >= expToNextLevel) {
        exp -= expToNextLevel;
        ++level;
        expToNextLevel += 3;
        upgradeOptions = BuildUpgradeOptions(shooter);
        levelUpActive = true;
    }
}

inline void LevelUpSystem::HandleInput(weapons::AutoShooter& shooter) {
    if (!levelUpActive) return;

    int selectedOption = -1;
    if (IsKeyPressed(KEY_ONE)) selectedOption = 0;
    if (IsKeyPressed(KEY_TWO)) selectedOption = 1;
    if (IsKeyPressed(KEY_THREE)) selectedOption = 2;

    const Rectangle cards[3] = {
        {40.0f, 160.0f, 220.0f, 300.0f},
        {290.0f, 160.0f, 220.0f, 300.0f},
        {540.0f, 160.0f, 220.0f, 300.0f}
    };
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        const Vector2 mouse = GetMousePosition();
        for (int i = 0; i < static_cast<int>(upgradeOptions.size()) && i < 3; ++i) {
            if (CheckCollisionPointRec(mouse, cards[i])) selectedOption = i;
        }
    }

    if (selectedOption >= 0 && selectedOption < static_cast<int>(upgradeOptions.size())) {
        ApplyUpgrade(upgradeOptions[selectedOption], shooter);
        levelUpActive = false;
        StartLevelUpIfReady(shooter);
    }
}

inline void LevelUpSystem::DrawOrbs() const {
    for (const ExpOrb& orb : expOrbs) {
        DrawCircleV(orb.position, orb.radius, PURPLE);
        DrawCircleV(orb.position, orb.radius * 0.45f, SKYBLUE);
    }
}

inline void LevelUpSystem::DrawHud() const {
    DrawText(TextFormat("Level %d  EXP %d/%d", level, exp, expToNextLevel), 10, 57, 16, WHITE);
    DrawRectangle(190, 60, 220, 12, DARKGRAY);
    DrawRectangle(190, 60, static_cast<int>(220.0f * static_cast<float>(exp) / expToNextLevel), 12, SKYBLUE);
    DrawRectangleLines(190, 60, 220, 12, WHITE);
}

inline void LevelUpSystem::DrawMenu() const {
    if (!levelUpActive) return;

    DrawRectangle(0, 0, 800, 600, Fade(BLACK, 0.82f));
    DrawText("LEVEL UP!", 300, 55, 42, GOLD);
    DrawText("Choose one upgrade - press 1, 2, or 3", 190, 112, 20, RAYWHITE);

    const Rectangle cards[3] = {
        {40.0f, 160.0f, 220.0f, 300.0f},
        {290.0f, 160.0f, 220.0f, 300.0f},
        {540.0f, 160.0f, 220.0f, 300.0f}
    };
    for (int i = 0; i < 3 && i < static_cast<int>(upgradeOptions.size()); ++i) {
        const UpgradeOption& option = upgradeOptions[i];
        DrawRectangleRec(cards[i], {38, 35, 31, 255});
        DrawRectangleLinesEx(cards[i], 3.0f, option.type == 0 ? GOLD : LIGHTGRAY);
        const int textX = static_cast<int>(cards[i].x + 10.0f);
        const int textWidth = static_cast<int>(cards[i].width - 20.0f);
        DrawText(TextFormat("%d", i + 1), static_cast<int>(cards[i].x + 95.0f), 174, 28, GOLD);
        DrawWrappedText(option.category, textX, 215, 12, textWidth,
                        option.type == 0 ? GOLD : SKYBLUE, 2);
        DrawWrappedText(option.weaponName, textX, 246, 18, textWidth, RAYWHITE, 2);
        DrawWrappedText(option.title, textX, 284, 15, textWidth, WHITE, 2);
        DrawWrappedText(option.detail, textX, 324, 13, textWidth, GREEN, 5);
        DrawText("Click to choose", textX, 425, 13, LIGHTGRAY);
    }
}

} // namespace progression

// Public functions used by main.cpp. The class itself stays private to this file.
namespace progression {

inline LevelUpSystem* CreateLevelUpSystem() {
    return new LevelUpSystem();
}

inline void DestroyLevelUpSystem(LevelUpSystem* levelUp) {
    delete levelUp;
}

inline bool IsActive(const LevelUpSystem* levelUp) {
    return levelUp->IsActive();
}

inline void HandleInput(LevelUpSystem* levelUp, weapons::AutoShooter& shooter) {
    levelUp->HandleInput(shooter);
}

inline void UpdateExperience(LevelUpSystem* levelUp, Vector2 playerPosition, float playerRadius) {
    levelUp->UpdateExperience(playerPosition, playerRadius);
}

inline void StartLevelUpIfReady(LevelUpSystem* levelUp, const weapons::AutoShooter& shooter) {
    levelUp->StartLevelUpIfReady(shooter);
}

inline void DrawOrbs(const LevelUpSystem* levelUp) {
    levelUp->DrawOrbs();
}

inline void DrawHud(const LevelUpSystem* levelUp) {
    levelUp->DrawHud();
}

inline void DrawMenu(const LevelUpSystem* levelUp) {
    levelUp->DrawMenu();
}

} // namespace progression
