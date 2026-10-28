#pragma once
// Music and sound effects. Files go in assets/audio/.
// Missing files are skipped.

#include "raylib.h"

#include <string>

namespace audio {

enum Sfx { SHOOT, HIT, LEVEL_UP, PICKUP, GAME_OVER, XP, SFX_COUNT };

// File names in the same order as Sfx.
inline const char* kSfxFiles[SFX_COUNT] = {
    "shoot.wav", "hit.wav", "level_up.wav", "pickup.wav", "game_over.wav", "xp.wav",
};

// Each sound has a few copies that take turns, so a new shot
// doesn't cut off the one still playing.
constexpr int kCopies = 4;

// Shoot, hit, and XP pitch range, in percent of normal.
constexpr int kPitchMin = 80;
constexpr int kPitchMax = 120;

inline Sound sounds[SFX_COUNT] = {};
inline Sound copies[SFX_COUNT][kCopies] = {};
inline int nextCopy[SFX_COUNT] = {};
inline bool loaded[SFX_COUNT] = {};
inline Music music = {};
inline bool musicLoaded = false;
inline bool muted = false;
inline int lastLevel = 1;
inline int lastXp = 0;

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
        for (int c = 0; c < kCopies; ++c) copies[i][c] = LoadSoundAlias(sounds[i]);
        loaded[i] = true;
    }
    const std::string path = FindFile("music.mp3");
    if (!path.empty()) {
        music = LoadMusicStream(path.c_str());
        musicLoaded = true;
        PlayMusicStream(music);
    }
}

// Call every frame or the music stops. M mutes and unmutes.
inline void Update() {
    if (IsKeyPressed(KEY_M)) {
        muted = !muted;
        SetMasterVolume(muted ? 0.0f : 1.0f);
    }
    if (musicLoaded) UpdateMusicStream(music);
}

// Turns the music down, e.g. while the level up menu is open.
inline void SetMusicQuiet(bool quiet) {
    if (musicLoaded) SetMusicVolume(music, quiet ? 0.3f : 1.0f);
}

// Pauses the music, e.g. on the game over screen. Resumes when paused is false.
inline void SetMusicPaused(bool paused) {
    if (!musicLoaded) return;
    const bool playing = IsMusicStreamPlaying(music);
    if (paused && playing) PauseMusicStream(music);
    if (!paused && !playing) ResumeMusicStream(music);
}

inline void Play(Sfx sfx) {
    if (!loaded[sfx]) return;
    Sound& sound = copies[sfx][nextCopy[sfx]];
    nextCopy[sfx] = (nextCopy[sfx] + 1) % kCopies;

    // These play constantly, so change the pitch a bit each time.
    if (sfx == SHOOT || sfx == HIT || sfx == XP) {
        SetSoundPitch(sound, GetRandomValue(kPitchMin, kPitchMax) / 100.0f);
    }
    PlaySound(sound);
}

// Call every frame. Plays the XP sound when XP or level goes up.
// XP drops back down on a level up, which is why level is checked too.
inline void TrackXp(int level, int xp) {
    if (xp > lastXp || level > lastLevel) Play(XP);
    lastLevel = level;
    lastXp = xp;
}

// Call once, before CloseWindow().
inline void Shutdown() {
    for (int i = 0; i < SFX_COUNT; ++i) {
        if (!loaded[i]) continue;
        for (int c = 0; c < kCopies; ++c) UnloadSoundAlias(copies[i][c]);
        UnloadSound(sounds[i]);
    }
    if (musicLoaded) UnloadMusicStream(music);
    CloseAudioDevice();
}

}  // namespace audio
