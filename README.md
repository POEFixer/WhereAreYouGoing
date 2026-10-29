# Where Are You Going

Current version: 1.0.3

Requires POE2Fixer 364 or newer.

A POEFixer plugin for Path of Exile 2 that draws a ring on the ground around the monsters you care about, coloured
by rarity: rare monsters in yellow and unique monsters in orange-red by default, normal and magic monsters if you
switch them on. With destination lines on, each ringed monster also gets a line to where it is going: the target
of the skill it is using (an attack or a spell), or the next point of the route it is walking.

## How to use

1. Install **Where Are You Going** from the **Plugin Manager**, the first tab under POEFixer's **Plugins** tab
   (POE2Fixer 364 or newer). It is enabled right away, and the Plugin Manager keeps it up to date. The **Enabled**
   box at the top of the plugin's own tab turns it off and on.
2. Enter a map. Rare and unique monsters get a ring around their feet that follows them as they move.
3. To see where monsters are heading, open the plugin's tab and tick **Show where monsters are going**. A filled
   dot at the end of a line marks the target of a skill; a small ring marks where the monster is walking to.

Changes apply at once and are saved automatically.

## Settings

### Rings
One row per rarity (Normal, Magic, Rare, Unique): a tick to ring that rarity, its **Colour** (its transparency makes
the ring see-through), **Size** (half the ring's width on screen) and **Width** (the thickness of its line). Sizes
are pixels at 1080p and grow with the game's resolution. **Default rings** restores the four rows. Friendly
monsters, dead ones and bosses that have not entered the fight yet never get a ring.

### Destination lines
- **Show where monsters are going**: a line from the ring to the monster's destination, in its rarity's colour.
  While the monster uses a skill the line goes to the skill's target, also when it runs at the same time;
  otherwise it goes to the next point of its walking route. A monster standing still has no line, and a
  destination off screen gets a line up to the screen edge.
- **Line width**, **End marker size** (pixels at 1080p).

### When to show
- **Only while the game window is active**, **Hide while the POEFixer menu is open**. Never drawn in towns and
  hideouts.

### Performance
- **Follow monsters every frame**: the rings stay on moving monsters. Off, a ring moves only at each search, which
  costs less.
- **Look for monsters every**: how often new, dead or departed monsters are noticed (250 ms by default).
- **Most rings at once**: above it, unique monsters come first, then rare, magic and normal, the nearest first.
- **Ring smoothness**: points per ring.
- **Default settings** restores every setting of the plugin, after a confirmation.

### Diagnostics
The last search (monsters seen, ringed, time taken), the reads per second and their cost, this frame's drawing,
and a short list of the ringed monsters with where each one is going. **Look again now** searches at once,
**Forget rings** drops every ring until the next search.

## Known limits

- Where a monster is going comes from its current action and its route as the game holds them, read about ten
  times a second, so a line can trail a sudden turn by a moment.
- Ring sizes are fixed on screen: zooming the camera in or out does not resize them.
- A monster that appears is ringed at the next search (250 ms by default).
- Settings live in `config/settings.json` next to the plugin. A settings file from the original plugin is taken
  over as it is (the same keys). A damaged file is kept as `config/settings.json.corrupt` and the defaults are
  used until you change a setting.

## Install

Install it from POE2Fixer → Plugins → Plugin Manager (one click, updates included), or download `WhereAreYouGoing.zip` from [Releases](https://github.com/POEFixer/WhereAreYouGoing/releases/latest) and extract it into the POE2Fixer folder (it creates `Plugins\WhereAreYouGoing\`). Then enable the plugin in the Plugins tab.

## Build from source

This repository is the plugin folder from the POE2Fixer source tree (`Plugins/WhereAreYouGoing/`) together with the host files it compiles against, kept at the same relative paths:

- `POEFixer/plugin_sdk/` - the POE2Fixer Plugin SDK v6 headers (`PluginAbi.h`, `PluginSDK.h`)
- `POEFixer/imgui/` - the Dear ImGui sources the DLL compiles against
- `tools/plugin_i18n/gen.py` - the translation tool: after editing `Plugins/WhereAreYouGoing/lang/en.json` run `python tools/plugin_i18n/gen.py Plugins/WhereAreYouGoing` (it rewrites the embedded English header); `--gate` checks every language file and the keys the sources use

1. Install Visual Studio 2022 with the "Desktop development with C++" workload (MSVC v143, Windows 10/11 SDK).
2. Open `WhereAreYouGoing.sln`, select **Release | x64** and build the solution.
3. The plugin is written to `x64\Release\Plugins\WhereAreYouGoing\WhereAreYouGoing.dll` (Release builds carry no debug information, see `Directory.Build.targets`).
4. Copy `WhereAreYouGoing.dll` into `Plugins\WhereAreYouGoing\` next to the POE2Fixer executable, together with `Plugins\WhereAreYouGoing\lang` from this repository (same folder names).

## Credits

Originally created by KaiEKum (https://github.com/KaiEKum/POEFixerPlugins). Maintained by POEFixer since
October 2026.
