// Finds the monsters to ring and keeps what the overlay needs about them.
//
// Every "Look for monsters every" interval one pass takes the monster list (the
// host drops every other entity type before the SDK copies any strings), keeps
// the monsters of the ticked rarities, the most important first up to the cap,
// and merges them into the target list so per-monster reads survive. Between
// passes only the targets that were on screen are refreshed: their position
// every frame (one small read each, when "Follow monsters every frame" is on)
// and, with destination lines on, their current action and route about ten
// times a second, a few targets per frame within a time budget. Everything runs
// on the render thread inside DrawUI; there is no worker thread to stop.
#pragma once

#include "../../../POEFixer/plugin_sdk/PluginSDK.h"
#include "Logic.h"
#include "Settings.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace Wayg {

struct Target {
    uint32_t  id = 0;
    uintptr_t address = 0;
    uintptr_t render = 0;
    uintptr_t actor = 0;
    int       rarity = 0;

    // World position of the feet (latest known).
    float x = 0.0f, y = 0.0f, z = 0.0f;
    double posTime = 0.0;                 // seconds: when x/y/z was last confirmed
    // Grid and world position from the same scan read (grid -> world conversion).
    float scanGridX = 0.0f, scanGridY = 0.0f, scanX = 0.0f, scanY = 0.0f;
    float distance = 0.0f;                // to the player at the last scan, world units

    bool onScreen = false;                // the feet projected on the last drawn frame
    bool fresh = true;                    // found by the last scan, not drawn yet

    // Destination (destination lines on)
    DestKind destKind = DestKind::None;
    float destX = 0.0f, destY = 0.0f, destZ = 0.0f;   // world
    double nextDestRead = 0.0;
};

struct TrackerStats {
    // last scan
    size_t monstersSeen = 0;
    size_t candidates = 0;
    size_t kept = 0;
    double scanMs = 0.0;
    int    keptByRarity[kRarityCount] = {};
    // per second (rolling)
    int positionReadsPerSec = 0;
    int positionRejectsPerSec = 0;
    int destReadsPerSec = 0;
    int actionValidPerSec = 0;            // actions the host could read
    int skillTargetsPerSec = 0;
    int walkTargetsPerSec = 0;
    double readMsPerSec = 0.0;            // position + destination reads
};

class Tracker {
public:
    void Reset();
    void RequestScan() { m_ForceScan = true; }
    // Nothing is drawn (overlay hidden): no per-frame reads until a ring is on screen again.
    void MarkAllOffScreen() {
        for (Target& t : m_Targets) t.onScreen = t.fresh = false;
    }
    void Tick(const PluginSDK::Context& ctx, const Settings& s, double now);

    std::vector<Target>& Targets() { return m_Targets; }
    const std::vector<Target>& Targets() const { return m_Targets; }
    // Town, hideout or no area: draw nothing.
    bool Suppressed() const { return m_Suppressed; }
    const TrackerStats& Stats() const { return m_Stats; }
    double LastScanAge(double now) const { return m_HaveScan ? now - m_LastScanAt : -1.0; }

private:
    void Scan(const PluginSDK::Context& ctx, const Settings& s, double now);
    void RefreshPositions(const PluginSDK::Context& ctx, double now);
    void RefreshDestinations(const PluginSDK::Context& ctx, double now);
    void ReadDestination(const PluginSDK::Context& ctx, Target& t);
    void RollStats(double now);

    std::vector<Target> m_Targets;
    std::vector<Target> m_Building;
    std::vector<Candidate> m_Candidates;
    std::unordered_map<uint32_t, size_t> m_OldIndex;

    double   m_NextScan = 0.0;
    double   m_LastScanAt = 0.0;
    bool     m_HaveScan = false;
    bool     m_ForceScan = true;
    bool     m_Suppressed = false;
    bool     m_HaveArea = false;
    uint64_t m_AreaCounter = 0;
    float    m_WorldPerCell = 0.0f;
    bool     m_LinesWereOn = false;
    size_t   m_DestCursor = 0;

    TrackerStats m_Stats;
    double m_StatsWindowStart = 0.0;
    int    m_PosReads = 0, m_PosRejects = 0, m_DestReads = 0, m_ActionValid = 0, m_Skill = 0, m_Walk = 0;
    double m_ReadMs = 0.0;
};

} // namespace Wayg
