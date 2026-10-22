#pragma once
// Stand-in targets so automatic shooting has something to hit.
// Replace this with the real enemy component later: keep filling
// weapons::ShootTarget and pass that vector to AutoShooter.

#include "core/target.hpp"
#include "raylib.h"

#include <cmath>
#include <vector>
#include <algorithm>

namespace enemies {

// Ten brown stand-ins that walk at the player and respawn off-screen when
// they die. Delete this class when the real enemy list exists. The shooter
// only reads and writes `targets`.
class PracticeBandits {
public:
    std::vector<weapons::ShootTarget> targets; // hand this to AutoShooter::Update

    explicit PracticeBandits(int count = 10) {
        targets.resize(count);
        speed_.resize(count);
        for (int i = 0; i < count; ++i) Place(i, {400.0f, 300.0f}, false);
    }

    // Walk toward the player, but stop just outside the blue circle so they
    // bunch up and get shot. A dead slot respawns farther out than the camera.
    void Update(Vector2 player, float dt) {
        for (int i = 0; i < static_cast<int>(targets.size()); ++i) {
            if (!targets[i].alive) {
                ++waves_;
                Place(i, player, true);
                continue;
            }
            const float dx = player.x - targets[i].position.x;
            const float dy = player.y - targets[i].position.y;
            const float len = std::sqrt(dx * dx + dy * dy);
            if (len > 36.0f) {
                targets[i].position.x += (dx / len) * speed_[i] * dt;
                targets[i].position.y += (dy / len) * speed_[i] * dt;
            }
        }
        ResolveCollisions();
    }

    // Body, hat brim, and a 3-hit health bar. White body means hitFlash is on.
// Body, hat brim, and a 3-hit health bar.
void Draw() const {
    for (const weapons::ShootTarget& target : targets) {
        if (!target.alive) continue;

        const Color body = target.hitFlash > 0.0f
            ? Color{236, 224, 196, 255}
            : Color{122, 52, 40, 255};

        DrawCircleV(target.position, target.radius, body);

        // Enemy body

// Cowboy hat crown (bucket/top part)
DrawRectangle(
    static_cast<int>(target.position.x - 9.0f),
    static_cast<int>(target.position.y - target.radius - 9.0f),
    18,
    9,
    {75, 43, 29, 255}
);

// Rounded top of the crown
DrawEllipse(
    static_cast<int>(target.position.x),
    static_cast<int>(target.position.y - target.radius - 9.0f),
    9.0f,
    3.0f,
    {75, 43, 29, 255}
);

// Cowboy hat brim (draw in front of the crown)
DrawEllipse(
    static_cast<int>(target.position.x),
    static_cast<int>(target.position.y - target.radius),
    17.0f,
    4.0f,
    {45, 30, 24, 255}
);

        // Health bar dimensions and position
        const float barWidth = 22.0f;
        const float barHeight = 4.0f;
        const float barX = target.position.x - barWidth / 2.0f;
        const float barY = target.position.y - target.radius - 10.0f;

        // Dark background behind the health bar
        DrawRectangle(
            static_cast<int>(barX),
            static_cast<int>(barY),
            static_cast<int>(barWidth),
            static_cast<int>(barHeight),
            {40, 24, 20, 255}
        );

        // Green health fill, clamped to the bar width
        const float healthRatio = std::max(
            0.0f,
            std::min(1.0f, static_cast<float>(target.hp) / static_cast<float>(target.maxHp))
        );

        DrawRectangle(
            static_cast<int>(barX),
            static_cast<int>(barY),
            static_cast<int>(barWidth * healthRatio),
            static_cast<int>(barHeight),
            {118, 168, 72, 255}
        );
    }
}

private:
    std::vector<float> speed_;
    int waves_ = 0;

    // First placement is inside oatmeal range so the gun fires immediately.
    // Respawn distance is past the edge of the 800x600 camera.
    void Place(int i, Vector2 player, bool respawn) {
        const float angle = 0.65f * static_cast<float>(i) + static_cast<float>(waves_) * 0.9f;
        const float dist = respawn ? 640.0f + static_cast<float>(i % 3) * 50.0f : 280.0f + static_cast<float>(i % 5) * 70.0f;
        targets[i].position = {player.x + std::cos(angle) * dist, player.y + std::sin(angle) * dist};
        targets[i].radius = 14.0f;
        targets[i].hp = 100;
        targets[i].maxHp = 100;
        targets[i].alive = true;
        targets[i].hitFlash = 0.0f;
        speed_[i] = 62.0f + static_cast<float>(i % 4) * 16.0f;
    }

    void ResolveCollisions() {//allow enemies to collide with eachother
    for (int i = 0; i < static_cast<int>(targets.size()); ++i) {
        if (!targets[i].alive) {
            continue;
        }

        for (int j = i + 1; j < static_cast<int>(targets.size()); ++j) {
            if (!targets[j].alive) {
                continue;
            }

            float dx = targets[j].position.x - targets[i].position.x;
            float dy = targets[j].position.y - targets[i].position.y;

            float distanceSquared = dx * dx + dy * dy;
            float minDistance = targets[i].radius + targets[j].radius;

            if (distanceSquared >= minDistance * minDistance) {
                continue;
            }

            float distance;

            if (distanceSquared > 0.0001f) {
                distance = std::sqrt(distanceSquared);
            } else {
                // Avoid division by zero if enemies occupy the same position
                dx = 1.0f;
                dy = 0.0f;
                distance = 1.0f;
            }

            // Calculate the overlap between the two circles
            float overlap = minDistance - distance;

            // Move each enemy half the overlap in opposite directions
            float pushX = (dx / distance) * (overlap / 2.0f);
            float pushY = (dy / distance) * (overlap / 2.0f);

            targets[i].position.x -= pushX;
            targets[i].position.y -= pushY;

            targets[j].position.x += pushX;
            targets[j].position.y += pushY;
        }
    }
}
};

}  // namespace enemies
