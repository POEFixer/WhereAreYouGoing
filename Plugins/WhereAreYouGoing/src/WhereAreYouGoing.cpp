// Where Are You Going: rings on the ground around monsters by rarity, and
// optional lines to where each ringed monster is going (the target of the skill
// it uses, else the next point of its walking route).
//
// Originally created by KaiEKum (https://github.com/KaiEKum/POEFixerPlugins).
// Maintained by POEFixer since October 2026.

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "../../../POEFixer/plugin_sdk/PluginSDK.h"
#include "../../../POEFixer/imgui/imgui.h"
#include "../../../POEFixer/imgui/imgui_internal.h"   // ImTextInitClassifiers

#include "Overlay.h"
#include "Settings.h"
#include "SettingsUi.h"
#include "Tracker.h"

#include <chrono>
#include <exception>
#include <new>
#include <string>

// Plugin info: the host checks it before CreatePlugin; the publish tool reads it.
POEFIXER_PLUGIN_INFO(
    "WhereAreYouGoing",
    "Where Are You Going",
    "1.0.2",
    "KaiEKum, POEFixer",
    "Draws a ring on the ground around rare and unique monsters (or any rarity you pick) and can show a line to where each one is going.")

namespace {

using namespace Wayg;

class WhereAreYouGoingPlugin final : public PluginSDK::Plugin {
public:
    const char* GetName() const override { return "WhereAreYouGoing"; }

    void OnEnable(bool /*isGameAttached*/) override {
        BindImGui();
        LoadSettings();
        m_Tracker.Reset();
        m_Start = std::chrono::steady_clock::now();
        m_Enabled = true;
    }

    void OnDisable() override {
        SaveSettings();
        m_Tracker.Reset();
        m_Enabled = false;
    }

    bool WantsOverlay() const override { return true; }

    void DrawUI() override {
        if (!m_Enabled || !HostCompatible() || !BindImGui()) return;
        RetryBlockedLoad();
        const PluginSDK::Context& c = *ctx();
        if (!c.Game.IsAttached() || !c.Game.IsInGame()) {
            m_Tracker.Reset();   // cheap when already empty; area data never survives a loading screen
            m_OverlayStats = OverlayStats{};
            return;
        }
        // Keep searching while hidden so the rings are current the moment they show
        // again; the per-frame reads stop, since nothing is on screen.
        m_Tracker.Tick(c, m_Settings, Now());
        const bool hidden = (m_Settings.onlyWhenForeground && !c.Game.IsForeground()) ||
                            (m_Settings.hideWhenMenuVisible && c.Game.IsMenuVisible());
        if (hidden) {
            m_Tracker.MarkAllOffScreen();
            m_OverlayStats = OverlayStats{};
            return;
        }
        DrawOverlay(c, m_Settings, m_Tracker, m_OverlayStats);
    }

    void DrawSettings() override {
        if (!BindImGui()) return;
        RetryBlockedLoad();
        PanelContext pc{ m_Settings, m_Tracker, m_OverlayStats, HostCompatible(), Now() };
        PanelResult result;
        DrawSettingsPanel(pc, result);
        if (result.changed) {
            m_Settings.Clamp();
            m_Dirty = true;
        }
        if (result.rescan) m_Tracker.RequestScan();
        // Save once the edit is finished, not on every frame of a slider drag.
        if (m_Dirty && !ImGui::IsAnyItemActive()) {
            SaveSettings();
            m_Dirty = false;
        }
    }

    void SaveSettings() override {
        if (!m_Loaded) return;   // never overwrite the file with defaults before it was read
        try {
            Settings toSave = m_Settings;
            toSave.Clamp();
            std::string json = ToJson(toSave);
            if (json == m_LastSaved) return;
            // The file could not be read at load: the player changed a setting
            // since, so it is replaced now, after keeping a copy of the old one.
            if (m_LoadBlocked) {
                m_LoadRetriesLeft = 0;   // the player's edit wins over a later read
                KeepUnreadableCopy(DirectoryPath());
            }
            if (Save(DirectoryPath(), json)) {
                m_LastSaved = std::move(json);
                if (m_LoadBlocked) {
                    m_LoadBlocked = false;
                    ctx()->Log.Warn("Where Are You Going: replaced the unreadable config/settings.json after a "
                                    "settings change (a copy was kept as .unreadable where possible)");
                }
            }
        } catch (const std::exception&) {
            // Out of memory while serialising: keep the previous file.
        }
    }

private:
    bool BindImGui() const {
        if (ctx()->ImGuiContext) ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx()->ImGuiContext));
        if (!ImGui::GetCurrentContext()) return false;
        // ImGui 1.93 keeps its word-wrap character table in a static of each ImGui
        // copy, filled only when that copy builds a font atlas. This DLL never
        // does (it draws with the host's context), so without this every wrapped
        // text and tooltip breaks mid-word. Cheap: returns at once when filled.
        ImTextInitClassifiers();
        return true;
    }

    double Now() const {
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - m_Start).count();
    }

    void LoadSettings() {
        m_LoadBlocked = false;
        m_LoadRetriesLeft = kLoadRetries;
        m_LoadRetryAt = std::chrono::steady_clock::now() + kLoadRetryEvery;
        try {
            Settings loaded;
            const LoadSource src = Load(DirectoryPath(), loaded);
            AdoptLoaded(src, std::move(loaded));
        } catch (const std::exception&) {
            m_Settings = Settings{};
            m_LastSaved = ToJson(m_Settings);   // nothing is written until a setting changes
            m_Loaded = true;
        }
    }

    void AdoptLoaded(LoadSource src, Settings&& loaded) {
        m_Settings = std::move(loaded);
        m_Loaded = true;
        switch (src) {
        case LoadSource::Json:
            m_LastSaved = ToJson(m_Settings);
            break;
        case LoadSource::Unreadable:
            // Defaults in memory, but nothing is written until a setting changes:
            // the file is the player's and is read again shortly.
            m_LoadBlocked = true;
            m_LastSaved = ToJson(m_Settings);
            ctx()->Log.Warn("Where Are You Going: config/settings.json exists but could not be read (another "
                            "program may hold it); using defaults without overwriting it, reading it again shortly");
            break;
        case LoadSource::CorruptJson:
            // The damaged file is kept as settings.json.corrupt; defaults in memory,
            // and nothing is written until the player changes a setting.
            m_LastSaved = ToJson(m_Settings);
            ctx()->Log.Warn("Where Are You Going: config/settings.json was damaged; kept as settings.json.corrupt, "
                            "defaults in use until a setting is changed");
            break;
        case LoadSource::Upstream:
            // The original plugin's file: same keys, so write it once to add ours.
            m_LastSaved.clear();
            ctx()->Log.Info("Where Are You Going: settings of the original plugin taken over");
            SaveSettings();
            break;
        case LoadSource::Defaults:
        default:
            m_LastSaved.clear();   // first start: write the defaults so the file can be edited
            SaveSettings();
            break;
        }
    }

    // A settings file that was locked at load is read again a few times; the
    // read is a small file every few seconds, and stops once a setting changes.
    void RetryBlockedLoad() {
        if (!m_LoadBlocked || m_LoadRetriesLeft <= 0 || m_Dirty) return;
        const auto now = std::chrono::steady_clock::now();
        if (now < m_LoadRetryAt) return;
        m_LoadRetryAt = now + kLoadRetryEvery;
        --m_LoadRetriesLeft;
        try {
            Settings loaded;
            const LoadSource src = Load(DirectoryPath(), loaded);
            if (src == LoadSource::Unreadable) {
                if (m_LoadRetriesLeft == 0)
                    ctx()->Log.Warn("Where Are You Going: config/settings.json is still unreadable; defaults stay "
                                    "in use and the file is replaced only if a setting is changed");
                return;
            }
            m_LoadBlocked = false;
            AdoptLoaded(src, std::move(loaded));
            m_Tracker.RequestScan();
            if (src == LoadSource::Json)
                ctx()->Log.Info("Where Are You Going: config/settings.json read on a later attempt");
        } catch (const std::exception&) {
            // Out of memory: keep the defaults and try again later.
        }
    }

    static constexpr int kLoadRetries = 15;
    static constexpr std::chrono::seconds kLoadRetryEvery{ 2 };

    Settings     m_Settings;
    Tracker      m_Tracker;
    OverlayStats m_OverlayStats;
    std::chrono::steady_clock::time_point m_Start = std::chrono::steady_clock::now();
    std::string  m_LastSaved;
    bool         m_Loaded = false;
    bool         m_LoadBlocked = false;   // the file exists but was unreadable at load
    int          m_LoadRetriesLeft = 0;
    std::chrono::steady_clock::time_point m_LoadRetryAt{};
    bool         m_Dirty = false;
    bool         m_Enabled = false;
};

} // namespace

extern "C" PLUGIN_API PluginSDK::Plugin* CreatePlugin() {
    return new (std::nothrow) WhereAreYouGoingPlugin();
}

extern "C" PLUGIN_API void DestroyPlugin(PluginSDK::Plugin* plugin) {
    delete plugin;
}
