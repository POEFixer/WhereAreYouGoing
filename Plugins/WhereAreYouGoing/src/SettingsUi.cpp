#include "SettingsUi.h"

#include "../../../POEFixer/plugin_sdk/PluginUi.h"
#include "../../../POEFixer/imgui/IconsFontAwesome6.h"

#include <algorithm>
#include <cstdio>

namespace Wayg {

namespace {

using PluginUi::Tip;

// Flow layout the kit has no helper for: the next item goes on the same row when
// it still fits, else on a new row.
void SameLineIfFitsWidth(float itemWidth) {
    ImGui::SameLine();
    if (ImGui::GetContentRegionAvail().x < itemWidth + ImGui::GetStyle().ItemSpacing.x) ImGui::NewLine();
}

const char* const kShowTips[kRarityCount] = {
    "Ring normal (white) monsters.\nBusy maps have many of them: leave it off unless you want to see every monster.",
    "Ring magic (blue) monsters.",
    "Ring rare (yellow) monsters.",
    "Ring unique monsters, bosses included.",
};

void DrawSectionRings(Settings& s, PanelResult& out) {
    PluginUi::BeginCard("###wayg_card_rings");
    PluginUi::Heading(ICON_FA_BULLSEYE, "Rings",
        "Each monster of a ticked rarity gets a ring on the ground around its feet, in the rarity's colour.\n\n"
        "Size is half the ring's width on screen and Width the thickness of its line, both in pixels at 1080p; "
        "they grow with the game's resolution.\n\n"
        "Friendly monsters, dead ones and bosses that have not entered the fight yet never get a ring.");

    const ImGuiStyle& style = ImGui::GetStyle();
    const float fh = ImGui::GetFrameHeight();
    const float em = ImGui::GetFontSize();
    float nameW = ImGui::CalcTextSize("Rarity").x;
    for (int r = 0; r < kRarityCount; ++r)
        nameW = std::max(nameW, fh + style.ItemInnerSpacing.x + ImGui::CalcTextSize(RarityLabel(r)).x);
    const float colourW = std::max(fh, ImGui::CalcTextSize("Colour").x + style.ItemInnerSpacing.x);
    // Sliders as wide as the other cards' (at most 15 em). On a narrow pane Size and
    // Width share one column, one above the other, so each keeps a usable width.
    const float cellPad = style.CellPadding.x * 2.0f;
    const float avail = ImGui::GetContentRegionAvail().x;
    const float twoSliders = (avail - nameW - colourW - cellPad * 4.0f) * 0.5f;
    const bool compact = twoSliders < em * 6.5f;
    const float sliderW = compact ? std::clamp(avail - nameW - colourW - cellPad * 3.0f, em * 4.0f, em * 15.0f)
                                  : std::min(twoSliders, em * 15.0f);
    const float tableX = ImGui::GetCursorPosX();
    float tableW = avail;
    const ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX;
    if (ImGui::BeginTable("###wayg_rarities", compact ? 3 : 4, flags)) {
        ImGui::TableSetupColumn("Rarity", ImGuiTableColumnFlags_WidthFixed, nameW);
        ImGui::TableSetupColumn("Colour", ImGuiTableColumnFlags_WidthFixed, colourW);
        if (compact) {
            ImGui::TableSetupColumn("Size / width", ImGuiTableColumnFlags_WidthFixed, sliderW);
        } else {
            ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, sliderW);
            ImGui::TableSetupColumn("Width", ImGuiTableColumnFlags_WidthFixed, sliderW);
        }
        ImGui::TableHeadersRow();
        for (int r = 0; r < kRarityCount; ++r) {
            RarityStyle& rs = s.rarity[static_cast<size_t>(r)];
            ImGui::PushID(r);
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            if (PluginUi::Check(RarityLabel(r), "on", &rs.enabled, kShowTips[r])) out.changed = out.rescan = true;

            ImGui::BeginDisabled(!rs.enabled);
            ImGui::TableNextColumn();
            ImVec4 c(rs.color.r, rs.color.g, rs.color.b, rs.color.a);
            if (PluginUi::Swatch("###colour", c)) {
                rs.color = Color4{ c.x, c.y, c.z, std::max(c.w, Limits::kAlphaMin) };
                out.changed = true;
            }
            Tip("Ring colour for this rarity; its destination lines use it too.\n"
                "Lower its transparency to make the ring see-through.");

            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            out.changed |= ImGui::SliderFloat("###size", &rs.radius, Limits::kRadiusMin, Limits::kRadiusMax,
                                              compact ? "Size %.0f px" : "%.0f px", ImGuiSliderFlags_AlwaysClamp);
            Tip("Ring size: half the ring's width on screen, in pixels at 1080p.");

            if (!compact) ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            out.changed |= ImGui::SliderFloat("###width", &rs.thickness, Limits::kThicknessMin, Limits::kThicknessMax,
                                              compact ? "Width %.1f px" : "%.1f px", ImGuiSliderFlags_AlwaysClamp);
            Tip("Thickness of the ring's line, in pixels at 1080p.");
            ImGui::EndDisabled();
            ImGui::PopID();
        }
        ImGui::EndTable();
        tableW = ImGui::GetItemRectSize().x;
    }

    ImGui::Spacing();
    const char* resetLabel = ICON_FA_ROTATE_LEFT "  Default rings###wayg_rings_reset";
    // Right-aligned under the table.
    ImGui::SetCursorPosX(tableX + std::max(0.0f, std::min(tableW, avail) - PluginUi::ButtonWidth(resetLabel)));
    if (PluginUi::SubtleButton(resetLabel)) ImGui::OpenPopup("###wayg_rings_confirm");
    Tip("Restore the four rarity rows: rare and unique on in yellow and orange-red, normal and magic off.");
    if (PluginUi::ConfirmPopup("###wayg_rings_confirm",
                               "Restore the default rings? Your colours and sizes for all four rarities are replaced.",
                               "Restore", "Cancel")) {
        s.rarity = DefaultRarities();
        out.changed = out.rescan = true;
    }
    PluginUi::EndCard();
}

void DrawSectionLines(Settings& s, PanelResult& out) {
    PluginUi::BeginCard("###wayg_card_lines");
    PluginUi::Heading(ICON_FA_ROUTE, "Destination lines",
        "A line from a ringed monster's ring to where the monster is going, in its rarity's colour.\n\n"
        "While it uses a skill (an attack or a spell) the line goes to the skill's target, also when it runs at "
        "the same time; a filled dot marks that end. While it only walks, the line goes to the next point of "
        "its route, marked with a ring. A monster standing still has no line.\n\n"
        "A destination off screen gets a line up to the screen edge.");
    out.changed |= PluginUi::Check("Show where monsters are going", "wayg_lines", &s.showLines,
        "Draw a line from each ring on screen to the monster's destination.\n"
        "Costs a few small reads per monster, about ten times a second.");
    ImGui::BeginDisabled(!s.showLines);
    ImGui::Spacing();
    const PluginUi::LabelColumn col{ "Line width", "End marker size" };
    out.changed |= PluginUi::SliderFloatRow(col, "Line width", "###wayg_line_w", &s.lineThickness, Limits::kLineMin,
        Limits::kLineMax, "%.1f px", "Thickness of the destination line, in pixels at 1080p.");
    out.changed |= PluginUi::SliderFloatRow(col, "End marker size", "###wayg_marker", &s.markerRadius,
        Limits::kMarkerMin, Limits::kMarkerMax, "%.0f px",
        "Size of the dot or ring at the line's end: half its width on screen, in pixels at 1080p.");
    ImGui::EndDisabled();
    PluginUi::EndCard();
}

void DrawSectionVisibility(Settings& s, PanelResult& out) {
    PluginUi::BeginCard("###wayg_card_when");
    PluginUi::Heading(ICON_FA_EYE, "When to show");
    out.changed |= PluginUi::Check("Only while the game window is active", "wayg_fg", &s.onlyWhenForeground,
        "Hide the rings while another window (a browser, the desktop) is in front of the game.");
    out.changed |= PluginUi::Check("Hide while the POEFixer menu is open", "wayg_menu", &s.hideWhenMenuVisible,
        "Hide the rings while the POEFixer menu is shown.\n"
        "Leave it off to see changes live while you edit these settings.");
    PluginUi::Hint("Never drawn in towns and hideouts.");
    PluginUi::EndCard();
}

void DrawSectionPerformance(Settings& s, PanelResult& out) {
    PluginUi::BeginCard("###wayg_card_perf");
    PluginUi::Heading(ICON_FA_GAUGE, "Performance",
        "The plugin looks for monsters a few times a second and remembers the ones it rings; drawing them "
        "happens every frame.\n\n"
        "Each search only goes through the monster list, and the reads per monster are spread over frames, "
        "so the game does not stutter even in busy maps.");
    out.changed |= PluginUi::Check("Follow monsters every frame", "wayg_follow", &s.followEveryFrame,
        "Read the position of each ringed monster on screen every frame, so the rings stay on moving monsters.\n"
        "Off: a ring moves only at each search (see Look for monsters every), which costs less.");
    ImGui::Spacing();
    const PluginUi::LabelColumn col{ "Look for monsters every", "Most rings at once", "Ring smoothness" };
    if (PluginUi::SliderIntRow(col, "Look for monsters every", "###wayg_scan", &s.scanIntervalMs, Limits::kScanMin,
            Limits::kScanMax, "%d ms",
            "How often the plugin looks for monsters that appeared, died or left.\n"
            "Shorter finds them sooner and costs a little more."))
        out.changed = true;
    if (PluginUi::SliderIntRow(col, "Most rings at once", "###wayg_max", &s.maxTargets, Limits::kTargetsMin,
            Limits::kTargetsMax, "%d",
            "Upper limit of ringed monsters.\n"
            "Above it, unique monsters come first, then rare, magic and normal, the nearest first."))
        out.changed = out.rescan = true;
    out.changed |= PluginUi::SliderIntRow(col, "Ring smoothness", "###wayg_segments", &s.ringSegments,
        Limits::kSegmentsMin, Limits::kSegmentsMax, "%d points",
        "Points per ring.\nMore points look rounder and cost a little more time each frame.");
    const float slidersRight = ImGui::GetItemRectMax().x;   // screen x where the slider rows end

    ImGui::Spacing();
    const char* resetLabel = ICON_FA_ROTATE_LEFT "  Default settings###wayg_all_reset";
    // Right-aligned under the sliders, as Default rings is under its table.
    const ImVec2 cursor = ImGui::GetCursorScreenPos();
    ImGui::SetCursorScreenPos(ImVec2(std::max(cursor.x, slidersRight - PluginUi::ButtonWidth(resetLabel)), cursor.y));
    if (PluginUi::SubtleButton(resetLabel)) ImGui::OpenPopup("###wayg_all_confirm");
    Tip("Restore every setting of this plugin: rings, destination lines, when to show and performance.");
    if (PluginUi::ConfirmPopup("###wayg_all_confirm",
                               "Restore every setting to its default? Your rings, lines and other choices are replaced.",
                               "Restore", "Cancel")) {
        s = Settings{};
        out.changed = out.rescan = true;
    }
    PluginUi::EndCard();
}

const char* GoingTo(const Target& t, bool linesOn) {
    if (!linesOn) return "-";
    if (!t.onScreen) return "(off screen)";
    switch (t.destKind) {
    case DestKind::Skill: return "Skill target";
    case DestKind::Walk:  return "Walking";
    default:              return "Nowhere";
    }
}

void DrawSectionDiagnostics(PanelContext& pc) {
    if (!PluginUi::BeginDiagnostics("What the plugin finds and what it costs: the last search, reads per second, "
                                    "this frame's drawing and the ringed monsters.", "wayg_diag"))
        return;
    const Settings& s = pc.settings;
    Tracker& tracker = pc.tracker;
    const TrackerStats& st = tracker.Stats();
    char line[256];

    if (tracker.Suppressed()) PluginUi::Hint("Town or hideout: nothing is looked for or drawn here.");
    const double age = tracker.LastScanAge(pc.now);
    if (age < 0.0) {
        PluginUi::Hint("No search yet: the plugin looks for monsters while you are in an area.");
    } else {
        std::snprintf(line, sizeof(line),
                      "Last search %.1f s ago: %d monsters seen, %d of a ticked rarity, %d ringed (%.2f ms).", age,
                      static_cast<int>(st.monstersSeen), static_cast<int>(st.candidates), static_cast<int>(st.kept),
                      st.scanMs);
        PluginUi::Hint(line);
    }
    std::snprintf(line, sizeof(line), "Ringed now: %d unique, %d rare, %d magic, %d normal.",
                  st.keptByRarity[kRarityUnique], st.keptByRarity[kRarityRare], st.keptByRarity[kRarityMagic],
                  st.keptByRarity[kRarityNormal]);
    PluginUi::Hint(line);
    std::snprintf(line, sizeof(line), "This frame: %d rings, %d lines, %d off screen (%.2f ms).", pc.overlay.rings,
                  pc.overlay.lines, pc.overlay.offScreen, pc.overlay.lastFrameUs / 1000.0);
    PluginUi::Hint(line);
    if (s.followEveryFrame)
        std::snprintf(line, sizeof(line), "Positions: %d reads per second, %d not believed (kept the last good one).",
                      st.positionReadsPerSec, st.positionRejectsPerSec);
    else
        std::snprintf(line, sizeof(line), "Positions: taken at each search (Follow monsters every frame is off).");
    PluginUi::Hint(line);
    if (s.showLines)
        std::snprintf(line, sizeof(line),
                      "Destinations: %d reads per second, %d with a readable action; %d skill targets, %d walking "
                      "destinations.",
                      st.destReadsPerSec, st.actionValidPerSec, st.skillTargetsPerSec, st.walkTargetsPerSec);
    else
        std::snprintf(line, sizeof(line), "Destinations: not read (destination lines are off).");
    PluginUi::Hint(line);
    std::snprintf(line, sizeof(line), "Reads cost %.2f ms per second.", st.readMsPerSec);
    PluginUi::Hint(line);

    ImGui::Spacing();
    const std::vector<Target>& targets = tracker.Targets();
    if (targets.empty()) {
        PluginUi::Hint("No ringed monster right now.");
    } else {
        constexpr size_t kRows = 8;
        const ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg |
                                      ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_PadOuterX;
        if (ImGui::BeginTable("###wayg_targets", 4, flags)) {
            const float gap = ImGui::GetStyle().ItemInnerSpacing.x * 2.0f;
            ImGui::TableSetupColumn("Rarity", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("Unique").x + gap);
            ImGui::TableSetupColumn("Distance", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("Distance").x + gap);
            ImGui::TableSetupColumn("On screen", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("On screen").x + gap);
            ImGui::TableSetupColumn("Going to", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableHeadersRow();
            for (size_t i = 0; i < targets.size() && i < kRows; ++i) {
                const Target& t = targets[i];
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(RarityLabel(t.rarity));
                ImGui::TableNextColumn();
                ImGui::Text("%.0f", t.distance);
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(t.onScreen ? "Yes" : "No");
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(GoingTo(t, s.showLines));
            }
            ImGui::EndTable();
        }
        if (targets.size() > kRows) {
            std::snprintf(line, sizeof(line), "The %d most important are listed; %d more are ringed.",
                          static_cast<int>(kRows), static_cast<int>(targets.size() - kRows));
            PluginUi::Hint(line);
        }
    }

    ImGui::Spacing();
    const char* rescanLabel = "Look again now###wayg_rescan";
    const char* forgetLabel = "Forget rings###wayg_forget";
    if (PluginUi::SubtleButton(rescanLabel)) tracker.RequestScan();
    Tip("Search for monsters at once instead of waiting for the next search.");
    SameLineIfFitsWidth(PluginUi::ButtonWidth(forgetLabel));
    if (PluginUi::SubtleButton(forgetLabel)) tracker.Reset();
    Tip("Drop every ring now.\nMonsters still around are found again at the next search.");
    PluginUi::EndDiagnostics();
}

} // namespace

void DrawSettingsPanel(PanelContext& pc, PanelResult& out) {
    if (!pc.hostCompatible) {
        ImGui::PushStyleColor(ImGuiCol_Text, PluginUi::Colors().Warning);
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextUnformatted("This POEFixer version is too old for Where Are You Going: nothing is drawn. "
                               "Update POEFixer to use the plugin.");
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }
    DrawSectionRings(pc.settings, out);
    ImGui::Spacing();
    DrawSectionLines(pc.settings, out);
    ImGui::Spacing();
    DrawSectionVisibility(pc.settings, out);
    ImGui::Spacing();
    DrawSectionPerformance(pc.settings, out);
    ImGui::Spacing();
    DrawSectionDiagnostics(pc);
    ImGui::Spacing();
    PluginUi::Hint("Originally created by KaiEKum. Maintained by POEFixer.");
    ImGui::Spacing();
}

} // namespace Wayg
