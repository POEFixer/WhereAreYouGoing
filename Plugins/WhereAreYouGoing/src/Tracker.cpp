#include "Tracker.h"

#include <algorithm>
#include <cmath>

namespace Wayg {

namespace {

using Clock = std::chrono::steady_clock;

constexpr double kDestPeriodSeconds = 0.10;                  // a target's action and route, re-read
constexpr int    kMaxDestReadsPerFrame = 6;
constexpr auto   kDestFrameBudget = std::chrono::microseconds(500);

double MsSince(Clock::time_point t0) {
    return std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
}

} // namespace

void Tracker::Reset() {
    m_Targets.clear();
    m_Building.clear();
    m_Candidates.clear();
    m_OldIndex.clear();
    m_NextScan = 0.0;
    m_HaveScan = false;
    m_ForceScan = true;
    m_Suppressed = false;
    m_HaveArea = false;
    m_DestCursor = 0;
    const TrackerStats rolling = m_Stats;
    m_Stats = TrackerStats{};
    // Keep the per-second figures: they describe the last second, not this area.
    m_Stats.positionReadsPerSec = rolling.positionReadsPerSec;
    m_Stats.positionRejectsPerSec = rolling.positionRejectsPerSec;
    m_Stats.destReadsPerSec = rolling.destReadsPerSec;
    m_Stats.actionValidPerSec = rolling.actionValidPerSec;
    m_Stats.skillTargetsPerSec = rolling.skillTargetsPerSec;
    m_Stats.walkTargetsPerSec = rolling.walkTargetsPerSec;
    m_Stats.readMsPerSec = rolling.readMsPerSec;
}

void Tracker::Tick(const PluginSDK::Context& ctx, const Settings& s, double now) {
    if (m_ForceScan || now >= m_NextScan) Scan(ctx, s, now);

    if (!m_Suppressed && !m_Targets.empty()) {
        if (s.followEveryFrame) RefreshPositions(ctx, now);
        if (s.showLines) RefreshDestinations(ctx, now);
    }
    if (!s.showLines && m_LinesWereOn) {
        for (Target& t : m_Targets) {
            t.destKind = DestKind::None;
            t.nextDestRead = 0.0;
        }
    }
    m_LinesWereOn = s.showLines;
    RollStats(now);
}

void Tracker::Scan(const PluginSDK::Context& ctx, const Settings& s, double now) {
    m_ForceScan = false;
    m_NextScan = now + static_cast<double>(std::clamp(s.scanIntervalMs, Limits::kScanMin, Limits::kScanMax)) / 1000.0;

    bool enabled[kRarityCount] = {};
    bool any = false;
    for (int r = 0; r < kRarityCount; ++r) {
        enabled[r] = s.rarity[static_cast<size_t>(r)].enabled;
        any |= enabled[r];
    }
    // Nothing to ring, or a town / hideout (a cheap flag read): no monster list at all.
    if (!any || ctx.Game.IsTownOrHideout()) {
        m_Targets.clear();
        m_Suppressed = any;
        m_Stats.monstersSeen = m_Stats.candidates = m_Stats.kept = 0;
        std::fill(std::begin(m_Stats.keptByRarity), std::end(m_Stats.keptByRarity), 0);
        m_Stats.scanMs = 0.0;
        return;
    }

    const auto t0 = Clock::now();
    PluginSDK::Snapshot snap = ctx.Game.GetSnapshot(PluginSDK::EntityType::Monster);
    if (!snap.IsAttached || snap.State != PluginSDK::GameState::InGame) {
        Reset();
        m_ForceScan = false;
        m_NextScan = now + static_cast<double>(s.scanIntervalMs) / 1000.0;
        return;
    }
    if (!m_HaveArea || snap.AreaChangeCounter != m_AreaCounter) {
        m_Targets.clear();   // entity ids are reused in the next area
        m_AreaCounter = snap.AreaChangeCounter;
        m_HaveArea = true;
    }
    m_Suppressed = snap.IsTown || snap.IsHideout;
    if (m_Suppressed) {
        m_Targets.clear();
        return;
    }
    if (std::isfinite(snap.WorldToGridConvertor) && snap.WorldToGridConvertor > 0.0f)
        m_WorldPerCell = snap.WorldToGridConvertor;

    const float px = snap.Player.WorldX, py = snap.Player.WorldY;
    m_Candidates.clear();
    m_Candidates.reserve(snap.Entities.size());
    for (size_t i = 0; i < snap.Entities.size(); ++i) {
        const PluginSDK::Entity& e = snap.Entities[i];
        MonsterView v;
        v.valid = e.IsValid && e.Address != 0;
        v.friendly = e.EntityState == PluginSDK::EntityState::MonsterFriendly;
        v.hiddenBoss = e.EntityState == PluginSDK::EntityState::PinnacleBossHidden;
        v.rarity = e.Rarity;
        v.hp = e.CurrentHP;
        v.maxHp = e.MaxHP;
        v.x = e.WorldX;
        v.y = e.WorldY;
        if (!IsTrackable(v, enabled)) continue;
        const float dx = e.WorldX - px, dy = e.WorldY - py;
        m_Candidates.push_back(Candidate{ i, e.Rarity, dx * dx + dy * dy });
    }
    const size_t candidates = m_Candidates.size();
    SelectTargets(m_Candidates, static_cast<size_t>(std::clamp(s.maxTargets, Limits::kTargetsMin, Limits::kTargetsMax)));

    // Merge: a monster that stays keeps what was read about it (destination, screen state).
    m_OldIndex.clear();
    for (size_t k = 0; k < m_Targets.size(); ++k) m_OldIndex[m_Targets[k].id] = k;
    m_Building.clear();
    m_Building.reserve(m_Candidates.size());
    int byRarity[kRarityCount] = {};
    for (const Candidate& c : m_Candidates) {
        const PluginSDK::Entity& e = snap.Entities[c.index];
        Target t;
        auto it = m_OldIndex.find(e.Id);
        if (it != m_OldIndex.end() && m_Targets[it->second].address == e.Address) t = m_Targets[it->second];
        t.id = e.Id;
        t.address = e.Address;
        t.render = e.Components.Render;
        t.actor = e.Components.Actor;
        t.rarity = e.Rarity;
        t.x = e.WorldX;
        t.y = e.WorldY;
        t.z = e.WorldZ;
        t.posTime = now;
        t.scanGridX = e.GridPositionX;
        t.scanGridY = e.GridPositionY;
        t.scanX = e.WorldX;
        t.scanY = e.WorldY;
        t.distance = std::sqrt(c.distSq);
        ++byRarity[std::clamp(e.Rarity, 0, kRarityCount - 1)];
        m_Building.push_back(t);
    }
    m_Targets.swap(m_Building);
    m_Building.clear();
    if (m_DestCursor >= m_Targets.size()) m_DestCursor = 0;

    m_HaveScan = true;
    m_LastScanAt = now;
    m_Stats.monstersSeen = snap.Entities.size();
    m_Stats.candidates = candidates;
    m_Stats.kept = m_Targets.size();
    std::copy(std::begin(byRarity), std::end(byRarity), std::begin(m_Stats.keptByRarity));
    m_Stats.scanMs = MsSince(t0);
}

void Tracker::RefreshPositions(const PluginSDK::Context& ctx, double now) {
    const auto t0 = Clock::now();
    for (Target& t : m_Targets) {
        // Off screen: not drawn, so not read; the next scan brings it back.
        if ((!t.onScreen && !t.fresh) || t.render == 0) continue;
        const PluginSDK::Render r = ctx.Components.ReadRender(t.render);
        ++m_PosReads;
        if (!r.Valid || !PositionPlausible(t.x, t.y, r.WorldX, r.WorldY, r.WorldZ, now - t.posTime)) {
            ++m_PosRejects;
            continue;
        }
        t.x = r.WorldX;
        t.y = r.WorldY;
        t.z = r.WorldZ;
        t.posTime = now;
    }
    m_ReadMs += MsSince(t0);
}

void Tracker::RefreshDestinations(const PluginSDK::Context& ctx, double now) {
    const size_t n = m_Targets.size();
    if (n == 0) return;
    if (m_DestCursor >= n) m_DestCursor = 0;
    const auto t0 = Clock::now();
    int reads = 0;
    for (size_t k = 0; k < n; ++k) {
        const size_t i = (m_DestCursor + k) % n;
        Target& t = m_Targets[i];
        if (!t.onScreen) {
            // Not drawn: forget the old destination and read a new one when it is back.
            t.destKind = DestKind::None;
            t.nextDestRead = 0.0;
            continue;
        }
        if (now < t.nextDestRead) continue;
        if (reads >= kMaxDestReadsPerFrame || (reads > 0 && Clock::now() - t0 >= kDestFrameBudget)) {
            m_DestCursor = i;   // the rest next frame
            m_ReadMs += MsSince(t0);
            return;
        }
        ReadDestination(ctx, t);
        t.nextDestRead = now + kDestPeriodSeconds;
        ++reads;
    }
    m_ReadMs += MsSince(t0);
}

void Tracker::ReadDestination(const PluginSDK::Context& ctx, Target& t) {
    ++m_DestReads;
    ActionView a;
    if (t.actor != 0) {
        const PluginSDK::ActorAction act = ctx.Components.ReadActorAction(t.actor);
        a.valid = act.Valid;
        a.flags = act.Flags;
        a.destX = act.DestX;
        a.destY = act.DestY;
        if (act.Valid) ++m_ActionValid;
    }
    // The monster's cell now: the scan's cell, moved as far as the feet moved since.
    float gx = t.scanGridX, gy = t.scanGridY;
    if (m_WorldPerCell > 0.0f) {
        gx += (t.x - t.scanX) / m_WorldPerCell;
        gy += (t.y - t.scanY) / m_WorldPerCell;
    }

    DestChoice choice = ChooseDestination(a, nullptr, 0, gx, gy);   // a skill target needs no route
    if (choice.kind == DestKind::None && MayWalk(a)) {
        // At most 64 route points (the SDK caps the read).
        const std::vector<PluginSDK::PathNode> nodes = ctx.Components.ReadPathfinding(t.address);
        Cell route[kMaxRouteNodes];
        const size_t count = std::min(nodes.size(), static_cast<size_t>(kMaxRouteNodes));
        for (size_t i = 0; i < count; ++i) route[i] = Cell{ nodes[i].X, nodes[i].Y };
        choice = ChooseDestination(a, route, count, gx, gy);
    }
    if (choice.kind == DestKind::None || !(m_WorldPerCell > 0.0f)) {
        t.destKind = DestKind::None;
        return;
    }
    float wx = 0.0f, wy = 0.0f;
    GridToWorld(choice.x, choice.y, t.scanGridX, t.scanGridY, t.scanX, t.scanY, m_WorldPerCell, wx, wy);
    t.destKind = choice.kind;
    t.destX = wx;
    t.destY = wy;
    t.destZ = PickGroundZ(ctx.Terrain.GetTerrainHeight(choice.x, choice.y), t.z);
    if (choice.kind == DestKind::Skill) ++m_Skill;
    else ++m_Walk;
}

void Tracker::RollStats(double now) {
    if (m_StatsWindowStart <= 0.0 || now < m_StatsWindowStart) m_StatsWindowStart = now;
    if (now - m_StatsWindowStart < 1.0) return;
    const double seconds = now - m_StatsWindowStart;
    auto perSec = [seconds](int v) { return static_cast<int>(std::lround(v / seconds)); };
    m_Stats.positionReadsPerSec = perSec(m_PosReads);
    m_Stats.positionRejectsPerSec = perSec(m_PosRejects);
    m_Stats.destReadsPerSec = perSec(m_DestReads);
    m_Stats.actionValidPerSec = perSec(m_ActionValid);
    m_Stats.skillTargetsPerSec = perSec(m_Skill);
    m_Stats.walkTargetsPerSec = perSec(m_Walk);
    m_Stats.readMsPerSec = m_ReadMs / seconds;
    m_PosReads = m_PosRejects = m_DestReads = m_ActionValid = m_Skill = m_Walk = 0;
    m_ReadMs = 0.0;
    m_StatsWindowStart = now;
}

} // namespace Wayg
