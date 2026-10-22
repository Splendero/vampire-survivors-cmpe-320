#pragma once
// Music and sound effects. Files go in assets/audio/.
// Missing files are skipped.

#include "raylib.h"

#include <string>

namespace audio {

enum Sfx { SHOOT, HIT, LEVEL_UP, PICKUP, GAME_OVER, SFX_COUNT };

// File names in the same order as Sfx.
inline const char* kSfxFiles[SFX_COUNT] = {
    "shoot.wav", "hit.wav", "level_up.wav", "pickup.wav", "game_over.wav",
};

inline Sound sounds[SFX_COUNT] = {};
inline bool loaded[SFX_COUNT] = {};
inline Music music = {};
inline bool musicLoaded = false;

// Returns "" if the file is not found.
inline std::string FindFile(const char* name) {
    const std::string rel = std::string("assets/audio/") + name;
    const std::string candidates[] = {
        rel,
        "../" + rel,
        "../../" + rel,
        std::string(GetApplicationDirectory()) + "../" + rel,
    };
    for (const std::string& path : candidates) {
        if (FileExists(path.c_str())) return path;
    }
    return "";
}

// Call once, after InitWindow().
inline void Init() {
    InitAudioDevice();
    for (int i = 0; i < SFX_COUNT; ++i) {
        const std::string path = FindFile(kSfxFiles[i]);
        if (path.empty()) continue;
        sounds[i] = LoadSound(path.c_str());
        loaded[i] = true;
    }
    const std::string path = FindFile("music.mp3");
    if (!path.empty()) {
        music = LoadMusicStream(path.c_str());
        musicLoaded = true;
        PlayMusicStream(music);
    }
}

// Call every frame or the music stops.
inline void Update() {
    if (musicLoaded) UpdateMusicStream(music);
}

inline void Play(Sfx sfx) {
    if (loaded[sfx]) PlaySound(sounds[sfx]);
}

// Call once, before CloseWindow().
inline void Shutdown() {
    for (int i = 0; i < SFX_COUNT; ++i) {
        if (loaded[i]) UnloadSound(sounds[i]);
    }
    if (musicLoaded) UnloadMusicStream(music);
    CloseAudioDevice();
}

}  // namespace audio
