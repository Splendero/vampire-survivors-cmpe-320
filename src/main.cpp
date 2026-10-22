#include "app/game.hpp"
#include "audio/audio.hpp"

#include <cstdlib>
#include <cstring>
#include <ctime>

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

    InitWindow(800, 600, "Western Survival Game");
    audio::Init();
    SetTargetFPS(60);

    app::Game game;
    game.Initialize();

    int frame = 0;
    while (!WindowShouldClose()) {
        audio::Update();
        float deltaTime = GetFrameTime();
        if (demoFrames > 0 && deltaTime <= 0.0f) deltaTime = 1.0f / 60.0f;

        game.Update(deltaTime);
        game.Draw();

        // After EndDrawing so the HUD batch is flushed before the grab.
        if (shotPath && demoFrames > 0 && frame == demoFrames - 1) TakeScreenshot(shotPath);
        if (demoFrames > 0 && ++frame >= demoFrames) break;
    }

    game.Shutdown();
    audio::Shutdown();
    CloseWindow();
    return 0;
}
