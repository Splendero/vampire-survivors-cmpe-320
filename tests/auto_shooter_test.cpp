// Headless checks for auto_shooter.hpp. No window. Build with build-linux.sh
// and run bin/auto_shooter_test. Premake does not compile this file; it is
// outside src/ so it does not add a second main() to the game.
#include "weapons/auto_shooter.hpp"

#include <cmath>
#include <iostream>
#include <vector>

static int fails = 0;

#define CHECK(cond)                                                                 \
    do {                                                                            \
        if (!(cond)) {                                                              \
            std::cerr << "FAIL " << __LINE__ << " " << #cond << "\n";               \
            ++fails;                                                                \
        }                                                                           \
    } while (0)

static weapons::ShootTarget Make(float x, float y, int hp = 3) {
    weapons::ShootTarget target;
    target.position = {x, y};
    target.radius = 10.0f;
    target.hp = hp;
    target.alive = true;
    target.hitFlash = 0.0f;
    return target;
}

static weapons::Weapon Oatmeal() {
    weapons::Weapon weapon;
    weapon.name = "Oatmeal";
    weapon.cooldown = 0.5f;
    weapon.chargeTime = 0.0f;
    weapon.range = 300.0f;
    weapon.speed = 200.0f;
    weapon.shotRadius = 6.0f;
    weapon.damage = 1;
    weapon.life = 3.0f;
    weapon.pierce = false;
    weapon.color = WHITE;
    weapon.timer = 0.0f;
    return weapon;
}

int main() {
    {
        weapons::AutoShooter gun;
        CHECK(gun.weapons.size() == 2);
        CHECK(gun.weapons[0].chargeTime == 0.0f);
        CHECK(gun.weapons[1].chargeTime > 0.0f);
        CHECK(gun.weapons[1].pierce);
    }

    // Aims at the nearest living target, not a closer corpse or a farther one.
    {
        weapons::AutoShooter gun(false);
        gun.weapons.push_back(Oatmeal());
        std::vector<weapons::ShootTarget> targets{Make(0.0f, 200.0f), Make(80.0f, 0.0f), Make(0.0f, 20.0f)};
        targets[2].alive = false;
        gun.Update({0.0f, 0.0f}, 0.016f, targets);
        CHECK(gun.weapons[0].shots.size() == 1);
        CHECK(gun.weapons[0].shots[0].velocity.x > 100.0f);
        CHECK(std::fabs(gun.weapons[0].shots[0].velocity.y) < 1.0f);
    }

    // Nothing in range, so the gun waits.
    {
        weapons::AutoShooter gun(false);
        gun.weapons.push_back(Oatmeal());
        std::vector<weapons::ShootTarget> targets{Make(800.0f, 0.0f)};
        gun.Update({0.0f, 0.0f}, 0.2f, targets);
        CHECK(gun.weapons[0].shots.empty());
        CHECK(targets[0].hp == 3);
    }

    // A cooldown shot hits the first target and stops.
    {
        weapons::AutoShooter gun(false);
        gun.weapons.push_back(Oatmeal());
        std::vector<weapons::ShootTarget> targets{Make(40.0f, 0.0f, 5), Make(120.0f, 0.0f, 5)};
        gun.Update({0.0f, 0.0f}, 0.016f, targets);
        gun.Update({0.0f, 0.0f}, 0.3f, targets);
        CHECK(targets[0].hp == 4);
        CHECK(targets[1].hp == 5);
        CHECK(gun.weapons[0].shots.empty());
    }

    // The charge shot waits, then pierces both targets in its path.
    {
        weapons::AutoShooter gun(false);
        weapons::Weapon charge = Oatmeal();
        charge.name = "Charge";
        charge.chargeTime = 1.0f;
        charge.damage = 3;
        charge.pierce = true;
        charge.speed = 200.0f;
        gun.weapons.push_back(charge);
        std::vector<weapons::ShootTarget> targets{Make(50.0f, 0.0f), Make(90.0f, 0.0f)};
        gun.Update({0.0f, 0.0f}, 0.4f, targets);
        CHECK(gun.weapons[0].shots.empty());
        CHECK(gun.weapons[0].timer > 0.3f);
        gun.Update({0.0f, 0.0f}, 0.7f, targets);
        CHECK(!targets[0].alive);
        CHECK(!targets[1].alive);
    }

    // Charge drains when every target is gone.
    {
        weapons::AutoShooter gun(false);
        weapons::Weapon charge = Oatmeal();
        charge.chargeTime = 1.0f;
        gun.weapons.push_back(charge);
        std::vector<weapons::ShootTarget> targets{Make(40.0f, 0.0f)};
        gun.Update({0.0f, 0.0f}, 0.5f, targets);
        const float charged = gun.weapons[0].timer;
        targets[0].alive = false;
        gun.Update({0.0f, 0.0f}, 0.2f, targets);
        CHECK(gun.weapons[0].timer < charged);
    }

    if (fails) {
        std::cerr << fails << " failed\n";
        return 1;
    }
    std::cout << "auto shooter tests passed\n";
    return 0;
}
