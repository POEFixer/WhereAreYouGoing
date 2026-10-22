// In-game drawing: a ring flat on the ground around each target's feet, and with
// destination lines on, a line from the ring to where the monster is going with a
// marker at its end. Sizes are pixels at 1080p and follow the game's resolution;
// every stroke has a thin dark edge so it reads on any ground.
#pragma once

#include "../../../POEFixer/plugin_sdk/PluginSDK.h"
#include "Settings.h"
#include "Tracker.h"

namespace Wayg {

struct OverlayStats {
    int    rings = 0;          // drawn this frame
    int    lines = 0;
    int    offScreen = 0;      // targets whose feet were not on screen
    double lastFrameUs = 0.0;
};

void DrawOverlay(const PluginSDK::Context& ctx, const Settings& s, Tracker& tracker, OverlayStats& stats);

} // namespace Wayg
