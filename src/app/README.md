# App

`game.hpp` is one run: player, guns, enemies, map, level-up, and pickups.

`main.cpp` opens the window and calls `Initialize`, `Update`, `Draw`, and `Shutdown`. It does not connect systems. That list is `Game::Wire`. To react to something that already happens, subscribe there instead of editing the shooter or the player.
