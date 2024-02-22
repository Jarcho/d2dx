Version 0.103.3 (2024-02-21)
============================

Bug fixes
---------

* Fix the mouse wheel not working depending on the cursor's position.
* Fix window size limits only taking the primary monitor's resolution into account. Now uses whichever monitor the window is currently on.
* Fix full screen only working on the primary monitor. Now uses whichever monitor the window is currently on.
* Fix the game's window being moved incorrectly when placed on a monitor either above or to the left of the primary monitor.
* Remember the window's position before entering full screen mode rather than always returning to the center of the primary monitor.
* The `position` config in `d2dx.cfg` can now be used to set a negative value to support position left and above the primary monitor. `[-1, -1]` is still used as a special value to mean the center of the primary monitor.

Other
-----

* The `position` config in `d2dx.cfg` now refers to the center of the window, not the top left.

Version 0.103.2 (2024-02-19)
============================

Performance
-----------

* Don't submit draw calls which aren't on the screen. Can be a small, but noticeable improvement.

Bug fixes
---------

* Don't reset the window position on resolution changes when a window position is set in `d2dx.cfg`, but the window has been moved after launch.

Other
-----

* Update d2fps to v1.0.2. Fixes an occasional graphical glitch when rendering rain.

Version 0.103.1 (2024-01-20)
============================

Bug fixes
---------

* Stabilize cursor position changes with non-integer window scale factors. (e.g. weapon switching would 'adjusts' the cursor position to the same spot, but d2dx would cause it to drift)
* Fix cursor adjustment positions in windowed mode. (e.g. the cursor would slowly drift when opening and closing panels)

Other
-----

* Update d2fps to v1.0.1. Improves snow rendering and mod compatibility.

Version 0.103.0 (2024-01-08)
============================

New features
------------

* Anti-aliasing support added for versions `1.07`, `1.08`, `1.09a`, `1.09b`, `1.14c`.
* Anti-aliasing now works on inventory items.
* Weather motion smoothing extended to all versions supported by d2fps. With this there is no longer any motion smoothing code left in D2DX.
* Add a new option `noframetearing` to prevent frame tearing even when vsync is disabled. This defaults to `true`
  * To work properly with variable refresh rates this might need to be disabled.

Other
-----

* Switch to a custom Sgd2FreeRes version.
  * Fixes the Arreat Summit background.
  * Improved rendering performance. Can up to double the frame rate.

Version 0.102.1 (2023-11-16)
============================

Bug fixes
---------

* Fix rounding error in the frame limiter that sometimes caused extra frames to be rendered (frame rate and cpu speed dependent).

Version 0.102.0 (2023-11-14)
============================

New features
------------

* Add motion smoothing to the background of the Arcane Sanctuary.
* Add motion smoothing to missiles.

Bug fixes
---------

* Don't mute the music when the window is repositioned.
* Don't let button states get stuck when the window loses focus.
* Fix video rendering on v1.00—v1.01.
* Fix animation speed of the cursor on higher frame rates.
* Fix animation speed of the clouds in the Arreat Summit on higher frame rates.
* Fix motion smoothing when switching from 25fps to other frame rates (e.g. switching ).

Changes
-------

* Allow window scale to be a floating point number.

Version 0.101.2 (2023-10-05)
============================

Bug fixes
---------

* Fix crash when vsync is disabled and compatibility mode is enabled.
* Fix loading D2FPS feature flags from the config file.
* Fix compatibility mode sometimes being detected when it's not enabled.

Version 0.101.1 (2023-10-02)
============================

Bug fixes
---------

* Don't move the cursor when it's positioned on the bottom row of the window's pixels.

Version 0.101.0 (2023-10-01)
============================

New features
------------

* Motion smoothing and FPS unlocking has been rewritten and implemented as external module (useable by any other renderer)
  * Perspective mode is notably improved. The distortion it applies based on a unit's position is now recalculated for the unit's displayed position rather than the position from the game's last update.
  * Almost all Diablo II versions are now supported starting from v1.00.
* Resolution mod is now loaded on v1.14c

Changes
-------

* V-sync is disabled by default in favour of a frame limiter. It can be enabled if you really want it. (If you have an old config file it will still be followed)

Bug fixes
---------

* Character animations in the menu no longer run faster than normal.
* CPU use in game and menus with V-sync off has been fixed
* CPU use when the game is minimized has been fixed.
* Text label drift when moving with motion smoothing enabled has been fixed.
* Text label jiggling with motion smoothing enabled has been fixed. (The game still sometimes sucks at ordering item labels and can cause them to be rearranged at a very fast rate when moving)
* Unit cursor detection is now done with the unit's displayed position rather than the position from the game's last update. Note this is a small a effect (a few pixels at most).
* Don't mute the music when reactivating the game's window.

Version 0.100.2 (2023-05-19)
============================

Bug fixes
---------

* Fix cursor behaviour when the render area's aspect ratio is not 4x3.
* Fix cursor jumping when opening the left and right panels when the game started in fullscreen and the switched to windowed mode.

Version 0.100.1 (2023-05-08)
============================

New features
------------

* Don't let the game minimize when the window selection changes

Bug fixes
---------

* Fix cursor locking area in window mode when the render area is smaller than the window size.
* Don't make the window wider than it needs to be when the monitor's height is exceeded.

Version 0.100.0 (2023-04-09)
============================

New features
------------

* Two new upscaling methods:
  * `Nearest Neighbour`: Uses the closest matching pixel. Looks sharp and pixilated.
  * `Rasterize`: Rasterizes the game at the target resolution. Slightly better pixel selection while still looking relatively sharp. Helps with perspective mode.
* Bilinear filtering when rendering textures. This is requested by the game when rendering most ground textures, but used to be ignored. This is especially helpful in perspective mode when the ground textures get distorted.
* New `bilinear-sharpness` configuration option to control how sharp the filtering of the new bilinear texture filter should be. Higher values will render closer to the old version.
* Display cutscenes using the full size of the window instead of being boxed into a 4x3 area.
* Add `nokeepaspectratio` option to stretch the game to the window.
* Update `SGD2FreeRes` to version `3.0.3.1`. This adds support for `v1.10` and `v1.12a`; and fixes the issue with black tiles appearing around the border of the screen.
* Implement full motion prediction for `v1.10`.
* Implement text motion prediction for `v1.09` and `v1.12`.

Performance
-----------

* Fix the small, but regular frame stutter experienced by some people.
* Fix the frame stutter that occurs at higher frame rates in certain areas. e.g "The River of Flame" and various parts of Act 5.

Bug fixes
---------

* Fix text labels jiggling occasionally, especially when there are a large number of them.
* Fix cursor entering the black part of the window in fullscreen mode when cursor locking is enabled.
* Only lock the cursor once the mouse has clicked the window.
* Fix crash on shutdown when motion prediction is disabled.
* Fix gradual slowdown over time when motion prediction is enabled.
* Fix inaccurately drawn lines.

Version 0.99.529
================

* Add motion prediction for 1.09d, complete except for hovering text (it's coming).
* Fix low fps in the menus for 1.09d.

Version 0.99.527b
=================

* Add 'filtering' option in cfg file, which allows using bilinear filtering or Catmull-Rom for scaling the game,
    for those who prefer the softer look over the default integer-scale/sharp-bilinear.
* Fix artifacts when vsync is off.

Version 0.99.526b
=================

* Fix motion-predicted texts looking corrupted/being positioned wrong.

Version 0.99.525
================

* Fix motion prediction of shadows not working for some units.
* Fix window size when window is scaled beyond the desktop dimensions.
* Fix some black text in old versions of MedianXL.
* Remove -dxtestsleepfix, this is now enabled by default (along with the fps fix).

Version 0.99.521
================

* Fix low fps in menu for 1.13d with basemod.
* Fix low fps for 1.13c and 1.14 with basemod.
* Fix basemod compatibility so that "BypassFPS" can be enabled without ill effects.

Version 0.99.519
================

* Unlock FPS in menus. (Known issue: char select screen animates too fast)
* Add experimental "sleep fix" to reduce microstutter in-game. Can be enabled with -dxtestsleepfix.
    Let me know if you notice any improvements to overall smoothness in-game, or any problems!

Version 0.99.518b
=================

* Fix size of window being larger than desktop area with -dxscaleN.

Version 0.99.517
================

* Fix in-game state detection, causing DX logo to show in-game and other issues.

Version 0.99.516
================

* High FPS (motion prediction) is now default enabled on supported game versions (1.12, 1.13c, 1.13d and 1.14d).
* Fix crash when trying to host TCP/IP game.

Version 0.99.512c
=================

* Add "frameless" window option in cfg file, for hiding the window frame.
* Fix corrupt graphics in low lighting detail mode.
* Fix corrupt graphics in perspective mode.
* Fix distorted automap cross.
* Fix mouse sometimes getting stuck on the edge of the screen when setting a custom resolution in the cfg file.

Version 0.99.511
================

* Change resolution mod from D2HD to the newer SGD2FreeRes (both by Mir Drualga).
    Custom resolutions now work in 1.09 and 1.14d, but (at this time) there is no support for 1.12. Let me know if this is a problem!
* Some performance optimizations.
* Remove sizing handles on the window (this was never intended).

Version 0.99.510
================

* Add the possibility to set a custom in-game resolution in d2dx.cfg. See the wiki for details.
* Remove special case for MedianXL Sigma (don't limit to 1024x768).

Version 0.99.508
================

* Fix motion prediction of water splashes e.g. in Kurast Docks.

Version 0.99.507
================

* Add motion prediction to weather.
* Improve visual quality of weather a bit (currently only with -dxtestmop).
* Don't block Project Diablo 2 when detected.

Version 0.99.506
================

* Fix crash sometimes happening when using a town portal.
* Add motion prediction to missiles.

Version 0.99.505b
=================

* Fix bug causing crash when using config file.
* Add option to set window position in config file. (default is to center)
* Update: Fix tracking of belt items and auras.
* Update: Fix teleportation causing "drift" in motion prediction.

Version 0.99.504
================

* Revamp configuration file support. NOTE that the old d2dx.cfg format has changed! See d2dx-defaults.cfg for instructions.

Version 0.99.503
================

* Add -dxnotitlechange option to leave window title alone. [patch by Xenthalon]
* Fix -dxscale2/3 not being applied correctly. [patch by Xenthalon]
* Improve the WIP -dxtestmop mode. Now handles movement of all units, not just the player.

Version 0.99.430b
=================

* Add experimental motion prediction ("smooth movement") feature. This gives actual in-game fps above 25. It is a work in progress, see
    the Wiki page (<https://github.com/bolrog/d2dx/wiki/Motion-Prediction>) for info on how to enable it.
* Updated: fix some glitches.

Version 0.99.429
================

* Fix AA being applied on merc portraits, and on text (again).

Version 0.99.428
================

* Fix AA sometimes being applied to the interior of text.

Version 0.99.423b
=================

* Fix high CPU usage.
* Improve caching.
* Remove registry fix (no longer needed).
* Updated: Fix AA being applied to some UI elements (it should not).
* Updated: Fix d2dx logo.

Version 0.99.422
================

* Fix missing stars in Arcane Sanctuary.
* Fix AA behind some transparent effects, e.g. character behind aura fx.
* Add -dxnocompatmodefix command line option (can be used in cases where other mods require XP compat mode).

Version 0.99.419
================

* Fix issue where "tooltip" didn't pop up immediately after placing an item in the inventory.
* Add support for cfg file (named d2dx.cfg, should contain cmdline args including '-').
* Further limit where AA can possibly be applied (only on relevant edges in-game and exclude UI surfaces).
* Performance optimizations.

Version 0.99.415
================

* Add fps fix that greatly smoothes out frame pacing and mouse cursor movement. Can be disabled with -dxnofpsfix.
* Improve color precision a bit on devices that support 10 bits per channel (this is not HDR, just reduces precision loss from in-game lighting/gamma).
* To improve bug reports, a log file (d2dx_log.txt) will be written to the Diablo II folder.

Version 0.99.414
================

* The mouse cursor is now confined to the game window by default. Disable with -dxnoclipcursor.
* Finish AA implementation, giving improved quality.
* Reverted behavior: the mouse cursor will now "jump" when opening inventory like in the unmodded game. This avoids the character suddenly changing movement direction if holding LMB while opening the inventory.
* Fix issue where D2DX was not able to turn off XP (etc) compatibility mode.
* Fix issue where D2DX used the DX 10.1 feature level by default even on better GPUs.
* Fix misc bugs.

Version 0.99.413
================

* Turn off XP compatibility mode for the game executables. It is not necessary with D2DX and causes issues like graphics corruption.
* Fix initial window size erroneously being 640x480 during intro FMVs.

Version 0.99.412
================

* Added (tasteful) FXAA, which is enabled by default since it looked so nice. (It doesn't destroy any detail.)

Version 0.99.411
================

* D2DX should now work on DirectX 10/10.1 graphics cards.

Version 0.99.410
================

* Improved non-integer scaling quality, using "anti-aliased nearest" sampling (similar to sharp-bilinear).
* Specifying -dxnowide will now select a custom screen mode that gives integer scaling, but with ~4:3 aspect ratio.
* Added -dxnoresmod option, which turns off the built-in SlashDiablo-HD (no custom resolutions).
* (For res mod authors) Tuned configurator API to disable the built-in SlashDiablo-HD automatically when used.
* Other internal improvements.

Version 0.99.408
================

* Fix window size being squashed vertically after alt-entering from fullscreen mode.
* Fix crash when running on Windows 7.

Version 0.99.407
================

* For widescreen mode, select 720p rather than 480p in-game resolution on 1440p monitors.

Version 0.99.406
================

* Fix bug that could crash the game in the Video Options menu.

Version 0.99.405
================

* Simplify installation by removing the need to copy SlashDiablo-HD/D2HD DLL and MPQ files.
    Only glide3x.dll needs to be copied to the Diablo II folder.

Version 0.99.403b
=================

* Fix mouse sensitivy being wrong in the horizontal direction in widescreen mode.
* Fix bug occasionally causing visual corruption when many things were on screen.
* Fix the well-known issue of the '5' character looking like a '6'. (Shouldn't interfere with other mods.)
* Fix "tearing" seen due to vsync being disabled. Re-enable vsync by default (use -dxnovsync to disable it).
* Some small performance improvements.
* Updated: include the relevant license texts.

Version 0.99.402
================

* Add seamless alt-enter switching between windowed and fullscreen modes.

Version 0.99.401c
=================

* Add experimental support for widescreen modes using a fork of SlashDiablo-HD by Mir Drualga and Bartosz Jankowski.
* Remove the use of "AA bilinear" filtering, in favor of point filtering. This is part of a work in progress and will be tweaked further.
* Cut VRAM footprint by 75% and reduce performance overhead.
* Source code is now in the git.
* Updated: fix occasional glitches due to the wrong texture cache policy being used.
* Updated again: forgot to include SlashDiabloHD.mpq, which is required.

Version 0.99.329b
=================

* Add support for 1024x768, tested with MedianXL which now seems to work.
* Fix window being re-centered by the game even if user moved it.
* Fix occasional crash introduced in 0.99.329.

Version 0.99.329
================

* Add support for 640x480 mode, and polish behavior around in-game resolution switching.

Version 0.98.329
================

* Add support for LoD 1.09d, 1.10 and 1.12.
* Add warning dialog on startup when using an unsupported game version.

Version 0.91.328
================

* Fix mouse pointer "jumping" when opening inventory and clicking items in fullscreen or scaled-window modes.

Version 0.91.327
================

* Fix two types of crashes in areas where there are many things on screen at once.
* Fix window not being movable.

Version 0.91.325b
=================

* Fix game being scaled wrong in LoD 1.13c/d.

Version 0.91.325
================

* Fix mode switching flicker on startup in fullscreen modes.
* Fix mouse cursor being "teleported" when leaving the window in windowed mode.
* Fix mouse speed not being consistent with the desktop.
* Fix game looking fuzzy on high DPI displays.
* Improve frame time consistency/latency.
* Add experimental window scaling support.
* More performance improvements.

Version 0.91.324
================

* Fix crash when hosting TCP/IP game.
* Speed up texture cache lookup.
* Changed to a slightly less insane version numbering scheme.

Version 0.9.210323c
===================

* Fix crash occurring after playing a while.

Version 0.9.210323b
===================

* Add support for LoD 1.13d.
* Fix accidental performance degradation in last build.

Version 0.9.210323
==================

* Add support for LoD 1.13c.
* Fix the delay/weird characters in the corner on startup in LoD 1.13c.
* Fix glitchy window movement/sizing on startup in LoD 1.13c.
* Performance improvements.

Version 0.9.210322
==================

* Fix line rendering (missing exp. bar, rain, npc crosses on mini map).
* Fix smudged fonts.
* Default fullscreen mode now uses the desktop resolution, and uses improved scaling (less fuzzy).

Version 0.9.210321b
===================

* Fix default fullscreen mode.

Version 0.9.210321
==================

* Initial release.
