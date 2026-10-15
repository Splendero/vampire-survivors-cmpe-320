#include "raylib.h"
#include "map/desert_map.hpp"
#include "weapons/auto_shooter.hpp"
#include "enemies/practice_bandits.hpp"

#include <cstdlib>
#include <cstring>

//basic player class, try to implement all classes and functions with this guy in mind
class Player {
public:
    Vector2 position;
    float radius; // stores the radius of the temp player repersentation
    float speed; // stores how fast player can move 

    Player() {
        position = {400.0f, 300.0f};
        radius = 20.0f;
        speed = 250.0f;
    }

    void Update(float deltaTime) { // delta time is used since other wise it would be frame based 
        // WASD movement
        if (IsKeyDown(KEY_W)) {
            position.y -= speed * deltaTime;
        }
        if (IsKeyDown(KEY_S)) {
            position.y += speed * deltaTime;
        }
        if (IsKeyDown(KEY_A)) {
            position.x -= speed * deltaTime;
        }
        if (IsKeyDown(KEY_D)) {
            position.x += speed * deltaTime;
        }
    }

    void Draw() { // drawing the player 
        DrawCircleV(position, radius, BLUE);
    }
};

int main(int argc, char** argv) {
    int demoFrames = 0;
    const char* shotPath = nullptr;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--demo") == 0 && i + 1 < argc) demoFrames = std::atoi(argv[++i]);
        if (std::strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) shotPath = argv[++i];
    }

    // Create game window
    InitWindow(800, 600, "Western Survival Game");

    //set fps limit 
    SetTargetFPS(60);

    // Create player object
    Player player;

    // Guns fire on their own at the nearest bandit in range.
    weapons::AutoShooter shooter;
    enemies::PracticeBandits bandits;

    // Procedural desert, same seed = same map
    DesertMap map(1337);

    // Camera follows the player, keeping them centered on screen
    Camera2D camera = {};
    camera.offset = {400.0f, 300.0f};
    camera.target = player.position;
    camera.zoom = 1.0f;

    int frame = 0;

    // Main game loop
    while (!WindowShouldClose()) {
        float deltaTime = GetFrameTime();
        if (demoFrames > 0 && deltaTime <= 0.0f) deltaTime = 1.0f / 60.0f;

        // Debug keys: R re-rolls the map, F1 shows biomes
        if (IsKeyPressed(KEY_R)) {
            map.SetSeed(map.Seed() * 1664525u + 1013904223u);
        }
        if (IsKeyPressed(KEY_F1)) {
            map.debugBiomes = !map.debugBiomes;
        }

        // Update game logic
        player.Update(deltaTime);
        bandits.Update(player.position, deltaTime);
        shooter.Update(player.position, deltaTime, bandits.targets);
        camera.target = player.position;
        map.Update(camera, deltaTime);

        // Draw everything
        BeginDrawing();

        ClearBackground(BLACK);

        BeginMode2D(camera);
        map.Draw(camera);
        bandits.Draw();
        shooter.Draw();
        player.Draw();
        shooter.DrawCharge(player.position);
        EndMode2D();

        DrawRectangle(0, 0, 800, 56, Fade(BLACK, 0.55f));
        DrawText(TextFormat("WASD move | R new map | F1 biomes | Seed %u | %s",
                            map.Seed(), desert::BiomeName(map.BiomeAt(player.position))),
                 10, 8, 18, WHITE);
        shooter.DrawHud(10, 30);

        EndDrawing();

        // After EndDrawing so the HUD batch is flushed before the grab.
        if (shotPath && demoFrames > 0 && frame == demoFrames - 1) TakeScreenshot(shotPath);

        if (demoFrames > 0 && ++frame >= demoFrames) break;
    }

    map.Unload();
    CloseWindow();

    return 0;
}
