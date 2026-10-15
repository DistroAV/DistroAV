# Architecture notes

Background for the summary in [AGENTS.md](../../AGENTS.md).

## Module entry point

[`src/plugin-main.cpp`](../../src/plugin-main.cpp) is the OBS module entry point
(`OBS_DECLARE_MODULE`, `obs_module_load`/`obs_module_unload`). It registers the
source/output/filter types with libobs and owns the global `ndiLib` pointer — the
loaded `NDIlib_v6` function table from `libndi` (dynamically loaded at runtime, not
linked; see `ndi-finder.cpp` for how the library is located on each OS).

## The five OBS plugin objects

Each corresponds to one `obs_*_info` struct registered in `plugin-main.cpp`:

| File | OBS object type | Purpose |
|---|---|---|
| [`ndi-source.cpp`](../../src/ndi-source.cpp) | source | Receives an NDI stream into OBS (video/audio/PTZ/tally). Largest file in the plugin. |
| [`main-output.cpp`](../../src/main-output.cpp) | output | Sends the OBS program mix out as an NDI stream. |
| [`preview-output.cpp`](../../src/preview-output.cpp) | output | Sends the OBS Studio-mode preview out as a separate NDI stream. |
| [`ndi-filter.cpp`](../../src/ndi-filter.cpp) | filter | Per-source/scene "dedicated NDI output" filter. |
| [`test-output.cpp`](../../src/test-output.cpp) | output | Minimal test-pattern NDI output, used for diagnostics. |

## Supporting modules

- [`ndi-finder.cpp`/`.h`](../../src/ndi-finder.cpp) — locates and dynamically loads
  the installed NDI runtime library across OS.
- [`config.cpp`/`.h`](../../src/config.cpp) — reads/writes plugin settings to OBS's
  `global.ini` under the `[NDIPlugin]` section (see the doc comment at the top of
  `config.h` for exact file paths per OS).
- [`sync-debug.cpp`/`.h`](../../src/sync-debug.cpp) — compile-time-gated (off by
  default) A/V sync logging; see git log around "sync-debug" for why (drift
  diagnostics).
- [`premultiplied-alpha-filter.cpp`](../../src/premultiplied-alpha-filter.cpp) —
  small standalone OBS filter for alpha premultiplication, independent of the NDI
  filter above.
- [`src/forms/`](../../src/forms) — Qt Widgets dialogs (`.ui` + `.cpp`/`.h` pairs):
  output settings dialog, update-check dialog. `AUTOMOC`/`AUTOUIC` are on, so new
  `.ui` files just need adding to `target_sources` in `CMakeLists.txt`.
- [`src/obs-support/`](../../src/obs-support) — small helpers bridging OBS's C
  frontend API and Qt (`obs-app.hpp`, `qt_wrapper.hpp`, `curl-helper.h`,
  `remote-text.cpp` for the update-checker's HTTP fetch).
- [`lib/ndi/`](../../lib/ndi) — vendored NDI SDK headers (C++ wrapper +
  DynamicLoad). Third-party, do not edit.
- [`data/locale/`](../../data/locale) — translation `.ini` files, one per
  language, keyed by the same string IDs used via `Str("NDIPlugin....")` in C++.

## Threading model (read before touching source/output code)

NDI frame receive/send happens off the Qt/UI thread. `ndi-source.cpp` and
`main-output.cpp`/`preview-output.cpp` each run their own worker thread(s) for
frame pumping; UI-thread code (settings dialogs, `config.cpp` callbacks) must not
block on NDI I/O. Commit history (search log for "Lock keys", "mutex", "queued
on the UI thread") shows this has been a source of real bugs — treat any new
cross-thread state as something to explicitly synchronize, not an oversight to
fix later.

## Performance-sensitive paths

The plugin's job is real-time A/V transport; the callbacks below run once per
frame or per audio buffer, on the worker threads described above, and directly
determine whether streams glitch, desync, or add latency. Treat any change to
these as performance-critical by default:

- `ndi-source.cpp` — the NDI receive callback(s) that hand frames/audio to OBS.
- `main-output.cpp` / `preview-output.cpp` — the OBS output callbacks that hand
  frames/audio to NDI for send.
- `ndi-filter.cpp` — the per-source filter's video/audio callbacks.
- `premultiplied-alpha-filter.cpp` — per-pixel work on the video path.

Inside these callbacks, avoid heap allocation, blocking I/O, synchronous
logging, and any lock that could be contended by another real-time callback
or by the UI thread — do that work once (setup/config-change time) and cache
the result instead of recomputing or re-locking per frame. When a change to
one of these files is not obviously free, say so and profile rather than
assume; don't trade pipeline performance for readability or a smaller diff.

## Error codes

`obs_log(LOG_ERROR, "ERR-4xx - ...")` and `obs_log(LOG_WARNING, "WARN-4xx - ...")`
calls use a stable numeric convention, catalogued for end users on the
[Troubleshooting wiki page](https://github.com/DistroAV/DistroAV/wiki/2.-Troubleshooting#error--warning-code---obs-log).
If you add, remove, or renumber one (grep `src -r -oE '"?(ERR|WARN)-[0-9]+'`
to see what's in use), update the wiki catalog to match — it can't be edited
from this repo.

## CLI test flags

`Config::ParseCommandLineArgs` (`src/config.cpp`) reads `--distroav-*` OBS
launch flags, useful for exercising failure/edge paths without faking real
state:

| Flag | Effect |
|---|---|
| `--distroav-debug` / `--distroav-verbose` | Debug / verbose logging. |
| `--distroav-log[=error\|warning\|info\|debug\|verbose]` | Set log level explicitly. |
| `--distroav-update-force[=0\|1]` | Force/skip the update-available state. |
| `--distroav-update-last-check-ignore` | Ignore the last-checked timestamp throttle. |
| `--distroav-update-local[=port]` | Point update checks at a local emulator (wiki: "Update Testing"). |
| `--distroav-check-ndilib-forcefail` / `--distroav-check-obs-forcefail` | Force the NDI-lib / OBS-version hard-requirement check to fail (simulates `ERR-401`/`ERR-424`). |
| `--distroav-check-ndilib-ignore` / `--distroav-check-obs-ignore` | Bypass those same requirement checks (`Config::Check*Bypass`). |
| `--distroav-detect-obsndi-force[=off\|on]` | Force old-`obs-ndi`-installed detection on/off. |

## Manual acceptance test

No automated suite exists (see `docs/agent_docs/build-system.md`), so the
wiki defines a manual checklist instead — the closest thing this project
has to "tests passing." For each platform: install the plugin, launch OBS,
confirm the "NDI Output Settings" Tools-menu entry appears; enable NDI
Output (Main + Preview); add an NDI Source with an "NDI Audio Output"
filter and loop back local OBS output/audio; close OBS and check the log
ends with `Number of memory leaks: 0` (this reportedly differs by platform
historically — verify rather than assume); uninstall. Log locations: Linux
`~/.config/obs-studio` (Flatpak: `~/.var/app/com.obsproject.Studio/config/obs-studio`),
macOS `~/Library/Application Support/obs-studio`, Windows
`%APPDATA%\obs-studio\logs`.
