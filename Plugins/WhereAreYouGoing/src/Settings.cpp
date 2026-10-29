#include "Settings.h"

#include "AtomicFile.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>

namespace Wayg {

using nlohmann::json;

namespace {

constexpr const char* kFileName = "settings.json";
constexpr int kFileVersion = 2;   // the original plugin wrote no version (= 1)

float Clamp01(float v) { return (v == v) ? std::clamp(v, 0.0f, 1.0f) : 1.0f; }

float ClampF(float v, float lo, float hi, float fallback) {
    if (!std::isfinite(v)) return fallback;
    return std::clamp(v, lo, hi);
}

// Four decimals are plenty for sizes and colours, and keep the file readable
// ("0.95" instead of the float's "0.949999988079071").
double Tidy(float v) { return std::round(static_cast<double>(v) * 10000.0) / 10000.0; }

std::string Key(int rarity, const char* field) { return std::string(RarityKey(rarity)) + field; }

// ---- field readers that never throw: a key with the wrong type is ignored ----
bool ReadBool(const json& j, const std::string& key, bool& v) {
    auto it = j.find(key);
    if (it == j.end() || !it->is_boolean()) return false;
    v = it->get<bool>();
    return true;
}
bool ReadFloat(const json& j, const std::string& key, float& v) {
    auto it = j.find(key);
    if (it == j.end() || !it->is_number()) return false;
    v = static_cast<float>(it->get<double>());
    return true;
}
bool ReadInt(const json& j, const std::string& key, int& v) {
    auto it = j.find(key);
    if (it == j.end() || !it->is_number()) return false;
    const double d = it->get<double>();
    if (!std::isfinite(d)) return false;
    v = static_cast<int>(std::lround(std::clamp(d, -1.0e9, 1.0e9)));
    return true;
}

// Every key the original plugin wrote (and the two legacy ones it still read).
bool HasUpstreamKey(const json& j) {
    static const char* const kGlobal[] = { "showMovementLines", "movementLineMultiplier", "movementLineThickness",
                                           "destinationMarkerRadius", "useFreshRenderPosition", "scanIntervalMs",
                                           "maxTargets", "ringSegments", "alpha" };
    for (const char* k : kGlobal)
        if (j.contains(k)) return true;
    static const char* const kFields[] = { "Enabled", "Radius", "Thickness", "ColorR", "ColorG", "ColorB", "ColorA" };
    static const char* const kLegacyShow[] = { "showNormal", "showMagic", "showRare", "showUnique" };
    for (int r = 0; r < kRarityCount; ++r) {
        if (j.contains(kLegacyShow[r])) return true;
        for (const char* f : kFields)
            if (j.contains(Key(r, f))) return true;
    }
    return false;
}

} // namespace

std::array<RarityStyle, kRarityCount> DefaultRarities() {
    return { {
        RarityStyle{ false, 80.0f, 5.0f, Color4{ 0.92f, 0.92f, 0.88f, 0.95f } },   // normal: off
        RarityStyle{ false, 80.0f, 5.0f, Color4{ 0.25f, 0.55f, 1.00f, 0.95f } },   // magic: off
        RarityStyle{ true,  80.0f, 5.0f, Color4{ 1.00f, 0.86f, 0.10f, 0.95f } },   // rare: yellow
        RarityStyle{ true,  80.0f, 5.0f, Color4{ 1.00f, 0.28f, 0.08f, 0.95f } },   // unique: orange-red
    } };
}

const char* RarityKey(int rarity) {
    switch (rarity) {
    case kRarityNormal: return "normal";
    case kRarityMagic:  return "magic";
    case kRarityRare:   return "rare";
    case kRarityUnique: return "unique";
    default:            return "unknown";
    }
}

void Settings::Clamp() {
    const Settings d;
    for (int r = 0; r < kRarityCount; ++r) {
        RarityStyle& st = rarity[static_cast<size_t>(r)];
        const RarityStyle& def = d.rarity[static_cast<size_t>(r)];
        st.radius = ClampF(st.radius, Limits::kRadiusMin, Limits::kRadiusMax, def.radius);
        st.thickness = ClampF(st.thickness, Limits::kThicknessMin, Limits::kThicknessMax, def.thickness);
        st.color.r = Clamp01(st.color.r);
        st.color.g = Clamp01(st.color.g);
        st.color.b = Clamp01(st.color.b);
        st.color.a = ClampF(st.color.a, Limits::kAlphaMin, 1.0f, def.color.a);
    }
    lineThickness = ClampF(lineThickness, Limits::kLineMin, Limits::kLineMax, d.lineThickness);
    markerRadius = ClampF(markerRadius, Limits::kMarkerMin, Limits::kMarkerMax, d.markerRadius);
    scanIntervalMs = std::clamp(scanIntervalMs, Limits::kScanMin, Limits::kScanMax);
    maxTargets = std::clamp(maxTargets, Limits::kTargetsMin, Limits::kTargetsMax);
    ringSegments = std::clamp(ringSegments, Limits::kSegmentsMin, Limits::kSegmentsMax);
}

bool ParseJson(const std::string& text, Settings& out, bool* fromUpstream) {
    json j = json::parse(text, nullptr, /*allow_exceptions=*/false, /*ignore_comments=*/true);
    if (j.is_discarded() || !j.is_object()) return false;

    static const char* const kLegacyShow[] = { "showNormal", "showMagic", "showRare", "showUnique" };
    Settings s;
    // The original plugin's "alpha" applied to every rarity without its own ColorA.
    float alpha = 0.0f;
    const bool haveAlpha = ReadFloat(j, "alpha", alpha);
    for (int r = 0; r < kRarityCount; ++r) {
        RarityStyle& st = s.rarity[static_cast<size_t>(r)];
        if (!ReadBool(j, Key(r, "Enabled"), st.enabled)) ReadBool(j, kLegacyShow[r], st.enabled);
        ReadFloat(j, Key(r, "Radius"), st.radius);
        ReadFloat(j, Key(r, "Thickness"), st.thickness);
        ReadFloat(j, Key(r, "ColorR"), st.color.r);
        ReadFloat(j, Key(r, "ColorG"), st.color.g);
        ReadFloat(j, Key(r, "ColorB"), st.color.b);
        if (!ReadFloat(j, Key(r, "ColorA"), st.color.a) && haveAlpha) st.color.a = alpha;
    }
    ReadBool(j, "showMovementLines", s.showLines);
    ReadFloat(j, "movementLineThickness", s.lineThickness);
    ReadFloat(j, "destinationMarkerRadius", s.markerRadius);
    ReadBool(j, "onlyWhenForeground", s.onlyWhenForeground);
    ReadBool(j, "hideWhenMenuVisible", s.hideWhenMenuVisible);
    ReadBool(j, "useFreshRenderPosition", s.followEveryFrame);
    ReadInt(j, "scanIntervalMs", s.scanIntervalMs);
    ReadInt(j, "maxTargets", s.maxTargets);
    ReadInt(j, "ringSegments", s.ringSegments);
    // "movementLineMultiplier" was written by the original plugin but never used: ignored.
    s.Clamp();

    if (fromUpstream) *fromUpstream = !j.contains("version") && HasUpstreamKey(j);
    out = s;
    return true;
}

std::string ToJson(const Settings& s) {
    nlohmann::ordered_json j;
    j["version"] = kFileVersion;
    for (int r = 0; r < kRarityCount; ++r) {
        const RarityStyle& st = s.rarity[static_cast<size_t>(r)];
        j[Key(r, "Enabled")] = st.enabled;
        j[Key(r, "Radius")] = Tidy(st.radius);
        j[Key(r, "Thickness")] = Tidy(st.thickness);
        j[Key(r, "ColorR")] = Tidy(st.color.r);
        j[Key(r, "ColorG")] = Tidy(st.color.g);
        j[Key(r, "ColorB")] = Tidy(st.color.b);
        j[Key(r, "ColorA")] = Tidy(st.color.a);
    }
    j["showMovementLines"] = s.showLines;
    j["movementLineThickness"] = Tidy(s.lineThickness);
    j["destinationMarkerRadius"] = Tidy(s.markerRadius);
    j["onlyWhenForeground"] = s.onlyWhenForeground;
    j["hideWhenMenuVisible"] = s.hideWhenMenuVisible;
    j["useFreshRenderPosition"] = s.followEveryFrame;
    j["scanIntervalMs"] = s.scanIntervalMs;
    j["maxTargets"] = s.maxTargets;
    j["ringSegments"] = s.ringSegments;
    return j.dump(2, ' ', false, nlohmann::ordered_json::error_handler_t::replace) + "\n";
}

std::filesystem::path SettingsPath(const std::filesystem::path& pluginDir) {
    return pluginDir / L"config" / kFileName;
}

LoadSource Load(const std::filesystem::path& pluginDir, Settings& out) {
    out = Settings{};
    if (pluginDir.empty()) return LoadSource::Defaults;
    const std::filesystem::path path = SettingsPath(pluginDir);
    std::string text;
    if (AtomicFile::ReadAll(path, text)) {
        bool upstream = false;
        Settings parsed;
        if (ParseJson(text, parsed, &upstream)) {
            out = parsed;
            return upstream ? LoadSource::Upstream : LoadSource::Json;
        }
        // Damaged: keep it for the player under another name instead of overwriting it.
        std::filesystem::path aside = path;
        aside += L".corrupt";
        ::MoveFileExW(path.c_str(), aside.c_str(), MOVEFILE_REPLACE_EXISTING);
        return LoadSource::CorruptJson;
    }
    // Present but not readable (locked, denied, oversized): not a first start.
    if (AtomicFile::Present(path)) return LoadSource::Unreadable;
    return LoadSource::Defaults;
}

void KeepUnreadableCopy(const std::filesystem::path& pluginDir) {
    if (pluginDir.empty()) return;
    const std::filesystem::path path = SettingsPath(pluginDir);
    std::filesystem::path aside = path;
    aside += L".unreadable";
    ::CopyFileW(path.c_str(), aside.c_str(), FALSE);
}

bool Save(const std::filesystem::path& pluginDir, const std::string& jsonText) {
    if (pluginDir.empty()) return false;
    return AtomicFile::WriteAll(SettingsPath(pluginDir), jsonText);
}

} // namespace Wayg
