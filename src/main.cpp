#include "raylib.h"
#include "map/desert_map.hpp"
#include "weapons/auto_shooter.hpp"
#include "enemies/practice_bandits.hpp"
#include "gameplay/levelup.hpp"
#include "player/Player.hpp"
#include "gameplay/pickups.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>




// Keep the game setup, update, drawing, and cleanup in separate functions.
struct GameState {
    Player player;
    weapons::AutoShooter shooter;
    enemies::PracticeBandits bandits;
    DesertMap map;
    Camera2D camera = {};
    progression::LevelUpSystem* levelUp = nullptr;
    bool gameOver = false; // true once HP hits 0; the game is frozen until Continue is clicked
    float runTime = 0.0f;  // seconds survived this run; paused during level up and game over
    pickups::PickupSystem pickups; // health crosses and other dropped items

    GameState() : shooter(false), map(1337) {}
};

// The Continue button on the game over screen, in screen pixels.
const Rectangle kContinueButton = {300.0f, 330.0f, 200.0f, 50.0f};

void InitializeGame(GameState& game) {
    // Guns fire on their own at the nearest bandit in range.
    weapons::Weapon revolver;
    revolver.name = "Revolver";
    revolver.cooldown = 0.42f;
    revolver.chargeTime = 0.0f;
    revolver.range = 360.0f;
    revolver.speed = 480.0f;
    revolver.shotRadius = 6.5f;
    revolver.damage = 34;
    revolver.life = 1.15f;
    revolver.pierce = 1; // Base shot hits one enemy; the first Pierce upgrade raises this to two.
    revolver.color = {214, 186, 132, 255};
    game.shooter.weapons.push_back(revolver);

    // Camera follows the player, keeping them centered on screen
    game.camera.offset = {400.0f, 300.0f};
    game.camera.target = game.player.position;
    game.camera.zoom = 1.0f;

    game.levelUp = progression::CreateLevelUpSystem();
}

// Start a fresh run: full HP, starting weapon only, level 1, new enemies.
// The map is kept so the restart is instant.
void ResetGame(GameState& game) {
    progression::DestroyLevelUpSystem(game.levelUp);
    game.levelUp = nullptr;

    game.player = Player();
    game.shooter = weapons::AutoShooter(false); // drops every upgrade and unlocked weapon
    game.bandits = enemies::PracticeBandits();
    game.gameOver = false;
    game.runTime = 0.0f;
    game.pickups = pickups::PickupSystem(); // clear anything left on the ground

    InitializeGame(game); // re-equips the Revolver and creates a new level-up system
}

void UpdateGame(GameState& game, float deltaTime) {
    if (game.gameOver) {
        // Everything is paused. Only the Continue button does anything.
        const bool clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
                             CheckCollisionPointRec(GetMousePosition(), kContinueButton);
        if (clicked) ResetGame(game);
        return;
    }

    // Debug keys: R re-rolls the map, F1 shows biomes
    if (IsKeyPressed(KEY_R)) {
        game.map.SetSeed(game.map.Seed() * 1664525u + 1013904223u);
    }
    if (IsKeyPressed(KEY_F1)) {
        game.map.debugBiomes = !game.map.debugBiomes;
    }

    if (progression::IsActive(game.levelUp)) {
        // The game pauses while the player chooses an upgrade.
        progression::HandleInput(game.levelUp, game.shooter);
    } else {
        // Update game logic. Respawn dead bandits before the guns aim, so a
        // corpse is not still the nearest target on the next frame.
        game.player.Update(deltaTime);
        game.bandits.Update(game.player.position, deltaTime);
        game.shooter.Update(game.player.position, deltaTime, game.bandits.targets);
        //game.shooter.Update(game.player.position, deltaTime, game.bandits.targets);
        //Resolve player enemy collisions after the other updates
        game.player.CheckCollisions(game.bandits.targets, deltaTime);

        if (game.player.health <= 0) {
            game.gameOver = true;
            return;
        }

        // Update dropped EXP and check whether the player can level up.
        progression::UpdateExperience(game.levelUp, game.player.position, game.player.radius, game.bandits.targets);
        progression::StartLevelUpIfReady(game.levelUp, game.shooter);

        // Enemies killed this frame may drop pickups; then collect any the player touches.
        game.pickups.SpawnFromKills(game.bandits.targets);
        game.pickups.Update(game.player, deltaTime);
        game.camera.target = game.player.position;
        game.map.Update(game.camera, deltaTime);

        // Only counts while the game is actually running.
        game.runTime += deltaTime;
    }
}

// Darkened screen with "GAME OVER" and a Continue button. Drawn last, on top of the HUD.
void DrawGameOver() {
    DrawRectangle(0, 0, 800, 600, Fade(BLACK, 0.7f));

    const char* title = "GAME OVER";
    const int titleSize = 64;
    DrawText(title, 400 - MeasureText(title, titleSize) / 2, 210, titleSize, RED);

    const bool hovered = CheckCollisionPointRec(GetMousePosition(), kContinueButton);
    DrawRectangleRec(kContinueButton, hovered ? Color{150, 96, 58, 255} : Color{110, 68, 40, 255});
    DrawRectangleLinesEx(kContinueButton, 2.0f, {236, 214, 170, 255});

    const char* label = "Continue";
    const int labelSize = 28;
    DrawText(label,
             static_cast<int>(kContinueButton.x + kContinueButton.width / 2) - MeasureText(label, labelSize) / 2,
             static_cast<int>(kContinueButton.y + (kContinueButton.height - labelSize) / 2),
             labelSize, WHITE);
}

void DrawGame(GameState& game) {
    // Draw everything
    BeginDrawing();

    ClearBackground(BLACK);

    BeginMode2D(game.camera);
    game.map.Draw(game.camera);
    progression::DrawOrbs(game.levelUp);
    game.pickups.Draw();
    game.bandits.Draw();
    game.shooter.Draw();          // projectiles, in world space
    game.player.Draw();
    game.shooter.DrawCharge(game.player.position); // ring sits on top of the player
    EndMode2D();

    DrawRectangle(0, 0, 800, 82, Fade(BLACK, 0.55f));
    DrawText(TextFormat("WASD move | R new map | F1 biomes | Seed %u | %s",
                        game.map.Seed(), desert::BiomeName(game.map.BiomeAt(game.player.position))),
             10, 8, 18, WHITE);
    game.shooter.DrawHud(10, 30);

    // Display player health
    DrawText(
        TextFormat("HP: %d / %d", game.player.health, game.player.maxHealth),
        650, 30, 18, game.player.health > 30 ? GREEN : RED
    );

    // Run timer as minutes:seconds, right-aligned in the top-right corner.
    const int totalSeconds = static_cast<int>(game.runTime);
    const char* timerText = TextFormat("%02d:%02d", totalSeconds / 60, totalSeconds % 60);
    const int timerSize = 24;
    DrawText(timerText, 790 - MeasureText(timerText, timerSize), 52, timerSize, WHITE);

    progression::DrawHud(game.levelUp);
    progression::DrawMenu(game.levelUp);

    if (game.gameOver) DrawGameOver();

    EndDrawing();
}

void ShutdownGame(GameState& game) {
    progression::DestroyLevelUpSystem(game.levelUp);
    game.map.Unload();
}

int main(int argc, char** argv) {
    // Optional: --demo <frames> quits after that many frames.
    // --screenshot <path> saves the last of those frames. Normal play ignores both.
    int demoFrames = 0;
    const char* shotPath = nullptr;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--demo") == 0 && i + 1 < argc) demoFrames = std::atoi(argv[++i]);
        if (std::strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) shotPath = argv[++i];
    }

    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    // Create game window
    InitWindow(800, 600, "Western Survival Game");

    //set fps limit 
    SetTargetFPS(60);

    GameState game;
    InitializeGame(game);

    int frame = 0;

    // Main game loop
    while (!WindowShouldClose()) {
        float deltaTime = GetFrameTime();
        if (demoFrames > 0 && deltaTime <= 0.0f) deltaTime = 1.0f / 60.0f;

        UpdateGame(game, deltaTime);
        DrawGame(game);

        // After EndDrawing so the HUD batch is flushed before the grab.
        if (shotPath && demoFrames > 0 && frame == demoFrames - 1) TakeScreenshot(shotPath);

        if (demoFrames > 0 && ++frame >= demoFrames) break;
    }

    ShutdownGame(game);
    CloseWindow();

    return 0;
}
