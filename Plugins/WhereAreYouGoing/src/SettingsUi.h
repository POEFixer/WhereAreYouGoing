// The Where Are You Going tab in the host's Plugins window. One small function
// per card.
#pragma once

#include "Overlay.h"
#include "Settings.h"
#include "Tracker.h"

namespace Wayg {

struct PanelResult {
    bool changed = false;   // persist once no widget is being edited
    bool rescan = false;    // which monsters qualify changed: look again now
};

struct PanelContext {
    Settings&           settings;
    Tracker&            tracker;
    const OverlayStats& overlay;
    bool                hostCompatible;
    double              now;   // seconds, the plugin's clock
};

void DrawSettingsPanel(PanelContext& pc, PanelResult& out);

} // namespace Wayg
