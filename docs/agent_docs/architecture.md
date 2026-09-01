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
  default) A/V sync logging, added recently; see git log around "sync-debug" for
  why (drift diagnostics).
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
block on NDI I/O. Recent commit history (search log for "Lock keys", "mutex",
"queued on the UI thread") shows this has been an active source of real bugs —
treat any new cross-thread state as something to explicitly synchronize, not an
oversight to fix later.

## Error codes

`obs_log(LOG_ERROR, "ERR-4xx - ...")` calls use a stable numeric convention,
catalogued for end users on the
[Troubleshooting wiki page](https://github.com/DistroAV/DistroAV/wiki/2.-Troubleshooting#error--warning-code---obs-log).
If you add a new hard/soft-requirement failure path, pick an unused number
(grep `src -r -oE '"?ERR-[0-9]+'` first) and tell the user the wiki catalog
needs a matching entry — it can't be edited from this repo.

**Verified against current code**: codes actually in use are `400`–`412`,
`424`, `425`, `430`. The wiki catalog additionally lists `413`–`423` and
`426` (update-check failures in `src/forms/update.cpp`, output-filter
failures, a config-validation error) and marks `424`/`425` "reserved for
future use" — but those two are already implemented in `plugin-main.cpp`
(OBS/NDI minimum-version checks), and the other wiki-only codes don't
appear anywhere in `src/`. Treat the wiki catalog as informative, not
ground truth; verify against `src/` when it matters for a specific code.

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
