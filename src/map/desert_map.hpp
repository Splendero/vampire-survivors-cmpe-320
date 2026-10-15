#pragma once
// Endless procedural desert map drawn with the mojave20 palette
// (assets/maps/desert/mojave20.hex). Header-only: include it from main.cpp,
// construct it after InitWindow() and call Unload() before CloseWindow().
//
// Everything is a pure function of (world position, seed), so the same seed
// always produces the same terrain, features and landmarks.

#include "raylib.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace desert {

// ---------------------------------------------------------------------------
// Palette (index order matches mojave20.hex)
// ---------------------------------------------------------------------------
enum Pal : int {
    SAND_LIGHT,   // dcd1b8
    SAND,         // c8b496
    ROCK_LIGHT,   // aeb0b2
    ROCK,         // 9196a0
    ROCK_MID,     // 60606a
    ROCK_DARK,    // 403f4a
    ROCK_DARKER,  // 38353e
    ROCK_SHADOW,  // 4b444c
    MAUVE,        // 79625d
    SAND_DARK,    // 9e8676
    SAND_WARM,    // be9668
    CLAY,         // a46848
    OLIVE,        // 715a3b
    OLIVE_DARK,   // 453e2a
    BROWN_DARK,   // 3f2926
    SHADOW_RED,   // 2e1515
    BLACK_BROWN,  // 17090c
    RUST,         // 541e13
    FLAME_RED,    // d03c32
    FLAME_ORANGE, // fba64c
    PAL_COUNT
};

inline constexpr uint32_t kMojave20[PAL_COUNT] = {
    0xdcd1b8, 0xc8b496, 0xaeb0b2, 0x9196a0, 0x60606a, 0x403f4a, 0x38353e,
    0x4b444c, 0x79625d, 0x9e8676, 0xbe9668, 0xa46848, 0x715a3b, 0x453e2a,
    0x3f2926, 0x2e1515, 0x17090c, 0x541e13, 0xd03c32, 0xfba64c,
};

// ---------------------------------------------------------------------------
// World layout
// ---------------------------------------------------------------------------
inline constexpr int TILE = 32;                          // world px per tile
inline constexpr int CHUNK_TILES = 16;
inline constexpr int CHUNK_PX = TILE * CHUNK_TILES;      // 512
inline constexpr int TEXEL = 2;                          // world px per ground texel
inline constexpr int CHUNK_TEX = CHUNK_PX / TEXEL;       // 256
inline constexpr int FIELD_STEP = 8;                     // field sample spacing for baking
inline constexpr int FIELD_N = CHUNK_PX / FIELD_STEP + 1;
inline constexpr int REGION_CHUNKS = 3;                  // landmark region = 3x3 chunks
inline constexpr int REGION_PX = CHUNK_PX * REGION_CHUNKS;

enum class Biome : uint8_t { Dunes, Hardpan, Badlands, Canyon, SaltFlat, Wash, Count };

inline const char* BiomeName(Biome b) {
    switch (b) {
        case Biome::Dunes:    return "Dunes";
        case Biome::Hardpan:  return "Hardpan";
        case Biome::Badlands: return "Badlands";
        case Biome::Canyon:   return "Red Canyon";
        case Biome::SaltFlat: return "Salt Flat";
        case Biome::Wash:     return "Dry Wash";
        default:              return "?";
    }
}

enum class FeatureType : uint8_t {
    Boulder, Spire, Joshua, Saguaro, PricklyPear, Barrel,
    Shrub, Tumbleweed, Skull, Driftwood, Flowers,
};

enum class LandmarkType : uint8_t {
    None, Mesa, Mine, GhostTown, Wagon, Railroad, Campfire, WaterTower, Bones,
};

// ---------------------------------------------------------------------------
// Deterministic hashing and noise
// ---------------------------------------------------------------------------
inline uint32_t Hash(int32_t x, int32_t y, uint32_t seed) {
    uint32_t h = static_cast<uint32_t>(x) * 0x8da6b343u;
    h ^= static_cast<uint32_t>(y) * 0xd8163841u;
    h ^= seed * 0xcb1ab31fu;
    h ^= h >> 16; h *= 0x7feb352du;
    h ^= h >> 15; h *= 0x846ca68bu;
    h ^= h >> 16;
    return h;
}

inline float Float01(uint32_t h) { return static_cast<float>(h >> 8) * (1.0f / 16777216.0f); }

inline float Hash01(int32_t x, int32_t y, uint32_t seed) { return Float01(Hash(x, y, seed)); }

struct Rng {
    uint32_t s;
    explicit Rng(uint32_t seed) : s(seed) {}
    uint32_t Next() {
        s = s * 747796405u + 2891336453u;
        uint32_t w = ((s >> ((s >> 28u) + 4u)) ^ s) * 277803737u;
        return (w >> 22u) ^ w;
    }
    float F() { return Float01(Next()); }
    float Range(float a, float b) { return a + (b - a) * F(); }
};

inline float SmoothLerp(float a, float b, float t) { return a + (b - a) * (t * t * (3.0f - 2.0f * t)); }

inline float ValueNoise(float x, float y, uint32_t seed) {
    const float fx = std::floor(x), fy = std::floor(y);
    const int32_t ix = static_cast<int32_t>(fx), iy = static_cast<int32_t>(fy);
    const float tx = x - fx, ty = y - fy;
    const float a = Hash01(ix, iy, seed), b = Hash01(ix + 1, iy, seed);
    const float c = Hash01(ix, iy + 1, seed), d = Hash01(ix + 1, iy + 1, seed);
    return SmoothLerp(SmoothLerp(a, b, tx), SmoothLerp(c, d, tx), ty);
}

inline float Fbm(float x, float y, uint32_t seed, int octaves) {
    float sum = 0.0f, amp = 0.5f, norm = 0.0f;
    for (int i = 0; i < octaves; ++i) {
        sum += amp * ValueNoise(x, y, seed + static_cast<uint32_t>(i) * 101u);
        norm += amp;
        x = x * 2.03f + 17.1f;
        y = y * 2.03f - 9.7f;
        amp *= 0.5f;
    }
    return sum / norm;
}

// fBm clusters around 0.5; stretch it so thresholds cover a useful range.
inline float Stretch(float v, float k) { return std::clamp((v - 0.5f) * k + 0.5f, 0.0f, 1.0f); }

// Distance between nearest and second-nearest cell points (small = on a cell edge).
inline float CellEdge(float x, float y, uint32_t seed) {
    const int32_t ix = static_cast<int32_t>(std::floor(x)), iy = static_cast<int32_t>(std::floor(y));
    float d1 = 9.0f, d2 = 9.0f;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            const uint32_t h = Hash(ix + dx, iy + dy, seed);
            const float px = static_cast<float>(ix + dx) + static_cast<float>(h & 0xFFFFu) / 65536.0f;
            const float py = static_cast<float>(iy + dy) + static_cast<float>(h >> 16) / 65536.0f;
            const float d = std::sqrt((px - x) * (px - x) + (py - y) * (py - y));
            if (d < d1) { d2 = d1; d1 = d; }
            else if (d < d2) { d2 = d; }
        }
    }
    return d2 - d1;
}

inline int FloorDiv(float v, int size) { return static_cast<int>(std::floor(v / static_cast<float>(size))); }

inline int FloorDiv(int a, int b) {
    int q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0))) --q;
    return q;
}

inline float Dist(Vector2 a, Vector2 b) { return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y)); }

inline float DistToSegment(Vector2 p, Vector2 a, Vector2 b) {
    const float vx = b.x - a.x, vy = b.y - a.y;
    const float len2 = vx * vx + vy * vy;
    float t = len2 > 0.0f ? ((p.x - a.x) * vx + (p.y - a.y) * vy) / len2 : 0.0f;
    t = std::clamp(t, 0.0f, 1.0f);
    return Dist(p, {a.x + vx * t, a.y + vy * t});
}

inline constexpr float kBayer4[16] = {
    0.5f / 16, 8.5f / 16, 2.5f / 16, 10.5f / 16,
    12.5f / 16, 4.5f / 16, 14.5f / 16, 6.5f / 16,
    3.5f / 16, 11.5f / 16, 1.5f / 16, 9.5f / 16,
    15.5f / 16, 7.5f / 16, 13.5f / 16, 5.5f / 16,
};

// ---------------------------------------------------------------------------
// Data
// ---------------------------------------------------------------------------
struct Fields {
    float elev = 0, moist = 0, rock = 0, river = 1;
    Biome biome = Biome::Dunes;
};

struct Feature {
    FeatureType type;
    Vector2 pos;       // foot point on the ground (used for y-sorting)
    float scale;       // 0..1 size variation
    uint32_t var;      // seed for drawing details
    float footprint;   // spacing radius
    float solidR;      // collision radius, 0 = walkable
    float cellX;       // tumbleweeds wrap inside their cell
    float cell;
};

struct FeatureSpec {
    FeatureType type;
    int cell;          // jittered-grid cell size in px (must divide CHUNK_PX)
    float chance;      // probability a cell tries to spawn this feature
    float footprint;
    bool flat;         // drawn under everything else, not y-sorted
};

// Ordered big to small so large features claim space first.
inline constexpr FeatureSpec kSpecs[] = {
    {FeatureType::Boulder,     128, 0.55f, 22.0f, false},
    {FeatureType::Spire,       128, 0.45f, 20.0f, false},
    {FeatureType::Joshua,      128, 0.40f, 18.0f, false},
    {FeatureType::Saguaro,     128, 0.45f, 14.0f, false},
    {FeatureType::PricklyPear, 128, 0.35f, 14.0f, false},
    {FeatureType::Barrel,       64, 0.14f,  9.0f, false},
    {FeatureType::Shrub,        64, 0.22f, 10.0f, false},
    {FeatureType::Tumbleweed,  256, 0.45f, 10.0f, false},
    {FeatureType::Skull,       256, 0.20f, 12.0f, true},
    {FeatureType::Driftwood,    64, 0.18f, 12.0f, true},
    {FeatureType::Flowers,      64, 0.45f,  8.0f, true},
};

inline bool IsFlat(FeatureType t) {
    for (const FeatureSpec& s : kSpecs) if (s.type == t) return s.flat;
    return false;
}

inline bool IsFlat(LandmarkType t) {
    return t == LandmarkType::Railroad || t == LandmarkType::Campfire || t == LandmarkType::Bones;
}

struct Landmark {
    LandmarkType type = LandmarkType::None;
    Vector2 pos{};                 // foot point used for y-sorting
    Vector2 clearCenter{};
    float clearRadius = 0;
    Vector2 a{}, b{};              // railroad endpoints
    float p0 = 0, p1 = 0, p2 = 0;  // type-specific dimensions
    uint32_t var = 0;
    std::array<Rectangle, 4> solids{};
    int solidCount = 0;
};

struct Chunk {
    int cx = 0, cy = 0;
    Texture2D tex{};
    std::vector<Feature> features;
};

// ---------------------------------------------------------------------------
// DesertMap
// ---------------------------------------------------------------------------
class DesertMap {
public:
    bool debugBiomes = false;

    explicit DesertMap(uint32_t seed) : seed_(seed) {
        for (int i = 0; i < PAL_COUNT; ++i) pal_[i] = HexToColor(kMojave20[i]);
        const char* candidates[] = {
            "assets/maps/desert/mojave20.hex",
            "../assets/maps/desert/mojave20.hex",
            "../../assets/maps/desert/mojave20.hex",
            TextFormat("%s../assets/maps/desert/mojave20.hex", GetApplicationDirectory()),
        };
        for (const char* path : candidates) {
            if (LoadPalette(path)) break;
        }
    }

    DesertMap(const DesertMap&) = delete;
    DesertMap& operator=(const DesertMap&) = delete;

    // Overrides the built-in colors with a .hex file (one RRGGBB per line).
    bool LoadPalette(const char* path) {
        if (!FileExists(path)) return false;
        char* text = LoadFileText(path);
        if (!text) return false;
        int count = 0;
        for (char* p = text; *p && count < PAL_COUNT;) {
            while (*p == '\n' || *p == '\r' || *p == ' ' || *p == '\t' || *p == '#') ++p;
            if (!*p) break;
            char* end = nullptr;
            const unsigned long v = std::strtoul(p, &end, 16);
            if (end - p == 6) pal_[count++] = HexToColor(static_cast<uint32_t>(v));
            p = (end == p) ? p + 1 : end;
        }
        UnloadFileText(text);
        return count == PAL_COUNT;
    }

    uint32_t Seed() const { return seed_; }

    void SetSeed(uint32_t seed) {
        Unload();
        seed_ = seed;
    }

    void Update(const Camera2D& cam, float dt) {
        time_ += dt;
        const Rectangle view = ViewRect(cam);
        const int x0 = FloorDiv(view.x, CHUNK_PX), x1 = FloorDiv(view.x + view.width, CHUNK_PX);
        const int y0 = FloorDiv(view.y, CHUNK_PX), y1 = FloorDiv(view.y + view.height, CHUNK_PX);

        for (int cy = y0; cy <= y1; ++cy)
            for (int cx = x0; cx <= x1; ++cx) EnsureChunk(cx, cy);

        int budget = 2;
        for (int cy = y0 - 1; cy <= y1 + 1 && budget > 0; ++cy)
            for (int cx = x0 - 1; cx <= x1 + 1 && budget > 0; ++cx)
                if (!chunks_.count(Key(cx, cy))) { EnsureChunk(cx, cy); --budget; }

        for (auto it = chunks_.begin(); it != chunks_.end();) {
            const Chunk& c = it->second;
            if (c.cx < x0 - 2 || c.cx > x1 + 2 || c.cy < y0 - 2 || c.cy > y1 + 2) {
                UnloadTexture(c.tex);
                it = chunks_.erase(it);
            } else {
                ++it;
            }
        }
    }

    void Draw(const Camera2D& cam) {
        const Rectangle view = ViewRect(cam);
        const int x0 = FloorDiv(view.x, CHUNK_PX), x1 = FloorDiv(view.x + view.width, CHUNK_PX);
        const int y0 = FloorDiv(view.y, CHUNK_PX), y1 = FloorDiv(view.y + view.height, CHUNK_PX);

        for (int cy = y0; cy <= y1; ++cy) {
            for (int cx = x0; cx <= x1; ++cx) {
                const Chunk& c = EnsureChunk(cx, cy);
                DrawTexturePro(c.tex, {0, 0, static_cast<float>(CHUNK_TEX), static_cast<float>(CHUNK_TEX)},
                               {static_cast<float>(cx * CHUNK_PX), static_cast<float>(cy * CHUNK_PX),
                                static_cast<float>(CHUNK_PX), static_cast<float>(CHUNK_PX)},
                               {0, 0}, 0.0f, WHITE);
            }
        }

        // Landmarks whose region could reach into view.
        landmarks_.clear();
        const int rx0 = FloorDiv(view.x - 400, REGION_PX), rx1 = FloorDiv(view.x + view.width + 400, REGION_PX);
        const int ry0 = FloorDiv(view.y - 400, REGION_PX), ry1 = FloorDiv(view.y + view.height + 400, REGION_PX);
        for (int ry = ry0; ry <= ry1; ++ry)
            for (int rx = rx0; rx <= rx1; ++rx) {
                Landmark lm = ComputeLandmark(rx, ry);
                if (lm.type != LandmarkType::None) landmarks_.push_back(lm);
            }

        // Flat pass: ground-level decorations.
        for (const Landmark& lm : landmarks_)
            if (IsFlat(lm.type)) DrawLandmark(lm);

        const Rectangle cull{view.x - 128, view.y - 64, view.width + 256, view.height + 192};
        const int fx0 = FloorDiv(cull.x, CHUNK_PX), fx1 = FloorDiv(cull.x + cull.width, CHUNK_PX);
        const int fy0 = FloorDiv(cull.y, CHUNK_PX), fy1 = FloorDiv(cull.y + cull.height, CHUNK_PX);

        items_.clear();
        for (int cy = fy0; cy <= fy1; ++cy) {
            for (int cx = fx0; cx <= fx1; ++cx) {
                auto it = chunks_.find(Key(cx, cy));
                if (it == chunks_.end()) continue;
                for (const Feature& f : it->second.features) {
                    if (!CheckCollisionPointRec(f.pos, cull) && f.type != FeatureType::Tumbleweed) continue;
                    if (IsFlat(f.type)) DrawFeature(f);
                    else items_.push_back({f.pos.y, &f, -1});
                }
            }
        }

        // Sorted pass: anything with height, back to front.
        for (int i = 0; i < static_cast<int>(landmarks_.size()); ++i)
            if (!IsFlat(landmarks_[i].type)) items_.push_back({landmarks_[i].pos.y, nullptr, i});
        std::sort(items_.begin(), items_.end(), [](const DrawItem& a, const DrawItem& b) { return a.y < b.y; });
        for (const DrawItem& item : items_) {
            if (item.feature) DrawFeature(*item.feature);
            else DrawLandmark(landmarks_[item.landmark]);
        }

        if (debugBiomes) DrawBiomeOverlay(view);
    }

    bool IsSolid(Vector2 p) const {
        const Landmark lm = ComputeLandmark(FloorDiv(p.x, REGION_PX), FloorDiv(p.y, REGION_PX));
        for (int i = 0; i < lm.solidCount; ++i)
            if (CheckCollisionPointRec(p, lm.solids[i])) return true;

        const int x0 = FloorDiv(p.x - 32, CHUNK_PX), x1 = FloorDiv(p.x + 32, CHUNK_PX);
        const int y0 = FloorDiv(p.y - 32, CHUNK_PX), y1 = FloorDiv(p.y + 32, CHUNK_PX);
        for (int cy = y0; cy <= y1; ++cy) {
            for (int cx = x0; cx <= x1; ++cx) {
                std::vector<Feature> generated;
                const std::vector<Feature>* features = nullptr;
                auto it = chunks_.find(Key(cx, cy));
                if (it != chunks_.end()) features = &it->second.features;
                else { generated = GenerateFeatures(cx, cy); features = &generated; }
                for (const Feature& f : *features)
                    if (f.solidR > 0 && Dist(p, f.pos) < f.solidR) return true;
            }
        }
        return false;
    }

    Biome BiomeAt(Vector2 p) const { return SampleFields(p.x, p.y).biome; }

    void Unload() {
        for (auto& [key, c] : chunks_) UnloadTexture(c.tex);
        chunks_.clear();
    }

private:
    struct DrawItem {
        float y;
        const Feature* feature;
        int landmark;
    };

    uint32_t seed_;
    float time_ = 0.0f;
    std::array<Color, PAL_COUNT> pal_{};
    std::unordered_map<int64_t, Chunk> chunks_;
    std::vector<Landmark> landmarks_;
    std::vector<DrawItem> items_;

    static Color HexToColor(uint32_t v) {
        return {static_cast<unsigned char>((v >> 16) & 0xFF), static_cast<unsigned char>((v >> 8) & 0xFF),
                static_cast<unsigned char>(v & 0xFF), 255};
    }

    static int64_t Key(int cx, int cy) {
        return (static_cast<int64_t>(cx) << 32) ^ static_cast<int64_t>(static_cast<uint32_t>(cy));
    }

    Color C(Pal p, unsigned char alpha = 255) const {
        Color c = pal_[p];
        c.a = alpha;
        return c;
    }

    static Rectangle ViewRect(const Camera2D& cam) {
        const Vector2 tl = GetScreenToWorld2D({0, 0}, cam);
        const Vector2 br = GetScreenToWorld2D({static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight())}, cam);
        return {tl.x, tl.y, br.x - tl.x, br.y - tl.y};
    }

    // -----------------------------------------------------------------------
    // Fields and biomes
    // -----------------------------------------------------------------------
    Fields SampleFields(float wx, float wy) const {
        const float tx = wx / TILE, ty = wy / TILE;
        Fields f;
        f.elev = Stretch(Fbm(tx / 48.0f, ty / 48.0f, seed_ + 1, 4), 1.7f);
        f.moist = Stretch(Fbm(tx / 70.0f + 13.1f, ty / 70.0f - 4.2f, seed_ + 2, 3), 1.7f);
        f.rock = Stretch(Fbm(tx / 36.0f, ty / 36.0f, seed_ + 3, 4), 1.6f);
        const float warp = ValueNoise(tx / 40.0f, ty / 40.0f, seed_ + 7) - 0.5f;
        auto riverAt = [&](float x, float y) {
            return Fbm(x / 90.0f + warp * 1.5f, y / 90.0f - warp * 1.5f, seed_ + 4, 3) - 0.5f;
        };
        // Divide by the gradient so washes keep a steady width instead of
        // ballooning where the noise is flat.
        const float r0 = riverAt(tx, ty);
        const float gx = riverAt(tx + 0.5f, ty) - r0, gy = riverAt(tx, ty + 0.5f) - r0;
        const float grad = std::max(std::sqrt(gx * gx + gy * gy) * 2.0f, 0.008f);
        f.river = std::min(std::fabs(r0) / grad, 64.0f);
        f.biome = Classify(f);
        return f;
    }

    // Half-width of a dry wash in tiles; wetter areas carve wider channels.
    static float WashHalfWidth(const Fields& f) { return 1.3f + f.moist * 1.8f; }

    static Biome Classify(const Fields& f) {
        if (f.river < WashHalfWidth(f) && f.rock < 0.74f) return Biome::Wash;
        if (f.rock > 0.62f && f.elev > 0.48f) return Biome::Canyon;
        if (f.rock > 0.66f) return Biome::Badlands;
        if (f.elev < 0.30f && f.moist > 0.45f) return Biome::SaltFlat;
        if (f.moist < 0.48f) return Biome::Dunes;
        return Biome::Hardpan;
    }

    template <size_t N>
    Color Ramp(const Pal (&ramp)[N], float t, float threshold) const {
        t = std::clamp(t, 0.0f, 0.9999f);
        const float s = t * static_cast<float>(N - 1);
        int i = static_cast<int>(s);
        if (s - static_cast<float>(i) > threshold && i + 1 < static_cast<int>(N)) ++i;
        return pal_[ramp[i]];
    }

    Color GroundColor(const Fields& f, float wx, float wy, int gx, int gy) const {
        static constexpr Pal kDune[] = {SAND_DARK, SAND_WARM, SAND, SAND_LIGHT};
        static constexpr Pal kHardpan[] = {MAUVE, SAND_DARK, SAND};
        static constexpr Pal kBadlands[] = {ROCK_DARKER, ROCK_DARK, ROCK_SHADOW, ROCK_MID, MAUVE, SAND_DARK};
        static constexpr Pal kCanyon[] = {SHADOW_RED, RUST, CLAY, SAND_WARM};
        static constexpr Pal kSalt[] = {ROCK, ROCK_LIGHT, SAND_LIGHT};
        static constexpr Pal kBank[] = {MAUVE, SAND_DARK};
        static constexpr Pal kBed[] = {SAND_DARK, SAND, SAND_LIGHT};

        const float bayer = kBayer4[(gy & 3) * 4 + (gx & 3)];
        const float detail = ValueNoise(wx / 40.0f, wy / 40.0f, seed_ + 9);
        const float speck = Hash01(gx, gy, seed_ + 31);

        switch (f.biome) {
            case Biome::Dunes: {
                const float band = std::sin(wx * 0.045f + wy * 0.012f + f.elev * 18.0f + detail * 2.0f);
                if (band > 0.96f) return C(SAND_LIGHT);
                if (speck < 0.003f) return C(ROCK_MID);
                return Ramp(kDune, 0.45f + (f.elev - 0.5f) * 0.9f + band * 0.12f + (detail - 0.5f) * 0.2f, bayer);
            }
            case Biome::Hardpan:
                if (speck < 0.004f) return C(ROCK_MID);
                return Ramp(kHardpan, 0.3f + (f.elev - 0.3f) * 0.8f + (detail - 0.5f) * 0.5f, bayer);
            case Biome::Badlands:
                return Ramp(kBadlands, (1.0f - f.rock) * 1.8f + (detail - 0.5f) * 0.5f, bayer);
            case Biome::Canyon: {
                const float strata = std::sin(f.elev * 120.0f + detail * 2.5f) * 0.5f + 0.5f;
                return Ramp(kCanyon, 0.12f + strata * 0.78f, bayer);
            }
            case Biome::SaltFlat:
                if (CellEdge(wx / 56.0f, wy / 56.0f, seed_ + 11) < 0.07f) return C(ROCK);
                return Ramp(kSalt, 0.55f + (detail - 0.5f) * 0.7f, bayer);
            case Biome::Wash: {
                const float depth = 1.0f - f.river / WashHalfWidth(f);
                if (depth < 0.2f) return Ramp(kBank, depth / 0.2f + (detail - 0.5f) * 0.3f, bayer);
                if (speck < 0.04f) return C(speck < 0.02f ? ROCK_MID : ROCK);
                return Ramp(kBed, 0.5f + (detail - 0.5f) * 0.6f, bayer);
            }
            default:
                return C(SAND);
        }
    }

    // -----------------------------------------------------------------------
    // Chunks
    // -----------------------------------------------------------------------
    Chunk& EnsureChunk(int cx, int cy) {
        const int64_t key = Key(cx, cy);
        auto it = chunks_.find(key);
        if (it != chunks_.end()) return it->second;
        return chunks_.emplace(key, BuildChunk(cx, cy)).first->second;
    }

    Chunk BuildChunk(int cx, int cy) const {
        Chunk chunk;
        chunk.cx = cx;
        chunk.cy = cy;
        const float ox = static_cast<float>(cx * CHUNK_PX), oy = static_cast<float>(cy * CHUNK_PX);

        // Fields are smooth, so sample a coarse grid and interpolate per texel.
        std::vector<Fields> grid(FIELD_N * FIELD_N);
        for (int j = 0; j < FIELD_N; ++j)
            for (int i = 0; i < FIELD_N; ++i)
                grid[j * FIELD_N + i] = SampleFields(ox + static_cast<float>(i * FIELD_STEP), oy + static_cast<float>(j * FIELD_STEP));

        std::vector<Color> pixels(CHUNK_TEX * CHUNK_TEX);
        std::vector<Biome> biomes(CHUNK_TEX * CHUNK_TEX);
        for (int py = 0; py < CHUNK_TEX; ++py) {
            const float ly = static_cast<float>(py * TEXEL) + TEXEL * 0.5f;
            const int gj = static_cast<int>(ly) / FIELD_STEP;
            const float fy = (ly - static_cast<float>(gj * FIELD_STEP)) / FIELD_STEP;
            for (int px = 0; px < CHUNK_TEX; ++px) {
                const float lx = static_cast<float>(px * TEXEL) + TEXEL * 0.5f;
                const int gi = static_cast<int>(lx) / FIELD_STEP;
                const float fx = (lx - static_cast<float>(gi * FIELD_STEP)) / FIELD_STEP;
                const Fields& a = grid[gj * FIELD_N + gi];
                const Fields& b = grid[gj * FIELD_N + gi + 1];
                const Fields& c = grid[(gj + 1) * FIELD_N + gi];
                const Fields& d = grid[(gj + 1) * FIELD_N + gi + 1];
                auto bilerp = [&](float Fields::*m) {
                    const float top = a.*m + (b.*m - a.*m) * fx;
                    const float bot = c.*m + (d.*m - c.*m) * fx;
                    return top + (bot - top) * fy;
                };
                Fields f;
                f.elev = bilerp(&Fields::elev);
                f.moist = bilerp(&Fields::moist);
                f.rock = bilerp(&Fields::rock);
                f.river = bilerp(&Fields::river);
                f.biome = Classify(f);
                const int idx = py * CHUNK_TEX + px;
                biomes[idx] = f.biome;
                pixels[idx] = GroundColor(f, ox + lx, oy + ly, cx * CHUNK_TEX + px, cy * CHUNK_TEX + py);
            }
        }

        DrawCracks(cx, cy, pixels, biomes);

        Image img{};
        img.data = pixels.data();
        img.width = CHUNK_TEX;
        img.height = CHUNK_TEX;
        img.mipmaps = 1;
        img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
        chunk.tex = LoadTextureFromImage(img);
        SetTextureFilter(chunk.tex, TEXTURE_FILTER_POINT);

        chunk.features = GenerateFeatures(cx, cy);
        return chunk;
    }

    // Dried-mud cracks on hardpan. Neighboring tiles are walked too so cracks
    // continue across chunk borders.
    void DrawCracks(int cx, int cy, std::vector<Color>& pixels, const std::vector<Biome>& biomes) const {
        const int baseX = cx * CHUNK_TEX, baseY = cy * CHUNK_TEX;
        auto plot = [&](int gx, int gy, Color col) {
            const int lx = gx - baseX, ly = gy - baseY;
            if (lx < 0 || ly < 0 || lx >= CHUNK_TEX || ly >= CHUNK_TEX) return;
            const int idx = ly * CHUNK_TEX + lx;
            if (biomes[idx] == Biome::Hardpan) pixels[idx] = col;
        };
        auto line = [&](float x0, float y0, float x1, float y1, Color col) {
            const int steps = static_cast<int>(std::max(std::fabs(x1 - x0), std::fabs(y1 - y0))) + 1;
            for (int s = 0; s <= steps; ++s) {
                const float t = static_cast<float>(s) / static_cast<float>(steps);
                plot(static_cast<int>(std::floor(x0 + (x1 - x0) * t)), static_cast<int>(std::floor(y0 + (y1 - y0) * t)), col);
            }
        };

        constexpr int texPerTile = TILE / TEXEL;
        for (int ty = -1; ty <= CHUNK_TILES; ++ty) {
            for (int tx = -1; tx <= CHUNK_TILES; ++tx) {
                const int gtx = cx * CHUNK_TILES + tx, gty = cy * CHUNK_TILES + ty;
                const uint32_t h = Hash(gtx, gty, seed_ + 21);
                if (Float01(h) > 0.35f) continue;
                if (SampleFields(gtx * TILE + TILE * 0.5f, gty * TILE + TILE * 0.5f).biome != Biome::Hardpan) continue;

                Rng rng(h);
                const Color col = C(rng.F() < 0.3f ? BROWN_DARK : MAUVE);
                float x = static_cast<float>(gtx * texPerTile) + rng.Range(2, texPerTile - 2);
                float y = static_cast<float>(gty * texPerTile) + rng.Range(2, texPerTile - 2);
                float angle = rng.Range(0, 2 * PI);
                const int segments = 2 + static_cast<int>(rng.F() * 3);
                for (int s = 0; s < segments; ++s) {
                    const float len = rng.Range(3, 8);
                    const float nx = x + std::cos(angle) * len, ny = y + std::sin(angle) * len;
                    line(x, y, nx, ny, col);
                    if (rng.F() < 0.4f) {
                        const float ba = angle + (rng.F() < 0.5f ? -1.0f : 1.0f) * rng.Range(0.6f, 1.3f);
                        line(nx, ny, nx + std::cos(ba) * len * 0.6f, ny + std::sin(ba) * len * 0.6f, col);
                    }
                    x = nx;
                    y = ny;
                    angle += rng.Range(-0.8f, 0.8f);
                }
            }
        }
    }

    // -----------------------------------------------------------------------
    // Features
    // -----------------------------------------------------------------------
    static bool Allowed(FeatureType t, const Fields& f) {
        const Biome b = f.biome;
        switch (t) {
            case FeatureType::Boulder:     return b == Biome::Badlands || b == Biome::Canyon;
            case FeatureType::Spire:       return b == Biome::Canyon;
            case FeatureType::Joshua:      return b == Biome::Hardpan && f.elev > 0.55f;
            case FeatureType::Saguaro:     return (b == Biome::Dunes || b == Biome::Hardpan) && f.moist > 0.38f;
            case FeatureType::PricklyPear: return (b == Biome::Hardpan || b == Biome::Badlands) && f.moist > 0.42f;
            case FeatureType::Barrel:      return b == Biome::Dunes || b == Biome::Hardpan || b == Biome::Badlands;
            case FeatureType::Shrub:       return b == Biome::Dunes || b == Biome::Hardpan || b == Biome::Wash;
            case FeatureType::Tumbleweed:  return b == Biome::Dunes || b == Biome::Hardpan || b == Biome::SaltFlat;
            case FeatureType::Skull:       return b != Biome::Wash && b != Biome::Canyon;
            case FeatureType::Driftwood:   return b == Biome::Wash;
            case FeatureType::Flowers:     return f.moist > 0.62f && (b == Biome::Dunes || b == Biome::Hardpan || b == Biome::Wash);
        }
        return false;
    }

    static bool InClearing(const Landmark& lm, Vector2 p, float pad) {
        if (lm.type == LandmarkType::None) return false;
        if (lm.type == LandmarkType::Railroad) return DistToSegment(p, lm.a, lm.b) < 40.0f + pad;
        return Dist(p, lm.clearCenter) < lm.clearRadius + pad;
    }

    std::vector<Feature> GenerateFeatures(int cx, int cy) const {
        std::vector<Feature> out;
        const Landmark lm = ComputeLandmark(FloorDiv(cx, REGION_CHUNKS), FloorDiv(cy, REGION_CHUNKS));

        for (const FeatureSpec& spec : kSpecs) {
            const int per = CHUNK_PX / spec.cell;
            const uint32_t typeSeed = seed_ ^ (0x9E3779B9u * (static_cast<uint32_t>(spec.type) + 1u));
            const float margin = std::max(spec.footprint, 8.0f);
            for (int j = 0; j < per; ++j) {
                for (int i = 0; i < per; ++i) {
                    const int gx = cx * per + i, gy = cy * per + j;
                    const uint32_t h = Hash(gx, gy, typeSeed);
                    if (Float01(h) >= spec.chance) continue;

                    Rng rng(h);
                    const float ox = static_cast<float>(gx * spec.cell), oy = static_cast<float>(gy * spec.cell);
                    const Vector2 p{ox + margin + rng.F() * (spec.cell - 2 * margin), oy + margin + rng.F() * (spec.cell - 2 * margin)};
                    if (!Allowed(spec.type, SampleFields(p.x, p.y))) continue;

                    const bool rolls = spec.type == FeatureType::Tumbleweed;
                    if (InClearing(lm, p, rolls ? spec.cell * 0.5f : spec.footprint)) continue;
                    if (!rolls) {
                        bool blocked = false;
                        for (const Feature& o : out)
                            if (o.type != FeatureType::Tumbleweed && Dist(o.pos, p) < o.footprint + spec.footprint) { blocked = true; break; }
                        if (blocked) continue;
                    }

                    Feature f{spec.type, p, rng.F(), rng.Next(), spec.footprint, 0.0f, ox, static_cast<float>(spec.cell)};
                    switch (spec.type) {
                        case FeatureType::Boulder: f.solidR = 10.0f + f.scale * 10.0f; break;
                        case FeatureType::Spire:   f.solidR = 12.0f + f.scale * 6.0f; break;
                        case FeatureType::Joshua:  f.solidR = 5.0f; break;
                        case FeatureType::Saguaro: f.solidR = 6.0f; break;
                        default: break;
                    }
                    out.push_back(f);
                }
            }
        }
        return out;
    }

    void Shadow(Vector2 p, float rx, float ry, unsigned char alpha = 60) const {
        DrawEllipse(static_cast<int>(p.x + rx * 0.3f), static_cast<int>(p.y), rx, ry, C(BLACK_BROWN, alpha));
    }

    void DrawFeature(const Feature& f) const {
        Rng r(f.var);
        const Vector2 p = f.pos;
        switch (f.type) {
            case FeatureType::Saguaro: {
                const float h = 40.0f + f.scale * 30.0f, w = 10.0f;
                Shadow(p, 14, 4);
                const float first = r.F() < 0.5f ? -1.0f : 1.0f;
                const int arms = 1 + static_cast<int>(r.F() * 2.0f);
                for (int i = 0; i < arms; ++i) {
                    const float side = i == 0 ? first : -first;
                    const float armY = p.y - h * r.Range(0.35f, 0.6f);
                    const float armUp = r.Range(10, 20);
                    const float ax = p.x + side * (w * 0.5f + 5.0f);
                    const Rectangle stub = side < 0 ? Rectangle{ax - 3.5f, armY - 3.5f, p.x - (ax - 3.5f), 7}
                                                    : Rectangle{p.x, armY - 3.5f, ax + 3.5f - p.x, 7};
                    DrawRectangleRec(stub, C(OLIVE_DARK));
                    DrawRectangleRounded({ax - 3.5f, armY - armUp, 7, armUp + 3.5f}, 1.0f, 4, C(OLIVE_DARK));
                    DrawRectangleRec({ax - 2.0f, armY - armUp + 3, 1.5f, armUp - 3}, C(OLIVE));
                }
                DrawRectangleRounded({p.x - w * 0.5f, p.y - h, w, h}, 1.0f, 6, C(OLIVE_DARK));
                DrawRectangleRec({p.x - w * 0.5f + 2, p.y - h + 4, 2, h - 8}, C(OLIVE));
                DrawRectangleRec({p.x + 1.5f, p.y - h + 5, 1, h - 9}, C(BROWN_DARK));
                if (r.F() < 0.3f) DrawCircleV({p.x, p.y - h + 2}, 2.5f, C(r.F() < 0.5f ? FLAME_ORANGE : FLAME_RED));
                break;
            }
            case FeatureType::Barrel: {
                const float rad = 6.0f + f.scale * 4.0f;
                Shadow(p, rad + 2, 3);
                DrawCircleV({p.x, p.y - rad}, rad, C(OLIVE_DARK));
                for (int k = -1; k <= 1; ++k)
                    DrawLineEx({p.x + k * rad * 0.5f, p.y - rad * 1.8f}, {p.x + k * rad * 0.5f, p.y - rad * 0.25f}, 1.0f, C(OLIVE));
                DrawCircleV({p.x, p.y - rad * 2.0f + 2}, 2.0f, C(r.F() < 0.5f ? FLAME_ORANGE : FLAME_RED));
                break;
            }
            case FeatureType::PricklyPear: {
                Shadow(p, 14, 4);
                const int pads = 3 + static_cast<int>(r.F() * 3);
                for (int k = 0; k < pads; ++k) {
                    const float px = p.x + r.Range(-9, 9);
                    const float py = p.y - 7 - k * 4.0f - r.Range(0, 6);
                    const float rx = r.Range(5, 7), ry = r.Range(7, 9);
                    DrawEllipse(static_cast<int>(px), static_cast<int>(py), rx, ry, C(OLIVE_DARK));
                    DrawEllipseLines(static_cast<int>(px), static_cast<int>(py), rx, ry, C(OLIVE));
                    if (r.F() < 0.5f) DrawCircleV({px + r.Range(-3, 3), py - ry + 1}, 1.6f, C(FLAME_RED));
                }
                break;
            }
            case FeatureType::Shrub: {
                Shadow(p, 9, 3, 45);
                const int branches = 6 + static_cast<int>(r.F() * 4);
                for (int k = 0; k < branches; ++k) {
                    const float a = r.Range(-PI * 0.85f, -PI * 0.15f);
                    const float len = r.Range(7, 14) * (0.8f + f.scale * 0.4f);
                    const Vector2 tip{p.x + std::cos(a) * len, p.y + std::sin(a) * len};
                    DrawLineEx(p, tip, 1.4f, C(k % 2 ? BROWN_DARK : MAUVE));
                    DrawCircleV(tip, 1.4f, C(OLIVE));
                }
                break;
            }
            case FeatureType::Tumbleweed: {
                const float speed = 30.0f + static_cast<float>(f.var % 40u);
                const float travel = (f.pos.x - f.cellX) + time_ * speed;
                const float lx = std::fmod(travel, f.cell);
                const float alpha = std::clamp(std::min(lx, f.cell - lx) / 32.0f, 0.0f, 1.0f);
                const float rad = 7.0f + f.scale * 5.0f;
                const float bounce = std::fabs(std::sin(time_ * 4.0f + static_cast<float>(f.var % 7u))) * 5.0f;
                const Vector2 ground{f.cellX + lx, p.y};
                const Vector2 c{ground.x, ground.y - rad - bounce};
                const float rot = travel / rad;
                DrawEllipse(static_cast<int>(ground.x), static_cast<int>(ground.y), rad, 3, C(BLACK_BROWN, static_cast<unsigned char>(50 * alpha)));
                const unsigned char a = static_cast<unsigned char>(255 * alpha);
                DrawCircleLinesV(c, rad, C(BROWN_DARK, a));
                DrawCircleLinesV(c, rad * 0.6f, C(MAUVE, a));
                for (int k = 0; k < 5; ++k) {
                    const float ang = rot + k * (PI / 5.0f);
                    DrawLineEx({c.x - std::cos(ang) * rad, c.y - std::sin(ang) * rad},
                               {c.x + std::cos(ang) * rad, c.y + std::sin(ang) * rad}, 1.0f, C(k % 2 ? MAUVE : BROWN_DARK, a));
                }
                break;
            }
            case FeatureType::Skull: {
                const Color bone = C(SAND_LIGHT);
                DrawRectangleRounded({p.x - 4, p.y - 8, 8, 10}, 0.6f, 4, bone);
                DrawRectangleRec({p.x - 2.5f, p.y + 1, 5, 4}, bone);
                for (float side : {-1.0f, 1.0f}) {
                    DrawLineEx({p.x + side * 3, p.y - 6}, {p.x + side * 10, p.y - 9}, 2.0f, bone);
                    DrawLineEx({p.x + side * 10, p.y - 9}, {p.x + side * 12, p.y - 14}, 1.6f, bone);
                    DrawRectangleRec({p.x + (side < 0 ? -3.0f : 1.0f), p.y - 5, 2, 2}, C(BLACK_BROWN));
                }
                for (int k = 0; k < 3; ++k) {
                    const Vector2 s{p.x + r.Range(8, 20) * (r.F() < 0.5f ? -1.0f : 1.0f), p.y + r.Range(-4, 8)};
                    const float a = r.Range(0, PI);
                    DrawLineEx({s.x - std::cos(a) * 4, s.y - std::sin(a) * 4}, {s.x + std::cos(a) * 4, s.y + std::sin(a) * 4}, 1.5f, bone);
                }
                break;
            }
            case FeatureType::Driftwood: {
                const float a = r.Range(0, PI);
                const float len = 18.0f + f.scale * 12.0f;
                const Vector2 d{std::cos(a) * len * 0.5f, std::sin(a) * len * 0.5f};
                DrawLineEx({p.x - d.x, p.y - d.y}, {p.x + d.x, p.y + d.y}, 3.0f, C(MAUVE));
                DrawLineEx({p.x - d.x, p.y - d.y - 1}, {p.x + d.x, p.y + d.y - 1}, 1.0f, C(SAND_DARK));
                const float ba = a + r.Range(0.5f, 1.0f);
                DrawLineEx({p.x + d.x * 0.3f, p.y + d.y * 0.3f},
                           {p.x + d.x * 0.3f + std::cos(ba) * 7, p.y + d.y * 0.3f + std::sin(ba) * 7}, 2.0f, C(MAUVE));
                break;
            }
            case FeatureType::Flowers: {
                const int n = 5 + static_cast<int>(r.F() * 5);
                for (int k = 0; k < n; ++k) {
                    const Vector2 s{p.x + r.Range(-10, 10), p.y + r.Range(-7, 7)};
                    DrawLineEx({s.x, s.y + 3}, s, 1.0f, C(OLIVE));
                    const float pick = r.F();
                    DrawCircleV(s, r.Range(1.4f, 2.2f), C(pick < 0.45f ? FLAME_RED : (pick < 0.85f ? FLAME_ORANGE : SAND_LIGHT)));
                }
                break;
            }
            case FeatureType::Boulder: {
                const int rocks = 1 + static_cast<int>(r.F() * 3);
                const float main = 10.0f + f.scale * 10.0f;
                for (int k = 0; k < rocks; ++k) {
                    const float rad = k == 0 ? main : main * r.Range(0.4f, 0.7f);
                    const Vector2 c{p.x + (k == 0 ? 0.0f : r.Range(-main, main)), p.y - rad * 0.7f + (k == 0 ? 0.0f : r.Range(0, main * 0.5f))};
                    Shadow({c.x, c.y + rad * 0.7f}, rad * 1.1f, rad * 0.35f, 70);
                    DrawCircleV({c.x + 1, c.y + 1}, rad, C(ROCK_DARKER));
                    DrawCircleV(c, rad, C(ROCK_MID));
                    DrawCircleV({c.x - rad * 0.3f, c.y - rad * 0.3f}, rad * 0.5f, C(ROCK));
                    DrawCircleV({c.x - rad * 0.4f, c.y - rad * 0.45f}, rad * 0.2f, C(ROCK_LIGHT));
                }
                break;
            }
            case FeatureType::Spire: {
                const float bw = 12.0f + f.scale * 8.0f, h = 50.0f + f.scale * 45.0f;
                Shadow(p, bw * 1.4f, 6, 80);
                static constexpr Pal strata[] = {CLAY, RUST, CLAY, SAND_WARM, RUST};
                const int bands = 5;
                const float bandH = h / bands;
                for (int k = 0; k < bands; ++k) {
                    const float t = static_cast<float>(k) / bands;
                    const float hw = bw * (1.0f - t * 0.6f);
                    const Rectangle band{p.x - hw, p.y - (k + 1) * bandH, hw * 2, bandH + 1};
                    DrawRectangleRec(band, C(strata[(k + f.var) % 5]));
                    DrawRectangleRec({band.x + band.width * 0.65f, band.y, band.width * 0.35f, band.height}, C(SHADOW_RED, 120));
                }
                DrawRectangleRec({p.x - bw * 0.45f, p.y - h - 3, bw * 0.9f, 4}, C(SAND_WARM));
                break;
            }
            case FeatureType::Joshua: {
                const float h = 30.0f + f.scale * 20.0f;
                Shadow(p, 16, 4);
                const Vector2 top{p.x, p.y - h * 0.55f};
                DrawLineEx(p, top, 4.0f, C(BROWN_DARK));
                const int branches = 2 + static_cast<int>(r.F() * 3);
                for (int k = 0; k < branches; ++k) {
                    const float side = (k % 2 == 0) ? -1.0f : 1.0f;
                    const Vector2 mid{top.x + side * r.Range(4, 10), top.y - r.Range(4, 10)};
                    const Vector2 tip{mid.x + side * r.Range(2, 6), mid.y - r.Range(6, 14)};
                    DrawLineEx(top, mid, 3.0f, C(BROWN_DARK));
                    DrawLineEx(mid, tip, 2.5f, C(BROWN_DARK));
                    for (int s = 0; s < 8; ++s) {
                        const float a = s * (2 * PI / 8);
                        DrawLineEx(tip, {tip.x + std::cos(a) * 7, tip.y + std::sin(a) * 6}, 1.5f, C(OLIVE));
                    }
                    DrawCircleV(tip, 4.0f, C(OLIVE_DARK));
                }
                break;
            }
        }
    }

    // -----------------------------------------------------------------------
    // Landmarks
    // -----------------------------------------------------------------------
    static LandmarkType PickLandmark(Biome b, float roll) {
        struct Option { LandmarkType type; float weight; };
        using L = LandmarkType;
        static constexpr Option rocky[] = {{L::Mesa, 0.5f}, {L::Mine, 0.35f}, {L::Bones, 0.15f}};
        static constexpr Option dunes[] = {{L::Bones, 0.3f}, {L::Wagon, 0.3f}, {L::Railroad, 0.2f}, {L::Campfire, 0.2f}};
        static constexpr Option hardpan[] = {{L::GhostTown, 0.3f}, {L::WaterTower, 0.2f}, {L::Railroad, 0.2f}, {L::Campfire, 0.15f}, {L::Wagon, 0.15f}};
        static constexpr Option salt[] = {{L::Bones, 0.4f}, {L::Wagon, 0.3f}, {L::Railroad, 0.3f}};
        static constexpr Option wash[] = {{L::Campfire, 0.5f}, {L::Wagon, 0.5f}};

        auto pick = [roll](const auto& options) {
            float total = 0.0f;
            for (const Option& o : options) total += o.weight;
            float acc = 0.0f;
            for (const Option& o : options) {
                acc += o.weight / total;
                if (roll < acc) return o.type;
            }
            return options[0].type;
        };

        switch (b) {
            case Biome::Badlands:
            case Biome::Canyon:   return pick(rocky);
            case Biome::Dunes:    return pick(dunes);
            case Biome::Hardpan:  return pick(hardpan);
            case Biome::SaltFlat: return pick(salt);
            case Biome::Wash:     return pick(wash);
            default:              return LandmarkType::None;
        }
    }

    // At most one landmark per region, kept away from region edges so its
    // shape and clearing never spill into neighboring regions.
    Landmark ComputeLandmark(int rx, int ry) const {
        Landmark lm;
        Rng rng(Hash(rx, ry, seed_ ^ 0x5bd1e995u));
        if (rng.F() > 0.6f) return lm;

        const float ox = static_cast<float>(rx * REGION_PX), oy = static_cast<float>(ry * REGION_PX);
        lm.pos = {ox + REGION_PX * (0.25f + 0.5f * rng.F()), oy + REGION_PX * (0.25f + 0.5f * rng.F())};
        lm.var = rng.Next();
        lm.type = PickLandmark(SampleFields(lm.pos.x, lm.pos.y).biome, rng.F());
        lm.clearCenter = lm.pos;
        const Vector2 p = lm.pos;

        switch (lm.type) {
            case LandmarkType::Mesa: {
                lm.p0 = rng.Range(240, 420);  // width
                lm.p1 = rng.Range(110, 180);  // plateau depth
                lm.p2 = rng.Range(50, 80);    // cliff height
                lm.solids[0] = {p.x - lm.p0 * 0.5f, p.y - lm.p1 - lm.p2, lm.p0, lm.p1 + lm.p2};
                lm.solidCount = 1;
                lm.clearCenter = {p.x, p.y - (lm.p1 + lm.p2) * 0.5f};
                lm.clearRadius = 0.5f * std::sqrt(lm.p0 * lm.p0 + (lm.p1 + lm.p2) * (lm.p1 + lm.p2)) + 50.0f;
                break;
            }
            case LandmarkType::Mine: {
                lm.p0 = rng.Range(110, 150);  // mound half-width
                lm.p1 = rng.Range(70, 90);    // mound half-height
                lm.solids[0] = {p.x - lm.p0, p.y - lm.p1 * 1.8f, lm.p0 * 2, lm.p1 * 1.8f - 10};
                lm.solidCount = 1;
                lm.clearRadius = lm.p0 + 100.0f;
                break;
            }
            case LandmarkType::GhostTown: {
                const int n = 2 + static_cast<int>(rng.F() * 3);
                const float spacing = 110.0f;
                for (int i = 0; i < n; ++i) {
                    const float w = rng.Range(60, 80), h = rng.Range(50, 64);
                    const float x = p.x + (i - (n - 1) * 0.5f) * spacing + rng.Range(-12, 12);
                    const float y = p.y + rng.Range(-30, 30);
                    lm.solids[i] = {x - w * 0.5f, y - h, w, h};
                }
                lm.solidCount = n;
                lm.clearCenter = {p.x, p.y - 30};
                lm.clearRadius = n * spacing * 0.5f + 90.0f;
                break;
            }
            case LandmarkType::Wagon:
                lm.solids[0] = {p.x - 35, p.y - 30, 70, 26};
                lm.solidCount = 1;
                lm.clearRadius = 90.0f;
                break;
            case LandmarkType::Railroad: {
                const Vector2 c{ox + REGION_PX * 0.5f + rng.Range(-100, 100), oy + REGION_PX * 0.5f + rng.Range(-100, 100)};
                const float angle = (rng.F() < 0.5f ? 0.0f : PI * 0.5f) + rng.Range(-0.15f, 0.15f);
                const float half = REGION_PX * 0.375f;
                const Vector2 d{std::cos(angle) * half, std::sin(angle) * half};
                lm.a = {c.x - d.x, c.y - d.y};
                lm.b = {c.x + d.x, c.y + d.y};
                lm.pos = c;
                lm.clearCenter = c;
                break;
            }
            case LandmarkType::Campfire:
                lm.clearRadius = 80.0f;
                break;
            case LandmarkType::WaterTower:
                lm.solids[0] = {p.x - 32, p.y - 8, 64, 10};
                lm.solidCount = 1;
                lm.clearRadius = 90.0f;
                break;
            case LandmarkType::Bones:
                lm.clearRadius = 110.0f;
                break;
            case LandmarkType::None:
                break;
        }
        return lm;
    }

    void DrawLandmark(const Landmark& lm) const {
        Rng r(lm.var);
        const Vector2 p = lm.pos;
        switch (lm.type) {
            case LandmarkType::Mesa: {
                const float w = lm.p0, topH = lm.p1, cliffH = lm.p2;
                const float x = p.x - w * 0.5f;
                DrawEllipse(static_cast<int>(p.x + 24), static_cast<int>(p.y), w * 0.58f, 22, C(BLACK_BROWN, 70));

                static constexpr Pal strata[] = {RUST, CLAY, SHADOW_RED, CLAY, RUST, ROCK_MID};
                const float faceX = x + 8, faceW = w - 16, faceTop = p.y - cliffH;
                const int bands = 4 + static_cast<int>(r.Next() % 3u);
                const float bandH = cliffH / bands;
                for (int i = 0; i < bands; ++i)
                    DrawRectangleRec({faceX, faceTop + i * bandH, faceW, bandH + 1}, C(strata[(i + lm.var) % 6]));
                DrawRectangleRec({faceX + faceW * 0.72f, faceTop, faceW * 0.28f, cliffH}, C(SHADOW_RED, 110));
                for (int i = 0; i < 7; ++i) {
                    const float cx = faceX + r.F() * faceW;
                    DrawLineEx({cx, faceTop + 2}, {cx + r.Range(-4, 4), faceTop + cliffH * r.Range(0.4f, 1.0f)}, 1.5f, C(BROWN_DARK));
                }
                for (int i = 0; i < 12; ++i) {
                    const float rad = r.Range(3, 7);
                    DrawCircleV({faceX + r.F() * faceW, p.y - rad * 0.4f}, rad, C(i % 2 ? ROCK_DARK : ROCK_MID));
                }

                const Rectangle top{x, faceTop - topH, w, topH + 8};
                DrawRectangleRounded(top, 0.35f, 8, C(CLAY));
                DrawRectangleRounded({x + 8, top.y + 6, w - 16, topH - 8}, 0.35f, 8, C(SAND_WARM));
                DrawRectangleRec({x + 16, faceTop + 2, w - 32, 3}, C(SAND, 180));
                for (int i = 0; i < 10; ++i)
                    DrawCircleV({x + 20 + r.F() * (w - 40), top.y + 14 + r.F() * (topH - 28)}, r.Range(2, 4), C(OLIVE_DARK));
                break;
            }
            case LandmarkType::Mine: {
                const float rx = lm.p0, ry = lm.p1;
                for (int i = 0; i < 8; ++i) {
                    const float ty = p.y + 4 + i * 14.0f;
                    DrawRectangleRec({p.x - 13, ty, 26, 4}, C(BROWN_DARK));
                }
                DrawLineEx({p.x - 8, p.y}, {p.x - 8, p.y + 112}, 2.0f, C(ROCK));
                DrawLineEx({p.x + 8, p.y}, {p.x + 8, p.y + 112}, 2.0f, C(ROCK));

                DrawEllipse(static_cast<int>(p.x + 20), static_cast<int>(p.y - 4), rx * 1.05f, ry * 0.35f, C(BLACK_BROWN, 70));
                DrawEllipse(static_cast<int>(p.x), static_cast<int>(p.y - ry * 0.8f), rx, ry, C(ROCK_DARK));
                DrawEllipse(static_cast<int>(p.x - rx * 0.15f), static_cast<int>(p.y - ry * 0.95f), rx * 0.8f, ry * 0.75f, C(ROCK_MID));
                DrawEllipse(static_cast<int>(p.x - rx * 0.3f), static_cast<int>(p.y - ry * 1.2f), rx * 0.4f, ry * 0.35f, C(ROCK));
                for (int i = 0; i < 10; ++i)
                    DrawCircleV({p.x + r.Range(-rx * 0.8f, rx * 0.8f), p.y - r.Range(ry * 0.2f, ry * 1.4f)}, r.Range(3, 6), C(i % 2 ? ROCK_SHADOW : ROCK));

                DrawRectangleRec({p.x - 22, p.y - 46, 44, 46}, C(OLIVE));
                DrawRectangleRec({p.x - 16, p.y - 40, 32, 40}, C(BLACK_BROWN));
                DrawRectangleRec({p.x - 26, p.y - 52, 52, 7}, C(BROWN_DARK));
                DrawRectangleRec({p.x - 22, p.y - 46, 5, 46}, C(BROWN_DARK));
                DrawRectangleRec({p.x + 17, p.y - 46, 5, 46}, C(BROWN_DARK));

                const Vector2 cart{p.x, p.y + 62};
                DrawRectangleRec({cart.x - 13, cart.y - 12, 26, 14}, C(ROCK_DARKER));
                DrawRectangleRec({cart.x - 11, cart.y - 15, 22, 4}, C(FLAME_ORANGE));
                DrawCircleV({cart.x - 6, cart.y - 15}, 2.5f, C(ROCK_LIGHT));
                DrawCircleV({cart.x - 8, cart.y + 3}, 3.0f, C(BLACK_BROWN));
                DrawCircleV({cart.x + 8, cart.y + 3}, 3.0f, C(BLACK_BROWN));
                break;
            }
            case LandmarkType::GhostTown: {
                int order[4] = {0, 1, 2, 3};
                std::sort(order, order + lm.solidCount, [&](int a, int b) {
                    return lm.solids[a].y + lm.solids[a].height < lm.solids[b].y + lm.solids[b].height;
                });
                float minX = 1e9f, maxX = -1e9f;
                for (int k = 0; k < lm.solidCount; ++k) {
                    const Rectangle s = lm.solids[order[k]];
                    minX = std::min(minX, s.x);
                    maxX = std::max(maxX, s.x + s.width);
                    const float bottom = s.y + s.height;
                    DrawEllipse(static_cast<int>(s.x + s.width * 0.6f), static_cast<int>(bottom), s.width * 0.6f, 8, C(BLACK_BROWN, 70));
                    if (r.F() < 0.5f) {
                        DrawRectangleRec({s.x + 6, s.y - 26, s.width - 12, 28}, C(OLIVE));
                        DrawRectangleRec({s.x + 12, s.y - 20, s.width - 24, 9}, C(SAND_DARK));
                    }
                    DrawRectangleRec(s, C(OLIVE));
                    for (float px = s.x + 7; px < s.x + s.width - 2; px += 8)
                        DrawLineEx({px, s.y + 2}, {px, bottom}, 1.0f, C(BROWN_DARK));
                    DrawRectangleRec({s.x - 4, s.y - 10, s.width + 8, 14}, C(BROWN_DARK));
                    if (r.F() < 0.6f) DrawRectangleRec({s.x + r.Range(4, s.width - 20), s.y - 10, r.Range(8, 16), 6}, C(SHADOW_RED));
                    DrawRectangleRec({s.x + s.width * 0.5f - 7, bottom - 24, 14, 24}, C(BLACK_BROWN));
                    const Rectangle win{s.x + 8, s.y + 12, 12, 10};
                    DrawRectangleRec(win, C(BLACK_BROWN));
                    DrawLineEx({win.x + 6, win.y}, {win.x + 6, win.y + 10}, 1.0f, C(OLIVE));
                    DrawLineEx({win.x, win.y + 5}, {win.x + 12, win.y + 5}, 1.0f, C(OLIVE));
                }
                const float fenceY = p.y + 50;
                bool hasPrev = false;
                float prev = 0.0f;
                for (float fx = minX - 20; fx <= maxX + 20; fx += 18) {
                    const bool missing = r.F() < 0.15f;
                    if (!missing) {
                        if (hasPrev && r.F() < 0.8f) {
                            DrawLineEx({prev, fenceY - 10}, {fx, fenceY - 10}, 1.5f, C(OLIVE));
                            DrawLineEx({prev, fenceY - 4}, {fx, fenceY - 4 + r.Range(-2, 2)}, 1.5f, C(OLIVE));
                        }
                        DrawRectangleRec({fx - 1.5f, fenceY - 14, 3, 14}, C(BROWN_DARK));
                    }
                    hasPrev = !missing;
                    prev = fx;
                }
                break;
            }
            case LandmarkType::Wagon: {
                DrawEllipse(static_cast<int>(p.x + 10), static_cast<int>(p.y), 50, 8, C(BLACK_BROWN, 70));
                DrawLineEx({p.x + 35, p.y - 10}, {p.x + 70, p.y - 2}, 2.5f, C(BROWN_DARK));
                auto wheel = [&](Vector2 c, float rad) {
                    DrawRing(c, rad - 2, rad, 0, 360, 20, C(BROWN_DARK));
                    for (int s = 0; s < 4; ++s) {
                        const float a = s * PI / 4;
                        DrawLineEx({c.x - std::cos(a) * rad, c.y - std::sin(a) * rad}, {c.x + std::cos(a) * rad, c.y + std::sin(a) * rad}, 1.0f, C(OLIVE));
                    }
                };
                wheel({p.x - 22, p.y - 9}, 9);
                DrawRectangleRec({p.x - 35, p.y - 30, 70, 20}, C(OLIVE));
                for (float px = p.x - 28; px < p.x + 35; px += 10)
                    DrawLineEx({px, p.y - 30}, {px, p.y - 10}, 1.0f, C(BROWN_DARK));
                for (int k = 0; k < 3; ++k) {
                    const Vector2 c{p.x - 22 + k * 22.0f, p.y - 30};
                    DrawRing(c, 17, 19, 180, 360, 12, C(SAND_LIGHT));
                }
                DrawCircleSector({p.x - 22, p.y - 30}, 18, 180, 300, 10, C(SAND, 220));
                wheel({p.x + 24, p.y - 7}, 9);
                DrawEllipseLines(static_cast<int>(p.x + 52), static_cast<int>(p.y + 14), 10, 4, C(BROWN_DARK));
                break;
            }
            case LandmarkType::Railroad: {
                const float len = Dist(lm.a, lm.b);
                const Vector2 dir{(lm.b.x - lm.a.x) / len, (lm.b.y - lm.a.y) / len};
                const Vector2 n{-dir.y, dir.x};
                const int ties = static_cast<int>(len / 22.0f);
                for (int i = 0; i <= ties; ++i) {
                    if (Hash01(i, 0, lm.var) < 0.08f) continue;
                    const float jitter = (Hash01(i, 1, lm.var) - 0.5f) * 4.0f;
                    const Vector2 q{lm.a.x + dir.x * (i * 22.0f + jitter), lm.a.y + dir.y * (i * 22.0f + jitter)};
                    DrawLineEx({q.x - n.x * 13, q.y - n.y * 13}, {q.x + n.x * 13, q.y + n.y * 13}, 5.0f, C(BROWN_DARK));
                }
                DrawLineEx({lm.a.x + n.x * 8, lm.a.y + n.y * 8}, {lm.b.x + n.x * 8, lm.b.y + n.y * 8}, 2.5f, C(ROCK));
                DrawLineEx({lm.a.x - n.x * 8, lm.a.y - n.y * 8}, {lm.b.x - n.x * 8, lm.b.y - n.y * 8}, 2.5f, C(ROCK_MID));
                for (Vector2 e : {lm.a, lm.b})
                    DrawLineEx({e.x - n.x * 14, e.y - n.y * 14}, {e.x + n.x * 14, e.y + n.y * 14}, 7.0f, C(OLIVE));
                break;
            }
            case LandmarkType::Campfire: {
                for (int k = 0; k < 3; ++k) {
                    const float a = k * (2 * PI / 3) + r.Range(-0.3f, 0.3f);
                    const Vector2 c{p.x + std::cos(a) * 32, p.y + std::sin(a) * 24};
                    const Vector2 d{-std::sin(a) * 12, std::cos(a) * 9};
                    DrawLineEx({c.x - d.x, c.y - d.y}, {c.x + d.x, c.y + d.y}, 6.0f, C(OLIVE));
                    DrawCircleV({c.x - d.x, c.y - d.y}, 3.0f, C(BROWN_DARK));
                    DrawCircleV({c.x + d.x, c.y + d.y}, 3.0f, C(BROWN_DARK));
                }
                const float glow = 0.18f + 0.08f * std::sin(time_ * 3.0f);
                DrawCircleV(p, 24, Fade(C(FLAME_ORANGE), glow));
                DrawCircleV(p, 12, C(BROWN_DARK));
                DrawCircleV(p, 8, C(ROCK_DARKER));
                for (int k = 0; k < 9; ++k) {
                    const float a = k * (2 * PI / 9);
                    DrawCircleV({p.x + std::cos(a) * 15, p.y + std::sin(a) * 12}, 4.0f, C(k % 2 ? ROCK_MID : ROCK));
                }
                for (int k = 0; k < 6; ++k) {
                    const Vector2 e{p.x + r.Range(-6, 6), p.y + r.Range(-5, 5)};
                    const float flicker = std::sin(time_ * 8.0f + k * 1.7f);
                    DrawCircleV(e, 1.5f + flicker * 0.5f, C(flicker > 0 ? FLAME_ORANGE : FLAME_RED));
                }
                break;
            }
            case LandmarkType::WaterTower: {
                DrawEllipse(static_cast<int>(p.x + 40), static_cast<int>(p.y), 50, 9, C(BLACK_BROWN, 70));
                for (float side : {-1.0f, 1.0f}) {
                    DrawLineEx({p.x + side * 28, p.y}, {p.x + side * 20, p.y - 62}, 4.0f, C(BROWN_DARK));
                    DrawLineEx({p.x + side * 28, p.y}, {p.x - side * 20, p.y - 62}, 1.5f, C(OLIVE));
                }
                DrawLineEx({p.x - 26, p.y - 20}, {p.x + 26, p.y - 20}, 2.0f, C(OLIVE));
                DrawRectangleRec({p.x - 30, p.y - 110, 60, 50}, C(RUST));
                for (int k = 0; k < 3; ++k)
                    DrawLineEx({p.x - 30, p.y - 98 + k * 14.0f}, {p.x + 30, p.y - 98 + k * 14.0f}, 2.0f, C(CLAY));
                for (int k = 0; k < 4; ++k) {
                    const float sx = p.x + r.Range(-26, 26);
                    DrawLineEx({sx, p.y - 104}, {sx, p.y - 104 + r.Range(12, 40)}, 1.5f, C(FLAME_RED));
                }
                DrawRectangleRec({p.x + 10, p.y - 110, 20, 50}, C(SHADOW_RED, 90));
                DrawTriangle({p.x, p.y - 130}, {p.x - 34, p.y - 108}, {p.x + 34, p.y - 108}, C(BROWN_DARK));
                DrawLineEx({p.x + 16, p.y - 4}, {p.x + 16, p.y - 60}, 1.0f, C(ROCK));
                break;
            }
            case LandmarkType::Bones: {
                const Color bone = C(SAND_LIGHT);
                for (int k = 0; k < 18; ++k) {
                    const float x = p.x - 80 + k * 8.0f;
                    DrawCircleV({x, p.y + std::sin(k * 0.4f) * 3}, 3.0f, bone);
                }
                for (int k = 0; k < 7; ++k) {
                    const float x = p.x - 40 + k * 10.0f;
                    const float y = p.y + std::sin((x - p.x + 80) / 8.0f * 0.4f) * 3;
                    for (float side : {-1.0f, 1.0f}) {
                        DrawLineEx({x, y}, {x + 6, y + side * 12}, 2.0f, bone);
                        DrawLineEx({x + 6, y + side * 12}, {x + 2, y + side * 22}, 2.0f, bone);
                    }
                }
                const Vector2 head{p.x + 72, p.y - 2};
                DrawRectangleRounded({head.x - 6, head.y - 7, 24, 14}, 0.6f, 4, bone);
                DrawCircleV({head.x + 2, head.y - 2}, 2.5f, C(BLACK_BROWN));
                DrawLineEx({head.x - 2, head.y - 6}, {head.x - 10, head.y - 18}, 2.5f, bone);
                DrawLineEx({head.x - 2, head.y + 6}, {head.x - 10, head.y + 18}, 2.5f, bone);
                for (int k = 0; k < 4; ++k) {
                    const Vector2 s{p.x + r.Range(-90, 60), p.y + r.Range(-40, 40)};
                    const float a = r.Range(0, PI);
                    DrawLineEx({s.x - std::cos(a) * 10, s.y - std::sin(a) * 10}, {s.x + std::cos(a) * 10, s.y + std::sin(a) * 10}, 3.0f, bone);
                    DrawCircleV({s.x - std::cos(a) * 10, s.y - std::sin(a) * 10}, 2.5f, bone);
                    DrawCircleV({s.x + std::cos(a) * 10, s.y + std::sin(a) * 10}, 2.5f, bone);
                }
                break;
            }
            case LandmarkType::None:
                break;
        }
    }

    void DrawBiomeOverlay(const Rectangle& view) const {
        static constexpr Pal kBiomeColor[] = {SAND_LIGHT, SAND_DARK, ROCK_DARK, FLAME_RED, ROCK_LIGHT, FLAME_ORANGE};
        const int tx0 = FloorDiv(view.x, TILE), tx1 = FloorDiv(view.x + view.width, TILE);
        const int ty0 = FloorDiv(view.y, TILE), ty1 = FloorDiv(view.y + view.height, TILE);
        for (int ty = ty0; ty <= ty1; ++ty)
            for (int tx = tx0; tx <= tx1; ++tx) {
                const Biome b = SampleFields(tx * TILE + TILE * 0.5f, ty * TILE + TILE * 0.5f).biome;
                DrawRectangle(tx * TILE, ty * TILE, TILE, TILE, C(kBiomeColor[static_cast<int>(b)], 110));
            }
    }
};

}  // namespace desert

using desert::DesertMap;
