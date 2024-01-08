# D2DX

D2DX is a Glide-wrapper and mod that makes the classic Diablo II/LoD run well on modern PCs, while honoring the original look and feel of the game. Play in a window or in fullscreen, glitch-free, with or without enhancements like widescreen, true high framerate and anti-aliasing.

## Features

- Turns the game into a well behaved DirectX 11 title on Windows 10 (also 7, 8 and 8.1).
- High quality scaling to fit modern screen sizes, including [widescreen aspect ratios](https://raw.githubusercontent.com/bolrog/d2dx/main/screenshots/d2dx2.png).
- High FPS mod using motion smoothing, bypassing the internal 25 fps limit. Play at 60 fps and higher! ([video](https://imgur.com/a/J1F8Ctb))
- [Anti-aliasing](https://github.com/bolrog/d2dx/wiki/Screenshots#anti-aliasing) of specific jagged edges in the game (sprites, walls, some floors).
- Seamless windowed/fullscreen switching with (ALT-Enter).
- Improved fullscreen: instant ALT-TAB and low latency.
- Improved windowed mode.
- Proper gamma/contrast.
- Fixes a few window-related glitches in Diablo II itself.

### Video Showcasing Motion Smoothing

  [FPS increase in menus, FPS increase for projectiles, monsters, +more](https://imgur.com/a/J1F8Ctb)

## Requirements

- Windows 7 SP1 and above (10 recommended for latency improvements).
- A CPU with SSE4 support.
- Integrated graphics or discrete GPU with DirectX 10.1 support.

## Version Compatibility

Basic rendering is supported for all Diablo II versions and should work for all mods.

High resolution is provided by [SGD2FreeRes] and supports the following versions: `1.09d`, `1.10`, `1.12`, `1.13c`, `1.13d`, `1.14c`, `1.14d`.

The anti-aliasing filter is supported on all LoD versions except: `1.09c`, `1.13a` and `1.13b`.

High FPS and motion smoothing is provided by [D2fps] and supports nearly all released Diablo II versions. This may have compatibility issues with other mods providing the same features.

### BaseMod

Disable `BypassFPS` in `BaseMod.ini`:

```ini
[BypassFPS]
Enabled=0
```

### MapHack (BH.dll)

Disable `Apply CPU Patch` and `Apply FPS Patch` in `BH.cfg`:

```none
Apply CPU Patch: False
Apply FPS Patch: False
```

### MedianXL

MedianXL is not compatible with the resolution mod. Launch with `-dxnoresmod` or set `noresmod=false` in `d2dx.cfg`.

### Others

For compatibility with other mods, see the [wiki](https://github.com/bolrog/d2dx/wiki/Compatibility-with-other-mods).

## Documentation

This readme contains basic information to get you started. See the [D2DX wiki](https://github.com/bolrog/d2dx/wiki/) for more documentation.

## Installation

  Copy the included "glide3x.dll" and "d2fps.dll" into your Diablo II folder.

  Note that in some cases you may have to also download and install the Visual C++ runtime library from Microsoft: <https://aka.ms/vs/16/release/vc_redist.x86.exe>

## Usage

To start the game with D2DX the game will have to be launched in glide mode. This can be done by:

* Launching the game with `-3dfx`. e.g. `game.exe -3dfx`.
* Selecting '3dfx Glide' from `D2VidTst.exe`
* Setting `HKEY_CURRENT_USER\SOFTWARE\Blizzard Entertainment\Diablo II\VideoConfig` to `3` in the registry

Windowed/fullscreen mode can be switched at any time by pressing ALT-Enter. The normal -w command-line option works too.

Many of the default settings of D2DX can be changed. For a full list of command-line options and how to use a configuration file, see the [wiki](https://github.com/bolrog/d2dx/wiki/).

## Troubleshooting

### I get a message box saying "Diablo II is unable to proceed. Unsupported graphics mode."

  You are running the download version of Diablo II from blizzard.com. This can be modified to work with D2DX (Wiki page about this to come).

### It's ugly/slow/buggy

  Let me know by filing an issue! I'd like to keep improving D2DX (within the scope of the project).

## Credits

Continued Development/Maintainence: Jarcho
Original development: bolrog
Patch contributions: Xenthalon

The research of many people in the Diablo II community over twenty years made this project possible.

Thanks to Mir Drualga for making the fantastic SGD2FreeRes mod!
Thanks also to everyone who contributes bug reports.

D2DX uses the following third party libraries:

- FNV1a hash reference implementation, which is in the public domain.
- Detours by Microsoft.
- SGD2FreeRes by Mir Drualga, licensed under Affero GPL v3.
- FXAA implementation by Timothy Lottes. (This software contains source code provided by NVIDIA Corporation.)
- stb_image by Sean Barrett
- pocketlzma by Robin Berg Pettersen
- 9-tap Catmull-Rom texture filtering by TheRealMJP.
- xxHash by Yann Collet

[SGD2FreeRes]: https://github.com/mir-diablo-ii-tools/SlashGaming-Diablo-II-Free-Resolution
[D2fps]: https://github.com/Jarcho/d2-rs/tree/main/d2fps
