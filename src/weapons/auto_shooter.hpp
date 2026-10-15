#pragma once
// Automatic shooting. Weapons fire on their own and aim at the nearest living
// target in range. There is no shoot button.
//
// The enemy code owns the targets. Fill a vector<ShootTarget> and pass it to
// AutoShooter::Update along with the player's position. Include this header,
// construct the shooter after InitWindow(), and call Draw() inside BeginMode2D.

#include "raylib.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace weapons {

struct ShootTarget {
    Vector2 position{0.0f, 0.0f};
    float radius = 14.0f;
    int hp = 3;
    bool alive = true;
    float hitFlash = 0.0f;
};

struct Projectile {
    Vector2 position{0.0f, 0.0f};
    Vector2 velocity{0.0f, 0.0f};
    float radius = 6.0f;
    int damage = 1;
    float life = 1.2f;
    bool alive = true;
    bool pierce = false;
    Color color{214, 186, 132, 255};
    int hitIndex[8]{};
    int hitCount = 0;
};

struct Weapon {
    const char* name = "Oatmeal";
    float cooldown = 0.45f;    // seconds between shots when chargeTime is 0
    float chargeTime = 0.0f;   // seconds to fully charge; 0 means fire on cooldown
    float range = 360.0f;
    float speed = 460.0f;
    float shotRadius = 7.0f;
    int damage = 1;
    float life = 1.2f;
    bool pierce = false;
    Color color{214, 186, 132, 255};
    float timer = 0.0f;        // cooldown remaining, or charge built up
    std::vector<Projectile> shots;
};

struct Spark {
    Vector2 position{0.0f, 0.0f};
    float life = 0.0f;
    Color color{255, 255, 255, 255};
};

inline float Len(Vector2 v) { return std::sqrt(v.x * v.x + v.y * v.y); }

inline float Dist(Vector2 a, Vector2 b) { return Len({a.x - b.x, a.y - b.y}); }

// Closest distance from point p to the segment ab, and how far along ab that is (0..1).
inline float DistToSegment(Vector2 p, Vector2 a, Vector2 b, float* outT) {
    const float abx = b.x - a.x;
    const float aby = b.y - a.y;
    const float len2 = abx * abx + aby * aby;
    float t = 0.0f;
    if (len2 > 0.0001f) {
        t = ((p.x - a.x) * abx + (p.y - a.y) * aby) / len2;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
    }
    if (outT) *outT = t;
    const Vector2 closest{a.x + abx * t, a.y + aby * t};
    return Dist(p, closest);
}

inline bool AlreadyHit(const Projectile& shot, int index) {
    for (int i = 0; i < shot.hitCount; ++i) {
        if (shot.hitIndex[i] == index) return true;
    }
    return false;
}

inline int NearestInRange(Vector2 origin, float range, const std::vector<ShootTarget>& targets) {
    int best = -1;
    float bestDist = range;
    for (int i = 0; i < static_cast<int>(targets.size()); ++i) {
        if (!targets[i].alive) continue;
        const float gap = Dist(origin, targets[i].position) - targets[i].radius;
        if (gap <= bestDist) {
            bestDist = gap;
            best = i;
        }
    }
    return best;
}

inline void ApplyHit(Projectile& shot, ShootTarget& target, int index, std::vector<Spark>& sparks) {
    target.hp -= shot.damage;
    target.hitFlash = 0.12f;
    if (target.hp <= 0) {
        target.hp = 0;
        target.alive = false;
    }
    if (shot.hitCount < 8) shot.hitIndex[shot.hitCount++] = index;
    if (sparks.size() < 64) sparks.push_back(Spark{target.position, 0.16f, shot.color});
    if (!shot.pierce || shot.hitCount >= 8) shot.alive = false;
}

inline void FireAt(Weapon& weapon, Vector2 origin, Vector2 aim) {
    if (weapon.shots.size() >= 48) return;
    const float dx = aim.x - origin.x;
    const float dy = aim.y - origin.y;
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len < 0.001f) return;

    Projectile shot;
    shot.position = origin;
    shot.velocity = {weapon.speed * dx / len, weapon.speed * dy / len};
    shot.radius = weapon.shotRadius;
    shot.damage = weapon.damage;
    shot.life = weapon.life;
    shot.alive = true;
    shot.pierce = weapon.pierce;
    shot.color = weapon.color;
    shot.hitCount = 0;
    weapon.shots.push_back(shot);
}

inline void StepShots(Weapon& weapon, float dt, std::vector<ShootTarget>& targets, std::vector<Spark>& sparks) {
    for (Projectile& shot : weapon.shots) {
        if (!shot.alive) continue;
        const Vector2 from = shot.position;
        shot.position.x += shot.velocity.x * dt;
        shot.position.y += shot.velocity.y * dt;
        shot.life -= dt;
        if (shot.life <= 0.0f) {
            shot.alive = false;
            continue;
        }

        if (shot.pierce) {
            for (int i = 0; i < static_cast<int>(targets.size()); ++i) {
                if (!shot.alive) break;
                if (!targets[i].alive || AlreadyHit(shot, i)) continue;
                if (DistToSegment(targets[i].position, from, shot.position, nullptr) <= shot.radius + targets[i].radius) {
                    ApplyHit(shot, targets[i], i, sparks);
                }
            }
        } else {
            int best = -1;
            float bestT = 2.0f;
            for (int i = 0; i < static_cast<int>(targets.size()); ++i) {
                if (!targets[i].alive || AlreadyHit(shot, i)) continue;
                float t = 0.0f;
                const float gap = DistToSegment(targets[i].position, from, shot.position, &t);
                if (gap <= shot.radius + targets[i].radius && t < bestT) {
                    bestT = t;
                    best = i;
                }
            }
            if (best >= 0) ApplyHit(shot, targets[best], best, sparks);
        }
    }

    weapon.shots.erase(std::remove_if(weapon.shots.begin(), weapon.shots.end(),
                                      [](const Projectile& shot) { return !shot.alive; }),
                       weapon.shots.end());
}

inline void UpdateWeapon(Weapon& weapon, Vector2 origin, float dt, std::vector<ShootTarget>& targets, std::vector<Spark>& sparks) {
    const int aim = NearestInRange(origin, weapon.range, targets);
    if (weapon.chargeTime > 0.0f) {
        if (aim >= 0) weapon.timer += dt;
        else weapon.timer = std::max(0.0f, weapon.timer - dt);
        if (aim >= 0 && weapon.timer >= weapon.chargeTime) {
            FireAt(weapon, origin, targets[aim].position);
            weapon.timer = 0.0f;
        }
    } else if (weapon.timer > 0.0f) {
        weapon.timer -= dt;
    } else if (aim >= 0) {
        FireAt(weapon, origin, targets[aim].position);
        weapon.timer = weapon.cooldown;
    }

    StepShots(weapon, dt, targets, sparks);
}

// The player's guns. Starts with oatmeal throwing and the charge shot.
class AutoShooter {
public:
    std::vector<Weapon> weapons;

    explicit AutoShooter(bool equipDefaults = true) {
        if (!equipDefaults) return;

        Weapon oatmeal;
        oatmeal.name = "Oatmeal";
        oatmeal.cooldown = 0.42f;
        oatmeal.chargeTime = 0.0f;
        oatmeal.range = 360.0f;
        oatmeal.speed = 480.0f;
        oatmeal.shotRadius = 6.5f;
        oatmeal.damage = 1;
        oatmeal.life = 1.15f;
        oatmeal.pierce = false;
        oatmeal.color = {214, 186, 132, 255};
        weapons.push_back(oatmeal);

        Weapon charge;
        charge.name = "Charge";
        charge.cooldown = 0.0f;
        charge.chargeTime = 1.35f;
        charge.range = 480.0f;
        charge.speed = 640.0f;
        charge.shotRadius = 12.0f;
        charge.damage = 3;
        charge.life = 0.95f;
        charge.pierce = true;
        charge.color = {214, 92, 42, 255};
        weapons.push_back(charge);
    }

    void Update(Vector2 origin, float dt, std::vector<ShootTarget>& targets) {
        for (ShootTarget& target : targets) {
            if (target.hitFlash > 0.0f) target.hitFlash -= dt;
        }
        for (Weapon& weapon : weapons) UpdateWeapon(weapon, origin, dt, targets, sparks_);
        for (Spark& spark : sparks_) spark.life -= dt;
        sparks_.erase(std::remove_if(sparks_.begin(), sparks_.end(),
                                     [](const Spark& spark) { return spark.life <= 0.0f; }),
                      sparks_.end());
    }

    void Draw() const {
        for (const Weapon& weapon : weapons) {
            for (const Projectile& shot : weapon.shots) {
                DrawCircleV(shot.position, shot.radius, shot.color);
                DrawCircleV(shot.position, shot.radius * 0.45f, {255, 236, 210, 255});
            }
        }
        for (const Spark& spark : sparks_) {
            DrawCircleV(spark.position, 3.0f + spark.life * 20.0f, Fade(spark.color, spark.life / 0.16f));
        }
    }

    // Growing ring on the player while a charge weapon is spinning up.
    void DrawCharge(Vector2 origin) const {
        for (const Weapon& weapon : weapons) {
            if (weapon.chargeTime <= 0.0f || weapon.timer <= 0.0f) continue;
            const float t = weapon.timer / weapon.chargeTime;
            DrawRing(origin, 26.0f, 30.0f, -90.0f, -90.0f + 360.0f * t, 28, weapon.color);
        }
    }

    void DrawHud(int x, int y) const {
        char line[180] = "Auto-fire";
        for (const Weapon& weapon : weapons) {
            char piece[48];
            if (weapon.chargeTime > 0.0f) {
                const int pct = static_cast<int>(100.0f * weapon.timer / weapon.chargeTime);
                std::snprintf(piece, sizeof(piece), "  |  %s %d%%", weapon.name, pct);
            } else if (weapon.timer <= 0.05f) {
                std::snprintf(piece, sizeof(piece), "  |  %s ready", weapon.name);
            } else {
                std::snprintf(piece, sizeof(piece), "  |  %s %.1fs", weapon.name, weapon.timer);
            }
            std::strncat(line, piece, sizeof(line) - std::strlen(line) - 1);
        }
        DrawText(line, x, y, 18, WHITE);
    }

private:
    std::vector<Spark> sparks_;
};

}  // namespace weapons
