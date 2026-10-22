// Where Are You Going settings: defaults, ranges and JSON persistence in
// <plugin folder>/config/settings.json. The file keeps the original plugin's flat
// keys ("rareEnabled", "rareRadius", "rareColorR", "scanIntervalMs", ...), so a
// file written by the original plugin is read as it is and the original plugin
// can still read ours. No SDK or ImGui dependency.
#pragma once

#include <array>
#include <filesystem>
#include <string>

namespace Wayg {

constexpr int kRarityNormal = 0;
constexpr int kRarityMagic = 1;
constexpr int kRarityRare = 2;
constexpr int kRarityUnique = 3;
constexpr int kRarityCount = 4;

struct Color4 {
    float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;
};

// One rarity's ring. Sizes are pixels at 1080p and grow with the resolution.
struct RarityStyle {
    bool   enabled = false;
    float  radius = 80.0f;      // half the ring's width on screen
    float  thickness = 5.0f;    // line width of the ring
    Color4 color;
};

namespace Limits {
constexpr float kRadiusMin = 8.0f,     kRadiusMax = 120.0f;
constexpr float kThicknessMin = 1.0f,  kThicknessMax = 16.0f;
constexpr float kAlphaMin = 0.10f;     // a ring is never fully transparent
constexpr float kLineMin = 1.0f,       kLineMax = 10.0f;
constexpr float kMarkerMin = 4.0f,     kMarkerMax = 40.0f;
constexpr int   kScanMin = 100,        kScanMax = 2000;
constexpr int   kTargetsMin = 16,      kTargetsMax = 512;
constexpr int   kSegmentsMin = 8,      kSegmentsMax = 48;
}

std::array<RarityStyle, kRarityCount> DefaultRarities();

struct Settings {
    std::array<RarityStyle, kRarityCount> rarity = DefaultRarities();

    // Destination lines
    bool  showLines = false;
    float lineThickness = 2.0f;     // px at 1080p
    float markerRadius = 15.0f;     // px at 1080p: the marker at the line's end

    // When to show
    bool onlyWhenForeground = true;
    bool hideWhenMenuVisible = false;

    // Performance
    bool followEveryFrame = true;   // upstream "useFreshRenderPosition"
    int  scanIntervalMs = 250;
    int  maxTargets = 48;
    int  ringSegments = 18;

    void Clamp();
};

const char* RarityKey(int rarity);     // "normal", "magic", "rare", "unique" (file keys)
const char* RarityLabel(int rarity);   // "Normal", "Magic", "Rare", "Unique"

// Parses a settings object. Returns false when the text is not a JSON object
// (`out` is left alone). `fromUpstream` (optional) is set when the object came
// from the original plugin: it has known keys but no "version".
bool ParseJson(const std::string& text, Settings& out, bool* fromUpstream = nullptr);
std::string ToJson(const Settings& s);

// Json:        our file, read.
// Upstream:    the original plugin's file, read (save once to add the new keys).
// Defaults:    no file yet (first start).
// CorruptJson: the file is not a JSON object; it was renamed to settings.json.corrupt.
//              Defaults are in use; save nothing until the player changes a setting.
// Unreadable:  the file exists but cannot be read now (held open by another program,
//              access denied, oversized). Defaults are in use; never write them over it
//              (see KeepUnreadableCopy).
enum class LoadSource { Json, Upstream, Defaults, CorruptJson, Unreadable };

// `pluginDir` is the folder the DLL lives in; the file is <pluginDir>/config/settings.json.
LoadSource Load(const std::filesystem::path& pluginDir, Settings& out);
bool Save(const std::filesystem::path& pluginDir, const std::string& json);
// Best effort: copies an unreadable settings.json to settings.json.unreadable
// before the first save replaces it.
void KeepUnreadableCopy(const std::filesystem::path& pluginDir);

std::filesystem::path SettingsPath(const std::filesystem::path& pluginDir);

} // namespace Wayg
