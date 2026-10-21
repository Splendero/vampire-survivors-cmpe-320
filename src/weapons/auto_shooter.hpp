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

// One thing a gun is allowed to shoot. Enemy code fills these in; this
// component does not spawn or own enemies.
struct ShootTarget {
    Vector2 position{0.0f, 0.0f};
    float radius = 14.0f;
    int hp = 100;
    int maxHp = 100; // full health, used to size the health bar
    bool alive = true;
    float hitFlash = 0.0f; // seconds left on the white hit flash, counted down by Update
};

// A shot already in the air. hitIndex records targets this shot has damaged
// so a piercing shot does not hit the same enemy every frame.
struct Projectile {
    Vector2 position{0.0f, 0.0f};
    Vector2 velocity{0.0f, 0.0f};
    float radius = 6.0f;
    int damage = 34;
    float life = 1.2f;
    bool alive = true;
    int maxHits = 1; // Maximum number of different enemies this projectile can hit.
    Color color{214, 186, 132, 255};
    int hitIndex[8]{};
    int hitCount = 0;
};

// One equipped gun. chargeTime == 0 fires on cooldown. chargeTime > 0
// builds timer while a target is in range, then releases one shot.
struct Weapon {
    const char* name = "Revolver";
    float cooldown = 0.45f;    // seconds between shots when chargeTime is 0
    float chargeTime = 0.0f;   // seconds to fully charge; 0 means fire on cooldown
    float range = 360.0f;
    float speed = 460.0f;
    float shotRadius = 7.0f;
    int damage = 34;
    float life = 1.2f;
    int pierce = 1; // Number of enemies a shot can hit, including the first enemy.
    Color color{214, 186, 132, 255};
    float timer = 0.0f;        // cooldown remaining, or charge built up
    std::vector<Projectile> shots;
};

// Short hit puff. Drawn and removed by AutoShooter, not by the weapons.
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

// True if this shot already applied damage to targets[index].
inline bool AlreadyHit(const Projectile& shot, int index) {
    for (int i = 0; i < shot.hitCount; ++i) {
        if (shot.hitIndex[i] == index) return true;
    }
    return false;
}

// Index of the closest living target whose edge is within range, or -1.
// Distance is measured to the edge, so a big target is easier to reach.
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

// Deal damage once, flash the target, and kill the shot unless it pierces.
inline void ApplyHit(Projectile& shot, ShootTarget& target, int index, std::vector<Spark>& sparks) {
    target.hp -= shot.damage;
    target.hitFlash = 0.12f;
    if (target.hp <= 0) {
        target.hp = 0;
        target.alive = false;
    }
    if (shot.hitCount < 8) shot.hitIndex[shot.hitCount++] = index;
    if (sparks.size() < 64) sparks.push_back(Spark{target.position, 0.16f, shot.color});
    if (shot.hitCount >= shot.maxHits || shot.hitCount >= 8) shot.alive = false;
}

// Spawn one projectile from origin toward aim. Caps the list so a stuck
// timer cannot allocate forever.
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
    shot.maxHits = std::max(1, std::min(8, weapon.pierce));
    shot.color = weapon.color;
    shot.hitCount = 0;
    weapon.shots.push_back(shot);
}

// Move every shot and test the whole segment it traveled this frame, so a
// fast shot cannot skip over a small target. A normal shot stops on the
// first target along that segment. A piercing shot keeps going.
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

        if (shot.maxHits > 1) {
            // Gather collisions and process them in the order the projectile
            // travels through them. This lets one piercing shot hit multiple
            // enemies along the same path in a single frame.
            struct ShotCollision { int index; float t; };
            std::vector<ShotCollision> collisions;
            for (int i = 0; i < static_cast<int>(targets.size()); ++i) {
                if (!targets[i].alive || AlreadyHit(shot, i)) continue;
                float t = 0.0f;
                const float gap = DistToSegment(targets[i].position, from, shot.position, &t);
                if (gap <= shot.radius + targets[i].radius) {
                    collisions.push_back({i, t});
                }
            }
            std::sort(collisions.begin(), collisions.end(),
                      [](const ShotCollision& a, const ShotCollision& b) { return a.t < b.t; });
            for (const ShotCollision& collision : collisions) {
                if (!shot.alive) break;
                const int i = collision.index;
                // A target may have been killed by another projectile earlier
                // in this same update, so check it again before applying damage.
                if (!targets[i].alive || AlreadyHit(shot, i)) continue;
                ApplyHit(shot, targets[i], i, sparks);
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

// Cooldown guns fire when timer hits 0 and a target is in range.
// Charge guns fill timer only while something is in range, then fire and
// reset. With nothing to shoot, stored charge drains back off.
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

// The player's guns. Starts with the Revolver and the Charge shot.
class AutoShooter {
public:
    std::vector<Weapon> weapons;

    explicit AutoShooter(bool equipDefaults = true) {
        if (!equipDefaults) return;

        Weapon revolver;
        revolver.name = "Revolver";
        revolver.cooldown = 0.42f;
        revolver.chargeTime = 0.0f;
        revolver.range = 360.0f;
        revolver.speed = 480.0f;
        revolver.shotRadius = 6.5f;
        revolver.damage = 34;
        revolver.life = 1.15f;
        revolver.pierce = 1;
        revolver.color = {214, 186, 132, 255};
        weapons.push_back(revolver);

        Weapon charge;
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
        weapons.push_back(charge);
    }

    // origin is the player position in world pixels. targets is not copied;
    // hits write hp, alive, and hitFlash back into that same vector.
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

    // World-space shots and hit sparks. Call inside BeginMode2D.
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

    // Screen-space line: "Auto-fire | Revolver 0.2s | Charge 40%".
    // Call after EndMode2D. x and y are pixels from the top-left of the window.
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
