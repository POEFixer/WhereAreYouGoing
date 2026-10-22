// Pure rules of Where Are You Going (no SDK, no ImGui), shared with the
// standalone test in tests/: which monsters get a ring, which are kept when there
// are more than the cap, where a monster is going, and the screen maths of a ring
// lying flat on the ground.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace Wayg {

// ---------------------------------------------------------------------------
// Which monsters get a ring
// ---------------------------------------------------------------------------

// The fields of a PluginSDK::Entity the filter needs.
struct MonsterView {
    bool  valid = false;
    bool  friendly = false;     // EntityState::MonsterFriendly (minions, allies)
    bool  hiddenBoss = false;   // EntityState::PinnacleBossHidden (not in the fight yet)
    int   rarity = 0;           // 0 normal, 1 magic, 2 rare, 3 unique
    int   hp = 0, maxHp = 0;
    float x = 0.0f, y = 0.0f;   // world position
};

// enabled[r]: rings for rarity r are switched on.
inline bool IsTrackable(const MonsterView& m, const bool enabled[4]) {
    if (!m.valid || m.friendly || m.hiddenBoss) return false;
    if (m.rarity < 0 || m.rarity > 3 || !enabled[m.rarity]) return false;
    if (m.maxHp <= 0 || m.hp <= 0) return false;   // dead, or no life read
    if (!std::isfinite(m.x) || !std::isfinite(m.y)) return false;
    if (std::fabs(m.x) < 1.0f && std::fabs(m.y) < 1.0f) return false;   // position not read
    return true;
}

struct Candidate {
    size_t index = 0;    // into the scan's entity list
    int    rarity = 0;
    float  distSq = 0;   // to the player, world units squared
};

// Orders candidates by importance (unique, rare, magic, normal; nearest first
// within a rarity) and keeps at most `cap` of them.
inline void SelectTargets(std::vector<Candidate>& c, size_t cap) {
    auto better = [](const Candidate& a, const Candidate& b) {
        if (a.rarity != b.rarity) return a.rarity > b.rarity;
        if (a.distSq != b.distSq) return a.distSq < b.distSq;
        return a.index < b.index;
    };
    if (c.size() > cap) {
        std::partial_sort(c.begin(), c.begin() + static_cast<std::ptrdiff_t>(cap), c.end(), better);
        c.resize(cap);
    } else {
        std::sort(c.begin(), c.end(), better);
    }
}

// ---------------------------------------------------------------------------
// Where a monster is going
// ---------------------------------------------------------------------------

constexpr int kActionMoving = 0x80;          // every moving action carries it (run 0x1080, 0x2080, 0x2082)
constexpr int kActionUsingAbility = 0x400;   // a skill (attack or spell) is being used
constexpr float kMinDestCells = 2.0f;        // closer than this: the monster is already there
constexpr float kMaxDestCells = 250.0f;      // farther than this: not a real target (about two screens)
constexpr int   kMaxRouteNodes = 64;

struct Cell { int x = 0, y = 0; };

// The monster's current action (PluginSDK::ActorAction).
struct ActionView {
    bool valid = false;   // false: the host could not read it (older host, no Actor)
    int  flags = 0;
    int  destX = 0, destY = 0;
};

enum class DestKind : uint8_t { None, Skill, Walk };

struct DestChoice {
    DestKind kind = DestKind::None;
    int x = 0, y = 0;   // grid cell
};

// A target cell that is real (inside the map's grid) and neither at the monster's
// own cell nor implausibly far away.
inline bool SaneDestination(int x, int y, float unitGridX, float unitGridY) {
    if (x <= 0 || y <= 0 || x >= 65535 || y >= 65535) return false;
    const float dx = static_cast<float>(x) - unitGridX, dy = static_cast<float>(y) - unitGridY;
    const float d2 = dx * dx + dy * dy;
    return std::isfinite(d2) && d2 >= kMinDestCells * kMinDestCells && d2 <= kMaxDestCells * kMaxDestCells;
}

// Whether the walking route is worth reading for this action: the monster moves,
// or the host could not say what it does (then the route alone decides).
inline bool MayWalk(const ActionView& a) { return !a.valid || (a.flags & kActionMoving) != 0; }

// The skill target wins over walking: a monster that runs while it casts aims at
// the skill's target. Else the first route point that is not the monster's own
// cell. A monster standing still (valid action without the moving bit) goes nowhere.
inline DestChoice ChooseDestination(const ActionView& a, const Cell* route, size_t routeCount, float unitGridX,
                                    float unitGridY) {
    if (a.valid && (a.flags & kActionUsingAbility) != 0 && SaneDestination(a.destX, a.destY, unitGridX, unitGridY))
        return DestChoice{ DestKind::Skill, a.destX, a.destY };
    if (!MayWalk(a) || !route) return {};
    const size_t n = std::min(routeCount, static_cast<size_t>(kMaxRouteNodes));
    for (size_t i = 0; i < n; ++i) {
        if (SaneDestination(route[i].x, route[i].y, unitGridX, unitGridY))
            return DestChoice{ DestKind::Walk, route[i].x, route[i].y };
    }
    return {};
}

// World X/Y of a grid cell, relative to a point whose grid and world positions
// come from the same read (avoids any offset between the two systems).
inline void GridToWorld(int gx, int gy, float refGridX, float refGridY, float refWorldX, float refWorldY,
                        float worldPerCell, float& wx, float& wy) {
    wx = refWorldX + (static_cast<float>(gx) - refGridX) * worldPerCell;
    wy = refWorldY + (static_cast<float>(gy) - refGridY) * worldPerCell;
}

// Ground height for a destination: the terrain height when the host has one that
// is plausible near the monster, else the monster's own height.
inline float PickGroundZ(float terrainHeight, float unitZ) {
    if (!std::isfinite(terrainHeight) || terrainHeight == 0.0f) return unitZ;
    return std::fabs(terrainHeight - unitZ) <= 1000.0f ? terrainHeight : unitZ;
}

// ---------------------------------------------------------------------------
// Positions read every frame
// ---------------------------------------------------------------------------

// A fresh position is believed only when it is a real reading and the monster
// could have got there since the last good one (a monster that despawned can
// leave stale or reused memory behind).
inline bool PositionPlausible(float oldX, float oldY, float x, float y, float z, double secondsSinceGood) {
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) return false;
    if (std::fabs(x) < 1.0f && std::fabs(y) < 1.0f) return false;
    const double dt = std::clamp(secondsSinceGood, 0.0, 2.0);
    const double dx = static_cast<double>(x) - oldX, dy = static_cast<double>(y) - oldY;
    const double maxMove = 250.0 + 2000.0 * dt;
    return dx * dx + dy * dy <= maxMove * maxMove;
}

// ---------------------------------------------------------------------------
// A ring flat on the ground, in screen space
// ---------------------------------------------------------------------------

struct V2 {
    float x = 0.0f, y = 0.0f;
};

// Screen pixels per world unit along world X (ex) and world Y (ey) at one point
// of the ground: the local shape of the camera's projection there.
struct GroundBasis {
    V2 ex{ 1.0f, 0.0f }, ey{ 0.0f, 1.0f };
};

// From two probes `probe` world units away along +X / +Y (or -X / -Y when sign is
// -1) of the centre. False when the probes say nothing usable.
inline bool BasisFromProbes(V2 centre, V2 alongX, float signX, V2 alongY, float signY, float probe, GroundBasis& out) {
    if (!(probe > 0.0f)) return false;
    GroundBasis b;
    b.ex = V2{ (alongX.x - centre.x) * signX / probe, (alongX.y - centre.y) * signX / probe };
    b.ey = V2{ (alongY.x - centre.x) * signY / probe, (alongY.y - centre.y) * signY / probe };
    const float det = b.ex.x * b.ey.y - b.ex.y * b.ey.x;   // zero: the axes collapse onto one line
    if (!std::isfinite(det) || std::fabs(det) < 1.0e-6f) return false;
    out = b;
    return true;
}

// World radius of a ground ring whose half-width on screen is `pixels`.
inline float WorldRadiusForPixels(const GroundBasis& b, float pixels) {
    const float h = std::sqrt(b.ex.x * b.ex.x + b.ey.x * b.ey.x);
    if (!std::isfinite(h) || h < 1.0e-4f || !(pixels > 0.0f)) return 0.0f;
    const float r = pixels / h;
    return std::isfinite(r) ? std::min(r, 20000.0f) : 0.0f;
}

// The screen point of a ground offset (wx, wy) in world units from the centre.
inline V2 GroundPoint(V2 centre, const GroundBasis& b, float wx, float wy) {
    return V2{ centre.x + wx * b.ex.x + wy * b.ey.x, centre.y + wx * b.ex.y + wy * b.ey.y };
}

// Sizes in the settings are pixels at 1080p; they grow with the game's resolution.
inline float ResolutionScale(float screenHeight) {
    if (!(screenHeight > 0.0f) || !std::isfinite(screenHeight)) return 1.0f;
    return std::clamp(screenHeight / 1080.0f, 0.5f, 4.0f);
}

// Halvings FarthestVisible gets in the overlay. The end it finds lies at most 1/4096
// of the segment before where the line really leaves the screen: under a pixel for
// a line whose full length would span up to 4096 px on screen, so the line reaches
// the edge and does not jump as the monster moves. One projection per halving, and
// only for a line whose end is off screen.
constexpr int kClipSteps = 12;

// The farthest point of a world segment that is still on screen, when its start
// is on screen and its end is not. `project(t, out)` projects start + t * (end -
// start) and returns false off screen. The on-screen part of a straight ground
// segment is one piece starting at t = 0, so halving the interval finds its end.
template <class Project>
inline bool FarthestVisible(Project&& project, int steps, float& tOut, V2& out) {
    float lo = 0.0f, hi = 1.0f;
    bool found = false;
    for (int i = 0; i < steps; ++i) {
        const float mid = 0.5f * (lo + hi);
        V2 p;
        if (project(mid, p)) {
            lo = mid;
            out = p;
            found = true;
        } else {
            hi = mid;
        }
    }
    tOut = lo;
    return found;
}

} // namespace Wayg
