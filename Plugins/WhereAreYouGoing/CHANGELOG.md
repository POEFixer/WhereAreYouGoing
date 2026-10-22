# Changelog

## 1.0.1

- Rebuilt with the updated plugin SDK headers (POEFixer 365 adds optional calls for UI control names, stash objects and tabs, wider entity scans and the host automation flag). No change in what the plugin does.

## 1.0.0

- First release maintained by POEFixer.
- Rings now lie flat on the ground around the monster's feet; before, they were often drawn below the monster.
- Following monsters every frame now reads only the monsters on screen and is on by default, so rings stay on
  moving monsters instead of jumping a few times a second (Performance; a choice saved with the original plugin
  is kept).
- Much lighter on performance: only the monster list is searched, an empty area or a town is no longer searched
  every frame, and the reads per monster are spread over frames, so busy maps no longer slow the game down.
- When there are more monsters than the ring limit, unique and rare monsters and the nearest ones are kept
  instead of whichever came first.
- Destination lines start at the ring's edge and stop at a marker: a filled dot for a skill target, a small ring
  for where the monster walks. A monster standing still no longer gets a line to an old destination, and a
  destination off screen gets a line up to the screen edge.
- Sizes follow the game's resolution, and rings and lines have a thin dark edge so they read on any ground.
- New settings tab in the POEFixer style with explanations on every option, plus options to draw only while the
  game is in front and to hide while the POEFixer menu is open, and a Diagnostics section.
- Bosses that have not entered the fight yet no longer get a ring.
- Settings are saved safely and automatically after each change; your settings from the original plugin are
  taken over, and a damaged settings file is kept instead of being overwritten.

## Original project

Where Are You Going was created by KaiEKum in https://github.com/KaiEKum/POEFixerPlugins (plugins/WhereAreYouGoing).
That project had no version numbers; 1.0.0 is the first version maintained by POEFixer.
