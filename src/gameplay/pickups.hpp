#pragma once
// Items enemies can drop that the player walks over to collect (health, power-ups, etc.).
//
// Usage (see main.cpp):
//   pickups::PickupSystem pickups;
//   pickups.SpawnFromKills(bandits.targets);   // after the shooter has updated
//   pickups.Update(player, deltaTime);         // collects anything the player touches
//   pickups.Draw();                            // inside BeginMode2D
//
// To add a new pickup type:
//   1. Add it to the Type enum.
//   2. Add a drop chance to kDropTable.
//   3. Say what it does in ApplyEffect() and how it looks in DrawPickup().

#include "raylib.h"
#include "../weapons/auto_shooter.hpp"
#include "../player/Player.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

namespace pickups {

enum class Type {
    Health, // red cross, restores HP
};

// One item lying on the ground.
struct Pickup {
    Type type = Type::Health;
    Vector2 position{0.0f, 0.0f};
    float radius = 12.0f;  // collection radius
    int amount = 0;        // what the effect gives (HP for Health)
    float age = 0.0f;      // seconds on the ground, used for the bob animation
    bool collected = false;
};

// Chance for a pickup to drop when an enemy dies: 1 in `oneIn`.
struct DropChance {
    Type type;
    int oneIn;
    int amount;
};

// Every pickup that can drop from an enemy. Each kill rolls every entry.
const DropChance kDropTable[] = {
    {Type::Health, 50, 30}, // 1/50 chance, +30 HP
};

// What happens when the player collects a pickup.
inline void ApplyEffect(const Pickup& pickup, Player& player) {
    switch (pickup.type) {
        case Type::Health:
            // Heal, but never above max HP.
            player.health = std::min(player.maxHealth, player.health + pickup.amount);
            break;
    }
}

// How each pickup looks. `bob` is a small up/down offset so items stand out.
inline void DrawPickup(const Pickup& pickup, float bob) {
    const Vector2 p{pickup.position.x, pickup.position.y + bob};
    switch (pickup.type) {
        case Type::Health: {
            // White disc behind a red cross.
            DrawCircleV(p, 12.0f, Color{245, 240, 230, 255});
            DrawRectangle(static_cast<int>(p.x - 3.0f), static_cast<int>(p.y - 8.0f), 6, 16, RED);
            DrawRectangle(static_cast<int>(p.x - 8.0f), static_cast<int>(p.y - 3.0f), 16, 6, RED);
            break;
        }
    }
}

class PickupSystem {
public:
    std::vector<Pickup> items;

    // Put a pickup on the ground at `position`.
    void Spawn(Type type, Vector2 position, int amount) {
        Pickup pickup;
        pickup.type = type;
        pickup.position = position;
        pickup.amount = amount;
        items.push_back(pickup);
    }

    // Roll the drop table for every enemy that died this frame.
    // Dead targets are still marked !alive at this point; the enemy code
    // respawns them at the start of the next frame (same as the EXP orbs).
    void SpawnFromKills(const std::vector<weapons::ShootTarget>& targets) {
        for (const weapons::ShootTarget& target : targets) {
            if (target.alive) continue;
            for (const DropChance& drop : kDropTable) {
                if (std::rand() % drop.oneIn == 0) {
                    // Nudge it off the EXP orb so both are visible.
                    Spawn(drop.type, {target.position.x + 14.0f, target.position.y}, drop.amount);
                }
            }
        }
    }

    // Collect anything the player is touching, then remove collected items.
    void Update(Player& player, float deltaTime) {
        for (Pickup& pickup : items) {
            pickup.age += deltaTime;

            const float dx = pickup.position.x - player.position.x;
            const float dy = pickup.position.y - player.position.y;
            const float reach = player.radius + pickup.radius;
            if (dx * dx + dy * dy <= reach * reach) {
                ApplyEffect(pickup, player);
                pickup.collected = true;
            }
        }
        items.erase(std::remove_if(items.begin(), items.end(),
                                   [](const Pickup& pickup) { return pickup.collected; }),
                    items.end());
    }

    // World-space drawing. Call inside BeginMode2D.
    void Draw() const {
        for (const Pickup& pickup : items) {
            DrawPickup(pickup, std::sin(pickup.age * 4.0f) * 2.0f);
        }
    }
};

}  // namespace pickups
