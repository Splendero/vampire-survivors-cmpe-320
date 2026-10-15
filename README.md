# vampire-survivors-cmpe-320

Vampire Survivors-style game for our CMPE 320 project.

Theme: Queen's University meets the Wild West.

## Folders

```
assets/                 art and sound
  audio/                music and sound effects
  sprites/              drawn art
    player/             playable characters
    enemies/            enemy art
    weapons/            weapon art
    ui/                 menu and HUD art
  particles/            hit sparks, level-up bursts, and similar effects
  maps/
    desert/             desert map (Spencer)

src/                    game code
  menu/                 start menu
  player/               who you play as
  enemies/              enemy types
  weapons/              weapons and upgrades (auto-fire)
  map/                  loading a map and its difficulty
  gameplay/             what happens during a run
  director/             what spawns, and how hard the run gets
```

## Automatic shooting

Weapons fire by themselves. `src/weapons/auto_shooter.hpp` aims oatmeal and the charge shot at the nearest living target. WASD only moves. Practice bandits in `src/enemies/practice_bandits.hpp` stand in until the real enemy types are merged.

Windows stays on the Premake `.bat` files. On Linux, with raylib available:

```bash
./build-linux.sh
./bin/auto_shooter_test
./bin/western
```
