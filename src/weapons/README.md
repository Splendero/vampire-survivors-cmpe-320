# Weapons

Weapons attack on their own. The player can upgrade a weapon or pick a new one.

- Oatmeal throwing
- Charge-up ability

`auto_shooter.hpp` is that component. `AutoShooter` starts with both guns. Oatmeal throws on a cooldown at the nearest living target in range. Charge builds while a target is in range, then fires a shot that pierces. Pass the player's position and a `vector<ShootTarget>` into `Update`. There is no shoot key.
