#pragma once
// One run. main.cpp only opens the window and calls Initialize, Update, Draw,
// and Shutdown. Systems do not call each other: Game::Wire is the list of
// event subscriptions. Add a new reaction there.

#include "raylib.h"
#include "core/events.hpp"
#include "audio/audio.hpp"
#include "map/desert_map.hpp"
#include "weapons/auto_shooter.hpp"
#include "enemies/practice_bandits.hpp"
#include "gameplay/levelup.hpp"
#include "player/Player.hpp"
#include "gameplay/pickups.hpp"

namespace app {

// The Continue button on the game over screen, in screen pixels.
const Rectangle kContinueButton = {300.0f, 330.0f, 200.0f, 50.0f};

void DrawGameOver();

struct Game {
    events::Bus bus;
    Player player;
    weapons::AutoShooter shooter;
    enemies::PracticeBandits bandits;
    DesertMap map;
    Camera2D camera = {};
    progression::LevelUpSystem levelUp;
    bool gameOver = false; // true once HP hits 0; frozen until Continue is clicked
    float runTime = 0.0f;  // seconds survived this run; paused during level up and game over
    pickups::PickupSystem pickups;

    Game() : shooter(false), map(1337) {}

    // Equip the starting revolver, point the camera, and subscribe to events.
    // The map is left alone so a restart does not rebuild the desert.
    void Initialize() {
        weapons::Weapon revolver;
        revolver.name = "Revolver";
        revolver.cooldown = 0.42f;
        revolver.chargeTime = 0.0f;
        revolver.range = 360.0f;
        revolver.speed = 480.0f;
        revolver.shotRadius = 6.5f;
        revolver.damage = 34;
        revolver.life = 1.15f;
        revolver.pierce = 1; // first Pierce upgrade raises this to two
        revolver.color = {214, 186, 132, 255};
        shooter.weapons.push_back(revolver);

        camera.offset = {400.0f, 300.0f};
        camera.target = player.position;
        camera.zoom = 1.0f;

        Wire();
    }

    // Full HP, starting weapon only, level 1, new enemies. The map stays.
    void Reset() {
        player = Player();
        shooter = weapons::AutoShooter(false); // drops every upgrade and unlocked weapon
        bandits = enemies::PracticeBandits();
        levelUp = progression::LevelUpSystem();
        pickups = pickups::PickupSystem();
        gameOver = false;
        runTime = 0.0f;

        Initialize();
    }

    void Update(float deltaTime) {
        if (gameOver) {
            // Everything is paused. Only the Continue button does anything.
            const bool clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
                                 CheckCollisionPointRec(GetMousePosition(), kContinueButton);
            if (clicked) Reset();
            return;
        }

        // Debug keys: R re-rolls the map, F1 shows biomes
        if (IsKeyPressed(KEY_R)) {
            map.SetSeed(map.Seed() * 1664525u + 1013904223u);
        }
        if (IsKeyPressed(KEY_F1)) {
            map.debugBiomes = !map.debugBiomes;
        }

        if (levelUp.IsActive()) {
            // The game pauses while the player chooses an upgrade.
            levelUp.HandleInput(shooter);
        } else {
            // Respawn dead bandits before the guns aim, so a corpse is not
            // still the nearest target. Kills during the shot publish
            // EnemyKilled, which drops the orb and rolls pickups.
            player.Update(deltaTime);
            bandits.Update(player.position, deltaTime);
            shooter.Update(player.position, deltaTime, bandits.targets);
            player.CheckCollisions(bandits.targets, deltaTime);

            if (player.health <= 0) {
                gameOver = true;
                return;
            }

            levelUp.UpdateExperience(player.position, player.radius);
            levelUp.StartLevelUpIfReady(shooter);
            pickups.Update(player, deltaTime);
            camera.target = player.position;
            map.Update(camera, deltaTime);

            // Only counts while the game is actually running.
            runTime += deltaTime;
        }
    }

    void Draw() {
        BeginDrawing();
        ClearBackground(BLACK);

        BeginMode2D(camera);
        map.Draw(camera);
        levelUp.DrawOrbs();
        pickups.Draw();
        bandits.Draw();
        shooter.Draw();
        player.Draw();
        shooter.DrawCharge(player.position);
        EndMode2D();

        DrawRectangle(0, 0, 800, 82, Fade(BLACK, 0.55f));
        DrawText(TextFormat("WASD move | R new map | F1 biomes | Seed %u | %s",
                            map.Seed(), desert::BiomeName(map.BiomeAt(player.position))),
                 10, 8, 18, WHITE);
        shooter.DrawHud(10, 30);

        DrawText(TextFormat("HP: %d / %d", player.health, player.maxHealth),
                 650, 30, 18, player.health > 30 ? GREEN : RED);

        const int totalSeconds = static_cast<int>(runTime);
        const char* timerText = TextFormat("%02d:%02d", totalSeconds / 60, totalSeconds % 60);
        const int timerSize = 24;
        DrawText(timerText, 790 - MeasureText(timerText, timerSize), 52, timerSize, WHITE);

        levelUp.DrawHud();
        levelUp.DrawMenu();

        if (gameOver) DrawGameOver();

        EndDrawing();
    }

    void Shutdown() { map.Unload(); }

private:
    // The only place systems are connected. Subscribe here when a new system
    // needs to hear about a kill, a shot, or the player dying.
    void Wire() {
        bus.Clear();
        player.SetBus(&bus);
        shooter.SetBus(&bus);

        bus.Subscribe(events::Type::EnemyKilled, [this](const events::Event& event) {
            levelUp.OnEnemyKilled(event.position);
            pickups.OnEnemyKilled(event.position);
        });
        bus.Subscribe(events::Type::PlayerDied, [this](const events::Event&) {
            gameOver = true;
        });

        bus.Subscribe(events::Type::WeaponFired, [](const events::Event&) { audio::Play(audio::SHOOT); });
        bus.Subscribe(events::Type::PlayerHurt, [](const events::Event&) { audio::Play(audio::HIT); });
        bus.Subscribe(events::Type::PlayerDied, [](const events::Event&) { audio::Play(audio::GAME_OVER); });
    }
};

inline void DrawGameOver() {
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

}  // namespace app
