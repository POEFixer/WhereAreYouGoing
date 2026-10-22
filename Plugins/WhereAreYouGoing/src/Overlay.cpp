#include "Overlay.h"

#include "Logic.h"

#include "../../../POEFixer/plugin_sdk/PluginUi.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>

namespace Wayg {

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kProbeWorld = 20.0f;   // world units: how far the ground's local shape is probed
// Closed outlines centred on their path (ImGui 1.93 strokes a closed shape inside
// by default, and takes the thickness before the flags): the dark edge then shows
// on both sides of the coloured line.
constexpr ImDrawFlags kClosedCentered = ImDrawFlags_Closed | ImDrawFlags_StrokeCenter;

ImU32 ToU32(const Color4& c) {
    return ImGui::ColorConvertFloat4ToU32(ImVec4(c.r, c.g, c.b, std::clamp(c.a, 0.0f, 1.0f)));
}

// The thin dark edge under every stroke, as strong as the colour is opaque.
ImU32 EdgeFor(const Color4& c) {
    return IM_COL32(0, 0, 0, static_cast<int>(170.0f * std::clamp(c.a, 0.0f, 1.0f)));
}

ImVec2 Im(V2 v) { return ImVec2(v.x, v.y); }

bool Project(const PluginSDK::Context& ctx, float x, float y, float z, V2& out) {
    float sx = 0.0f, sy = 0.0f;
    if (!ctx.Render.WorldToScreen(x, y, z, sx, sy) || !std::isfinite(sx) || !std::isfinite(sy)) return false;
    out = V2{ sx, sy };
    return true;
}

// The ground's local shape where the feet are: two short probes along world X and
// Y (the other way when one leaves the screen; the host projects on-screen points
// only).
bool ProbeBasis(const PluginSDK::Context& ctx, float x, float y, float z, V2 centre, GroundBasis& out) {
    V2 ax, ay;
    float sx = 1.0f, sy = 1.0f;
    bool okX = Project(ctx, x + kProbeWorld, y, z, ax);
    if (!okX) {
        okX = Project(ctx, x - kProbeWorld, y, z, ax);
        sx = -1.0f;
    }
    bool okY = Project(ctx, x, y + kProbeWorld, z, ay);
    if (!okY) {
        okY = Project(ctx, x, y - kProbeWorld, z, ay);
        sy = -1.0f;
    }
    return okX && okY && BasisFromProbes(centre, ax, sx, ay, sy, kProbeWorld, out);
}

float Dist(V2 a, V2 b) {
    const float dx = a.x - b.x, dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

// One straight stroke. ImGui 1.93's AddLine has no half-pixel offset any more, so
// an exactly horizontal or vertical line is drawn with AddLineH / AddLineV on a
// whole pixel (crisp); any other angle is antialiased anyway.
void Segment(ImDrawList* dl, V2 a, V2 b, ImU32 col, float thickness) {
    if (std::fabs(a.y - b.y) < 0.5f) {
        dl->AddLineH(std::min(a.x, b.x), std::max(a.x, b.x), PluginUi::SnapPx(0.5f * (a.y + b.y)), col, thickness,
                     ImDrawFlags_StrokeCenterBiased);
    } else if (std::fabs(a.x - b.x) < 0.5f) {
        dl->AddLineV(PluginUi::SnapPx(0.5f * (a.x + b.x)), std::min(a.y, b.y), std::max(a.y, b.y), col, thickness,
                     ImDrawFlags_StrokeCenterBiased);
    } else {
        dl->AddLine(Im(a), Im(b), col, thickness);   // centred stroke (1.93 default for lines)
    }
}

struct RingShape {
    std::array<float, Limits::kSegmentsMax> cosT{}, sinT{};
    int segs = 0;
};

// Points of a ground ring of world radius r around `centre` (screen).
void RingPoints(const RingShape& shape, V2 centre, const GroundBasis& b, float r, std::array<ImVec2, Limits::kSegmentsMax>& pts) {
    for (int i = 0; i < shape.segs; ++i) {
        const size_t k = static_cast<size_t>(i);
        pts[k] = Im(GroundPoint(centre, b, shape.cosT[k] * r, shape.sinT[k] * r));
    }
}

} // namespace

void DrawOverlay(const PluginSDK::Context& ctx, const Settings& s, Tracker& tracker, OverlayStats& stats) {
    const auto t0 = std::chrono::steady_clock::now();
    stats.rings = stats.lines = stats.offScreen = 0;
    std::vector<Target>& targets = tracker.Targets();
    if (tracker.Suppressed() || targets.empty()) {
        stats.lastFrameUs = 0.0;
        return;
    }
    const PluginSDK::ScreenSize screen = ctx.Game.GetScreenSize();
    if (!(screen.Width > 0.0f) || !(screen.Height > 0.0f)) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    if (!dl) return;

    const float k = ResolutionScale(screen.Height);
    RingShape shape;
    shape.segs = std::clamp(s.ringSegments, Limits::kSegmentsMin, Limits::kSegmentsMax);
    for (int i = 0; i < shape.segs; ++i) {
        const float a = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(shape.segs);
        shape.cosT[static_cast<size_t>(i)] = std::cos(a);
        shape.sinT[static_cast<size_t>(i)] = std::sin(a);
    }
    std::array<ImVec2, Limits::kSegmentsMax> pts{};
    const float lineTh = std::max(1.0f, std::clamp(s.lineThickness, Limits::kLineMin, Limits::kLineMax) * k);
    const float markerPx = std::clamp(s.markerRadius, Limits::kMarkerMin, Limits::kMarkerMax) * k;

    // Targets are ordered most important first; draw them last so they end on top.
    for (auto it = targets.rbegin(); it != targets.rend(); ++it) {
        Target& t = *it;
        const RarityStyle& st = s.rarity[static_cast<size_t>(std::clamp(t.rarity, 0, kRarityCount - 1))];
        t.fresh = false;   // projected once: from now on it is read only while on screen
        if (!st.enabled) {   // switched off since the last scan
            t.onScreen = false;
            continue;
        }
        V2 c;
        if (!Project(ctx, t.x, t.y, t.z, c)) {
            t.onScreen = false;
            ++stats.offScreen;
            continue;
        }
        t.onScreen = true;

        GroundBasis b;
        float r = 0.0f;
        const float ringPx = std::clamp(st.radius, Limits::kRadiusMin, Limits::kRadiusMax) * k;
        if (ProbeBasis(ctx, t.x, t.y, t.z, c, b)) r = WorldRadiusForPixels(b, ringPx);
        const bool onGround = r > 0.0f;
        if (!onGround) {   // no usable ground shape: a plain screen circle of the same size
            b = GroundBasis{};
            r = ringPx;
        }
        c = V2{ PluginUi::SnapPx(c.x), PluginUi::SnapPx(c.y) };

        const ImU32 col = ToU32(st.color);
        const ImU32 edge = EdgeFor(st.color);
        const float th = std::max(1.0f, std::clamp(st.thickness, Limits::kThicknessMin, Limits::kThicknessMax) * k);
        RingPoints(shape, c, b, r, pts);
        dl->AddPolyline(pts.data(), shape.segs, edge, th + 2.0f, kClosedCentered);
        dl->AddPolyline(pts.data(), shape.segs, col, th, kClosedCentered);
        ++stats.rings;

        // The line is placed with the ground's shape (world units); without it, this
        // frame shows the ring only.
        if (!s.showLines || t.destKind == DestKind::None || !onGround) continue;
        const float dx = t.destX - t.x, dy = t.destY - t.y, dz = t.destZ - t.z;
        const float len = std::sqrt(dx * dx + dy * dy);
        if (!(len > r)) continue;   // already inside its own ring
        const float ux = dx / len, uy = dy / len;
        float rm = WorldRadiusForPixels(b, markerPx);   // the end marker, in the ring's world units
        if (!(rm > 0.0f)) rm = markerPx;
        const V2 start = GroundPoint(c, b, ux * r, uy * r);   // on the ring, towards the destination
        V2 end;
        V2 lineEnd;
        const bool endVisible = Project(ctx, t.destX, t.destY, t.destZ, end);
        bool drawLine = true;
        if (endVisible) {
            lineEnd = GroundPoint(end, b, -ux * rm, -uy * rm);   // stops at the marker's edge
            drawLine = len > r + rm;
        } else {
            // Destination off screen: the line runs up to where it leaves the screen.
            float along = 0.0f;
            auto project = [&](float u, V2& out) { return Project(ctx, t.x + dx * u, t.y + dy * u, t.z + dz * u, out); };
            if (!FarthestVisible(project, kClipSteps, along, lineEnd) || along * len <= r) continue;
        }
        if (drawLine && Dist(start, lineEnd) >= 2.0f) {
            Segment(dl, start, lineEnd, edge, lineTh + 2.0f);
            Segment(dl, start, lineEnd, col, lineTh);
            ++stats.lines;
        }
        if (!endVisible) continue;

        // End marker: a filled dot for a skill target, a ring for a walking destination.
        RingPoints(shape, end, b, rm, pts);
        if (t.destKind == DestKind::Skill) {
            dl->AddConvexPolyFilled(pts.data(), shape.segs, col);
            dl->AddPolyline(pts.data(), shape.segs, edge, std::max(1.0f, 1.5f * k), kClosedCentered);
        } else {
            dl->AddPolyline(pts.data(), shape.segs, edge, lineTh + 2.0f, kClosedCentered);
            dl->AddPolyline(pts.data(), shape.segs, col, lineTh, kClosedCentered);
        }
    }
    stats.lastFrameUs = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count();
}

} // namespace Wayg
