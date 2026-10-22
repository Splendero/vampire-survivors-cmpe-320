# Weapons

Weapons attack on their own. The player can upgrade a weapon or pick a new one.

- Oatmeal throwing
- Charge-up ability

`auto_shooter.hpp` is that component. The run starts with the Revolver. Charge is unlocked on level up. Both fire on their own at the nearest living target. There is no shoot key.

A killing hit publishes `events::Type::EnemyKilled`. A shot leaving the gun publishes `WeaponFired`. Subscribe from `Game::Wire` instead of editing this file. `ShootTarget` lives in `src/core/target.hpp`.
