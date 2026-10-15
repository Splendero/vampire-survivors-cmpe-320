# Enemies

Enemy types and how each one behaves.

- Western people
- Bears
- Canadian geese
- Mosquitos
- Bandits
- Tumbleweed
- Horses
- Sheriff
- Big snakes
- Bosses
- UFO

Enemies drop XP when they die. They get stronger as the run goes on.

`practice_bandits.hpp` is a temporary horde so the guns have targets. Real enemy types should fill `weapons::ShootTarget` and hand that list to `AutoShooter`.
