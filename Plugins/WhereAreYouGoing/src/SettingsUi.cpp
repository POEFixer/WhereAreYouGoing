#include "SettingsUi.h"

#include "../../../POEFixer/plugin_sdk/PluginUi.h"
#include "../../../POEFixer/imgui/IconsFontAwesome6.h"

#include <sal.h>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <string>

// Every text a user reads is a key of lang/en.json, looked up as the tab is drawn
// (PluginI18n.h, through PluginUi.h); numbers are formatted here into named TrF
// arguments, never by a translation.

namespace Wayg {

namespace {

namespace I18n = PluginSDK::I18n;
using PluginUi::Tip;

// printf for a TrF argument: Num("%.1f", seconds). The format is code, never a translation.
std::string Num(_In_z_ _Printf_format_string_ const char* format, ...) {
    char buf[128];
    va_list args;
    va_start(args, format);
    std::vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    return buf;
}

// A slider's display format from the text of `key`, whose placeholder {name}
// stands for the number ("{px} px"): the text's own '%' are doubled first (ImGui
// reads the format as printf), then {name} becomes `conversion` ("%.0f"), so the
// slider shows the translation and Ctrl+click still edits the bare number.
std::string SliderFormat(const char* key, const char* name, const char* conversion) {
    const std::string text = I18n::Tr(key);
    std::string out;
    out.reserve(text.size() + 8);
    for (const char c : text) {
        out.push_back(c);
        if (c == '%') out.push_back('%');
    }
    const std::string token = std::string("{") + name + "}";
    const size_t at = out.find(token);
    if (at == std::string::npos) return conversion;
    out.replace(at, token.size(), conversion);
    return out;
}

// "<icon>  <text>###<id>": a button label whose ImGui id never changes with the language.
std::string IconLabel(const char* icon, const char* key, const char* id) {
    return std::string(icon) + "  " + I18n::Tr(key) + "###" + id;
}

// The rarity's name in the program language (the game's own word for it).
const char* RarityText(int rarity) {
    static constexpr const char* kKeys[kRarityCount] = {
        I18n::Key("rarity_normal"), I18n::Key("rarity_magic"), I18n::Key("rarity_rare"), I18n::Key("rarity_unique"),
    };
    return rarity >= 0 && rarity < kRarityCount ? I18n::Tr(kKeys[rarity]) : "?";
}

// Flow layout the kit has no helper for: the next item goes on the same row when
// it still fits, else on a new row.
void SameLineIfFitsWidth(float itemWidth) {
    ImGui::SameLine();
    if (ImGui::GetContentRegionAvail().x < itemWidth + ImGui::GetStyle().ItemSpacing.x) ImGui::NewLine();
}

constexpr const char* kShowTips[kRarityCount] = {
    I18n::Key("rings_show_normal_tip"),
    I18n::Key("rings_show_magic_tip"),
    I18n::Key("rings_show_rare_tip"),
    I18n::Key("rings_show_unique_tip"),
};

void DrawSectionRings(Settings& s, PanelResult& out) {
    PluginUi::BeginCard("###wayg_card_rings");
    PluginUi::Heading(ICON_FA_BULLSEYE, I18n::Tr("rings_title"), I18n::Tr("rings_help"));

    const ImGuiStyle& style = ImGui::GetStyle();
    const float fh = ImGui::GetFrameHeight();
    const float em = ImGui::GetFontSize();
    const char* rarityHeader = I18n::Tr("rings_col_rarity");
    const char* colourHeader = I18n::Tr("rings_col_colour");
    float nameW = ImGui::CalcTextSize(rarityHeader).x;
    for (int r = 0; r < kRarityCount; ++r)
        nameW = std::max(nameW, fh + style.ItemInnerSpacing.x + ImGui::CalcTextSize(RarityText(r)).x);
    const float colourW = std::max(fh, ImGui::CalcTextSize(colourHeader).x + style.ItemInnerSpacing.x);
    // Sliders as wide as the other cards' (at most 15 em). On a narrow pane Size and
    // Width share one column, one above the other, so each keeps a usable width.
    const float cellPad = style.CellPadding.x * 2.0f;
    const float avail = ImGui::GetContentRegionAvail().x;
    const float twoSliders = (avail - nameW - colourW - cellPad * 4.0f) * 0.5f;
    const bool compact = twoSliders < em * 6.5f;
    const float sliderW = compact ? std::clamp(avail - nameW - colourW - cellPad * 3.0f, em * 4.0f, em * 15.0f)
                                  : std::min(twoSliders, em * 15.0f);
    const std::string sizeFormat = compact ? SliderFormat(I18n::Key("rings_size_compact"), "px", "%.0f")
                                           : SliderFormat(I18n::Key("unit_px"), "px", "%.0f");
    const std::string widthFormat = compact ? SliderFormat(I18n::Key("rings_width_compact"), "px", "%.1f")
                                            : SliderFormat(I18n::Key("unit_px"), "px", "%.1f");
    const float tableX = ImGui::GetCursorPosX();
    float tableW = avail;
    const ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX;
    if (ImGui::BeginTable("###wayg_rarities", compact ? 3 : 4, flags)) {
        ImGui::TableSetupColumn(I18n::TrId("rings_col_rarity"), ImGuiTableColumnFlags_WidthFixed, nameW);
        ImGui::TableSetupColumn(I18n::TrId("rings_col_colour"), ImGuiTableColumnFlags_WidthFixed, colourW);
        if (compact) {
            ImGui::TableSetupColumn(I18n::TrId("rings_col_size_width"), ImGuiTableColumnFlags_WidthFixed, sliderW);
        } else {
            ImGui::TableSetupColumn(I18n::TrId("rings_col_size"), ImGuiTableColumnFlags_WidthFixed, sliderW);
            ImGui::TableSetupColumn(I18n::TrId("rings_col_width"), ImGuiTableColumnFlags_WidthFixed, sliderW);
        }
        ImGui::TableHeadersRow();
        for (int r = 0; r < kRarityCount; ++r) {
            RarityStyle& rs = s.rarity[static_cast<size_t>(r)];
            ImGui::PushID(r);
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            if (PluginUi::Check(RarityText(r), "on", &rs.enabled, I18n::Tr(kShowTips[r]))) out.changed = out.rescan = true;

            ImGui::BeginDisabled(!rs.enabled);
            ImGui::TableNextColumn();
            ImVec4 c(rs.color.r, rs.color.g, rs.color.b, rs.color.a);
            if (PluginUi::Swatch("###colour", c)) {
                rs.color = Color4{ c.x, c.y, c.z, std::max(c.w, Limits::kAlphaMin) };
                out.changed = true;
            }
            Tip(I18n::Tr("rings_colour_tip"));

            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            out.changed |= ImGui::SliderFloat("###size", &rs.radius, Limits::kRadiusMin, Limits::kRadiusMax,
                                              sizeFormat.c_str(), ImGuiSliderFlags_AlwaysClamp);
            Tip(I18n::Tr("rings_size_tip"));

            if (!compact) ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            out.changed |= ImGui::SliderFloat("###width", &rs.thickness, Limits::kThicknessMin, Limits::kThicknessMax,
                                              widthFormat.c_str(), ImGuiSliderFlags_AlwaysClamp);
            Tip(I18n::Tr("rings_width_tip"));
            ImGui::EndDisabled();
            ImGui::PopID();
        }
        ImGui::EndTable();
        tableW = ImGui::GetItemRectSize().x;
    }

    ImGui::Spacing();
    const std::string resetLabel = IconLabel(ICON_FA_ROTATE_LEFT, I18n::Key("rings_reset"), "wayg_rings_reset");
    // Right-aligned under the table.
    ImGui::SetCursorPosX(tableX + std::max(0.0f, std::min(tableW, avail) - PluginUi::ButtonWidth(resetLabel.c_str())));
    if (PluginUi::SubtleButton(resetLabel.c_str())) ImGui::OpenPopup("###wayg_rings_confirm");
    Tip(I18n::Tr("rings_reset_tip"));
    // "Cancel" is the host's own text (the kit's default).
    if (PluginUi::ConfirmPopup("###wayg_rings_confirm", I18n::Tr("rings_reset_confirm"), I18n::Tr("confirm_restore"))) {
        s.rarity = DefaultRarities();
        out.changed = out.rescan = true;
    }
    PluginUi::EndCard();
}

void DrawSectionLines(Settings& s, PanelResult& out) {
    PluginUi::BeginCard("###wayg_card_lines");
    PluginUi::Heading(ICON_FA_ROUTE, I18n::Tr("lines_title"), I18n::Tr("lines_help"));
    out.changed |= PluginUi::Check(I18n::Tr("lines_show"), "wayg_lines", &s.showLines, I18n::Tr("lines_show_tip"));
    ImGui::BeginDisabled(!s.showLines);
    ImGui::Spacing();
    const char* widthLabel = I18n::Tr("lines_width");
    const char* markerLabel = I18n::Tr("lines_marker");
    const PluginUi::LabelColumn col{ widthLabel, markerLabel };
    out.changed |= PluginUi::SliderFloatRow(col, widthLabel, "###wayg_line_w", &s.lineThickness, Limits::kLineMin,
        Limits::kLineMax, SliderFormat(I18n::Key("unit_px"), "px", "%.1f").c_str(), I18n::Tr("lines_width_tip"));
    out.changed |= PluginUi::SliderFloatRow(col, markerLabel, "###wayg_marker", &s.markerRadius,
        Limits::kMarkerMin, Limits::kMarkerMax, SliderFormat(I18n::Key("unit_px"), "px", "%.0f").c_str(),
        I18n::Tr("lines_marker_tip"));
    ImGui::EndDisabled();
    PluginUi::EndCard();
}

void DrawSectionVisibility(Settings& s, PanelResult& out) {
    PluginUi::BeginCard("###wayg_card_when");
    PluginUi::Heading(ICON_FA_EYE, I18n::Tr("when_title"));
    out.changed |= PluginUi::Check(I18n::Tr("when_foreground"), "wayg_fg", &s.onlyWhenForeground,
                                   I18n::Tr("when_foreground_tip"));
    out.changed |= PluginUi::Check(I18n::Tr("when_hide_menu"), "wayg_menu", &s.hideWhenMenuVisible,
                                   I18n::Tr("when_hide_menu_tip"));
    PluginUi::Hint(I18n::Tr("when_never_in_towns"));
    PluginUi::EndCard();
}

void DrawSectionPerformance(Settings& s, PanelResult& out) {
    PluginUi::BeginCard("###wayg_card_perf");
    PluginUi::Heading(ICON_FA_GAUGE, I18n::Tr("perf_title"), I18n::Tr("perf_help"));
    out.changed |= PluginUi::Check(I18n::Tr("perf_follow"), "wayg_follow", &s.followEveryFrame,
                                   I18n::Tr("perf_follow_tip"));
    ImGui::Spacing();
    const char* scanLabel = I18n::Tr("perf_scan");
    const char* maxLabel = I18n::Tr("perf_max_rings");
    const char* segmentsLabel = I18n::Tr("perf_segments");
    const PluginUi::LabelColumn col{ scanLabel, maxLabel, segmentsLabel };
    if (PluginUi::SliderIntRow(col, scanLabel, "###wayg_scan", &s.scanIntervalMs, Limits::kScanMin, Limits::kScanMax,
                               SliderFormat(I18n::Key("unit_ms"), "ms", "%d").c_str(), I18n::Tr("perf_scan_tip")))
        out.changed = true;
    if (PluginUi::SliderIntRow(col, maxLabel, "###wayg_max", &s.maxTargets, Limits::kTargetsMin, Limits::kTargetsMax,
                               "%d", I18n::Tr("perf_max_rings_tip")))
        out.changed = out.rescan = true;
    out.changed |= PluginUi::SliderIntRow(col, segmentsLabel, "###wayg_segments", &s.ringSegments,
        Limits::kSegmentsMin, Limits::kSegmentsMax, SliderFormat(I18n::Key("perf_segments_points"), "count", "%d").c_str(),
        I18n::Tr("perf_segments_tip"));
    const float slidersRight = ImGui::GetItemRectMax().x;   // screen x where the slider rows end

    ImGui::Spacing();
    const std::string resetLabel = IconLabel(ICON_FA_ROTATE_LEFT, I18n::Key("perf_reset_all"), "wayg_all_reset");
    // Right-aligned under the sliders, as Default rings is under its table.
    const ImVec2 cursor = ImGui::GetCursorScreenPos();
    ImGui::SetCursorScreenPos(
        ImVec2(std::max(cursor.x, slidersRight - PluginUi::ButtonWidth(resetLabel.c_str())), cursor.y));
    if (PluginUi::SubtleButton(resetLabel.c_str())) ImGui::OpenPopup("###wayg_all_confirm");
    Tip(I18n::Tr("perf_reset_all_tip"));
    if (PluginUi::ConfirmPopup("###wayg_all_confirm", I18n::Tr("perf_reset_all_confirm"), I18n::Tr("confirm_restore"))) {
        s = Settings{};
        out.changed = out.rescan = true;
    }
    PluginUi::EndCard();
}

const char* GoingTo(const Target& t, bool linesOn) {
    if (!linesOn) return "-";
    if (!t.onScreen) return I18n::Tr("diag_going_off_screen");
    switch (t.destKind) {
    case DestKind::Skill: return I18n::Tr("diag_going_skill");
    case DestKind::Walk:  return I18n::Tr("diag_going_walk");
    default:              return I18n::Tr("diag_going_nowhere");
    }
}

void DrawSectionDiagnostics(PanelContext& pc) {
    if (!PluginUi::BeginDiagnostics(I18n::Tr("diag_tip"), "wayg_diag"))
        return;
    const Settings& s = pc.settings;
    Tracker& tracker = pc.tracker;
    const TrackerStats& st = tracker.Stats();

    if (tracker.Suppressed()) PluginUi::Hint(I18n::Tr("diag_suppressed"));
    const double age = tracker.LastScanAge(pc.now);
    if (age < 0.0) {
        PluginUi::Hint(I18n::Tr("diag_no_search"));
    } else {
        PluginUi::Hint(I18n::TrF("diag_last_search", { { "age", Num("%.1f", age) },
                                                        { "seen", Num("%d", static_cast<int>(st.monstersSeen)) },
                                                        { "candidates", Num("%d", static_cast<int>(st.candidates)) },
                                                        { "ringed", Num("%d", static_cast<int>(st.kept)) },
                                                        { "ms", Num("%.2f", st.scanMs) } }).c_str());
    }
    PluginUi::Hint(I18n::TrF("diag_ringed_now", { { "unique", Num("%d", st.keptByRarity[kRarityUnique]) },
                                                  { "rare", Num("%d", st.keptByRarity[kRarityRare]) },
                                                  { "magic", Num("%d", st.keptByRarity[kRarityMagic]) },
                                                  { "normal", Num("%d", st.keptByRarity[kRarityNormal]) } }).c_str());
    PluginUi::Hint(I18n::TrF("diag_this_frame", { { "rings", Num("%d", pc.overlay.rings) },
                                                  { "lines", Num("%d", pc.overlay.lines) },
                                                  { "off_screen", Num("%d", pc.overlay.offScreen) },
                                                  { "ms", Num("%.2f", pc.overlay.lastFrameUs / 1000.0) } }).c_str());
    if (s.followEveryFrame)
        PluginUi::Hint(I18n::TrF("diag_positions", { { "reads", Num("%d", st.positionReadsPerSec) },
                                                     { "rejected", Num("%d", st.positionRejectsPerSec) } }).c_str());
    else
        PluginUi::Hint(I18n::Tr("diag_positions_at_search"));
    if (s.showLines)
        PluginUi::Hint(I18n::TrF("diag_destinations", { { "reads", Num("%d", st.destReadsPerSec) },
                                                        { "actions", Num("%d", st.actionValidPerSec) },
                                                        { "skills", Num("%d", st.skillTargetsPerSec) },
                                                        { "walks", Num("%d", st.walkTargetsPerSec) } }).c_str());
    else
        PluginUi::Hint(I18n::Tr("diag_destinations_off"));
    PluginUi::Hint(I18n::TrF("diag_read_cost", { { "ms", Num("%.2f", st.readMsPerSec) } }).c_str());

    ImGui::Spacing();
    const std::vector<Target>& targets = tracker.Targets();
    if (targets.empty()) {
        PluginUi::Hint(I18n::Tr("diag_no_ringed"));
    } else {
        constexpr size_t kRows = 8;
        // Fixed columns of width 0 fit their translated header and cells every frame
        // (the table is not resizable).
        const ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg |
                                      ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_PadOuterX;
        if (ImGui::BeginTable("###wayg_targets", 4, flags)) {
            ImGui::TableSetupColumn(I18n::TrId("rings_col_rarity"), ImGuiTableColumnFlags_WidthFixed, 0.0f);
            ImGui::TableSetupColumn(I18n::TrId("diag_col_distance"), ImGuiTableColumnFlags_WidthFixed, 0.0f);
            ImGui::TableSetupColumn(I18n::TrId("diag_col_on_screen"), ImGuiTableColumnFlags_WidthFixed, 0.0f);
            ImGui::TableSetupColumn(I18n::TrId("diag_col_going_to"), ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableHeadersRow();
            for (size_t i = 0; i < targets.size() && i < kRows; ++i) {
                const Target& t = targets[i];
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(RarityText(t.rarity));
                ImGui::TableNextColumn();
                ImGui::Text("%.0f", t.distance);
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(t.onScreen ? I18n::Tr("diag_yes") : I18n::Tr("diag_no"));
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(GoingTo(t, s.showLines));
            }
            ImGui::EndTable();
        }
        if (targets.size() > kRows) {
            PluginUi::Hint(I18n::TrF("diag_more_ringed", { { "listed", Num("%d", static_cast<int>(kRows)) },
                                                           { "more", Num("%d", static_cast<int>(targets.size() - kRows)) } })
                               .c_str());
        }
    }

    ImGui::Spacing();
    const std::string rescanLabel = std::string(I18n::Tr("diag_rescan")) + "###wayg_rescan";
    const std::string forgetLabel = std::string(I18n::Tr("diag_forget")) + "###wayg_forget";
    if (PluginUi::SubtleButton(rescanLabel.c_str())) tracker.RequestScan();
    Tip(I18n::Tr("diag_rescan_tip"));
    SameLineIfFitsWidth(PluginUi::ButtonWidth(forgetLabel.c_str()));
    if (PluginUi::SubtleButton(forgetLabel.c_str())) tracker.Reset();
    Tip(I18n::Tr("diag_forget_tip"));
    PluginUi::EndDiagnostics();
}

} // namespace

void DrawSettingsPanel(PanelContext& pc, PanelResult& out) {
    if (!pc.hostCompatible) {
        ImGui::PushStyleColor(ImGuiCol_Text, PluginUi::Colors().Warning);
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextUnformatted(I18n::Tr("host_too_old"));
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
