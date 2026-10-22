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
  app/                  one run, and the event subscriptions that connect it
  core/                 event bus and the shared hit target
  menu/                 start menu
  player/               who you play as
  enemies/              enemy types
  weapons/              weapons and upgrades (auto-fire)
  map/                  loading a map and its difficulty
  gameplay/             level-up, pickups, and the rest of a run
  director/             what spawns, and how hard the run gets
```

## Events

Systems do not call each other. The shooter publishes `EnemyKilled` and `WeaponFired`. The player publishes `PlayerHurt` and `PlayerDied`. `src/app/game.hpp` (`Game::Wire`) is the only place that subscribes, and that is where experience orbs and pickups come from. To react to a kill, a shot, or the player's death, add a subscription there. See `src/core/README.md`.

## Automatic shooting

Weapons fire by themselves. `src/weapons/auto_shooter.hpp` aims the revolver at the nearest living target. WASD only moves. Practice bandits in `src/enemies/practice_bandits.hpp` stand in until the real enemy types are merged.

Windows stays on the Premake `.bat` files. On Linux, with raylib available:

```bash
./build-linux.sh
./bin/auto_shooter_test
./bin/western
```
