#pragma once
// Something a weapon is allowed to hit. Enemy code owns these. The shooter
// only reads and writes the fields below. Kept out of the weapon file so
// the player, pickups, and enemies do not have to include the guns.

#include "raylib.h"

namespace weapons {

struct ShootTarget {
    Vector2 position{0.0f, 0.0f};
    float radius = 14.0f;
    int hp = 100;
    int maxHp = 100; // full health, used to size the health bar
    bool alive = true;
    float hitFlash = 0.0f; // seconds left on the white hit flash
};

}  // namespace weapons
