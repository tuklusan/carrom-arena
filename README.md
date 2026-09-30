# Carrom Arena

[![Play Online](https://img.shields.io/badge/play-online-brightgreen?logo=googlechrome&logoColor=white)](https://tuklusan.github.io/carrom-arena/)
[![Latest Release](https://img.shields.io/github/v/release/tuklusan/carrom-arena?label=release)](https://github.com/tuklusan/carrom-arena/releases/latest)
[![CI](https://github.com/tuklusan/carrom-arena/actions/workflows/ci.yml/badge.svg)](https://github.com/tuklusan/carrom-arena/actions/workflows/ci.yml)
[![Platforms](https://img.shields.io/badge/platforms-Windows%20%7C%20Linux%20%7C%20macOS%20%7C%20Web-blue)](#download-or-play-online)
[![Made with raylib](https://img.shields.io/badge/made%20with-raylib-orange)](https://www.raylib.com/)
[![Physics: Box2D](https://img.shields.io/badge/physics-Box2D-9cf)](https://box2d.org/)
[![License: Non-Commercial](https://img.shields.io/badge/license-non--commercial-lightgrey)](LICENSE)

https://github.com/user-attachments/assets/ecdca033-4538-4e11-a14c-7dabf06280a8

**Carrom Arena** is a graphical, cross-platform, four-robot autonomous **Carrom simulation**, written in portable C17 with raylib and real Box2D physics. Four AI players plan and execute complete matches under official **International Carrom Federation (ICF) rules** — no human gameplay input required. Watch it run natively on Windows, Linux and macOS, or play the exact same simulation instantly in any browser over WebAssembly. Pause, change speed, or restart with a new seed at any time.

## Download or Play Online

**[Latest release ⬇](https://github.com/tuklusan/carrom-arena/releases/latest)** for Windows, Linux and macOS — grab the zip for your platform, unzip, and run. Everything (assets, sound effects) is embedded in the single executable. No installation, no dependencies.

**[Play online ▶](https://tuklusan.github.io/carrom-arena/)** — the same simulation, running right in your browser via WebAssembly. Click once on the page to start (browsers require a click before audio can play).

## Controls

| Key | Action |
|-----|--------|
| **Space** | Pause / resume |
| **+ / -** | Speed up / slow down |
| **R** | Restart with a new seed |
| **M** | Mute |
| **Q / Esc** | Quit |

## Screenshots

Every release is built and run on all of the platforms below, as proof it actually works there — not just that it compiles.

### Linux

<table>
<tr>
<td align="center"><img src="docs/screenshots/screenshot-ubuntu-24.04.png" width="260"><br><sub>Ubuntu 24.04 · x64</sub></td>
<td align="center"><img src="docs/screenshots/screenshot-ubuntu-24.04-arm.png" width="260"><br><sub>Ubuntu 24.04 · ARM64</sub></td>
<td align="center"><img src="docs/screenshots/screenshot-ubuntu-22.04.png" width="260"><br><sub>Ubuntu 22.04 · x64</sub></td>
</tr>
<tr>
<td align="center"><img src="docs/screenshots/screenshot-ubuntu-22.04-arm.png" width="260"><br><sub>Ubuntu 22.04 · ARM64</sub></td>
<td align="center"><img src="docs/screenshots/screenshot-ubuntu-26.04.png" width="260"><br><sub>Ubuntu 26.04 · x64</sub></td>
<td align="center"><img src="docs/screenshots/screenshot-ubuntu-26.04-arm.png" width="260"><br><sub>Ubuntu 26.04 · ARM64</sub></td>
</tr>
<tr>
<td align="center"><img src="docs/screenshots/screenshot-ubuntu-slim.png" width="260"><br><sub>Ubuntu Slim · x64</sub></td>
<td></td>
<td></td>
</tr>
</table>

### Windows

<table>
<tr>
<td align="center"><img src="docs/screenshots/screenshot-windows-2022.png" width="260"><br><sub>Windows Server 2022 · x64</sub></td>
<td align="center"><img src="docs/screenshots/screenshot-windows-2025.png" width="260"><br><sub>Windows Server 2025 · x64</sub></td>
<td align="center"><img src="docs/screenshots/screenshot-windows-2025-vs2026.png" width="260"><br><sub>Windows Server 2025 (VS2026) · x64</sub></td>
</tr>
</table>

### macOS

<table>
<tr>
<td align="center"><img src="docs/screenshots/screenshot-macos-26.png" width="260"><br><sub>macOS 26 Tahoe · Apple Silicon</sub></td>
<td align="center"><img src="docs/screenshots/screenshot-macos-26-intel.png" width="260"><br><sub>macOS 26 Tahoe · Intel</sub></td>
<td align="center"><img src="docs/screenshots/screenshot-macos-15.png" width="260"><br><sub>macOS 15 Sequoia · Apple Silicon</sub></td>
</tr>
<tr>
<td align="center"><img src="docs/screenshots/screenshot-macos-15-intel.png" width="260"><br><sub>macOS 15 Sequoia · Intel</sub></td>
<td align="center"><img src="docs/screenshots/screenshot-macos-14.png" width="260"><br><sub>macOS 14 Sonoma · Apple Silicon</sub></td>
<td align="center"><img src="docs/screenshots/screenshot-xcode-27.png" width="260"><br><sub>Xcode 27 (preview) · Apple Silicon</sub></td>
</tr>
</table>

## Part of an Ongoing Series — and the Robots Got the Last Laugh

This project has a stranger history than four robots calmly flicking a striker around a board suggests. It began as a job handed to an actual **AI-run software company** — six autonomous roles (CEO, CPO, CTO, programmer, reviewer, tester) built by forking an agentic coding CLI onto free NVIDIA NIM models, the subject of an ongoing **Agentic AI Development** series on this blog. That company was fired — for cause, 2026-09-24 — and the operator (a carbon-based life form) finished the job personally: a native C17/raylib/Box2D build, then a full WebAssembly port that took *two* completely unrelated audio bugs to get working (one of them a silently-dropped `HEAPF32` export that only gave itself up under a browser console), an AI shot evaluator that used to snub a wide-open queen in favor of an equally-easy coin, and a Tron-style wormhole backdrop that needed four rounds of "no, not like that" before it finally read as travelling through space instead of a very confused lava lamp.

Read the full, warts-and-all account — **["The AI Software Company Got Fired — I Finished the Carrom Game Myself"](https://supratim-sanyal.blogspot.com/2026/09/fire-ai-software-company-finish-carrom-arena-game.html)**, Part 7 of the series.

## License

Carrom Arena is released under the **SANYALnet Labs Non-Commercial License** (see [`LICENSE`](LICENSE)): free use, modification and distribution for non-commercial purposes only, with attribution — *Based on original work by Supratim Sanyal of SANYALnet Labs.*

Third-party material keeps its own license: raylib 5.5 (zlib/libpng), Box2D v3.1.0 (MIT), Unity Test Framework (MIT), sound effects from Kenney (CC0, see [`assets/audio/CREDITS.md`](assets/audio/CREDITS.md)). The in-game radio streams the public [AH.FM](https://ah.fm) internet radio and bundles no recording.
