#pragma once
// Stand-in targets so automatic shooting has something to hit.
// Replace this with the real enemy component later: keep filling
// weapons::ShootTarget and pass that vector to AutoShooter.

#include "weapons/auto_shooter.hpp"

#include <cmath>
#include <vector>

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
    }

    // Body, hat brim, and a 3-hit health bar. White body means hitFlash is on.
    void Draw() const {
        for (const weapons::ShootTarget& target : targets) {
            if (!target.alive) continue;
            const Color body = target.hitFlash > 0.0f ? Color{236, 224, 196, 255} : Color{122, 52, 40, 255};
            DrawCircleV(target.position, target.radius, body);
            DrawRectangle(static_cast<int>(target.position.x - 11.0f), static_cast<int>(target.position.y - target.radius - 5.0f), 22, 5, {62, 36, 28, 255});
            const int bar = static_cast<int>(22.0f * (static_cast<float>(target.hp) / 3.0f));
            DrawRectangle(static_cast<int>(target.position.x - 11.0f), static_cast<int>(target.position.y - target.radius - 12.0f), 22, 3, {40, 24, 20, 255});
            DrawRectangle(static_cast<int>(target.position.x - 11.0f), static_cast<int>(target.position.y - target.radius - 12.0f), bar, 3, {118, 168, 72, 255});
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
        targets[i].hp = 3;
        targets[i].alive = true;
        targets[i].hitFlash = 0.0f;
        speed_[i] = 62.0f + static_cast<float>(i % 4) * 16.0f;
    }
};

}  // namespace enemies
