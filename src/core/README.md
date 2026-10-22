# Core

Shared pieces every system can use.

- `target.hpp` is a `weapons::ShootTarget`. Enemy code owns the list. Guns only write `hp`, `alive`, and `hitFlash`.
- `events.hpp` is the event bus. Publish a fact from the system that caused it. Subscribe from `src/app/game.hpp` (`Game::Wire`) if you want something else to react.

Events in use:

- `EnemyKilled` — experience orbs and pickups. `position` is where they died.
- `WeaponFired` — nothing listens yet. Use it for a gunshot sound.
- `PlayerHurt` — `amount` is the damage, `health` is HP left.
- `PlayerDied` — the run stops. `Game::Update` also checks HP, so game over still happens if nothing is subscribed.
