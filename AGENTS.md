# AGENTS.md

DistroAV is a C++ OBS Studio plugin (native module, `distroav.dll`/`.so`/`.plugin`)
that sends/receives video+audio over NDI. It links against libobs, Qt6 Widgets,
and the NDI SDK (vendored headers in [`lib/ndi/`](lib/ndi)). No test suite exists;
correctness is verified by building and exercising the plugin inside OBS.

For anything not covered here, see [docs/agent_docs/architecture.md](docs/agent_docs/architecture.md)
and [docs/agent_docs/build-system.md](docs/agent_docs/build-system.md).

## Build (Windows)

Use the `-ci` preset (`windows-ci-x64`), not the plain `windows-x64` preset —
the only difference is `CMAKE_COMPILE_WARNING_AS_ERROR=ON`, which is what
actually gates CI on macOS/Linux. MSVC and Clang/GCC warn about different
things, so a build that's warning-clean under the plain preset can still fail
CI on another OS; the `-ci` preset surfaces the overlapping subset of those
warnings (unused parameters, narrowing conversions, etc.) locally first.

```powershell
cmake --preset windows-ci-x64
cmake --build build_x64 --preset windows-ci-x64
```

A fresh configure + build with this preset produces
`build_x64/RelWithDebInfo/distroav.dll`, with `CMAKE_COMPILE_WARNING_AS_ERROR:BOOL=TRUE`
visible in `build_x64/CMakeCache.txt`. First configure fetches OBS source +
prebuilt deps into `.deps/` — large and slow; skip only if `build_x64/` is
already configured with this same `-ci` preset. Cache variables apply at
configure time only, so switching an existing `build_x64/` from the plain
preset requires re-running the `cmake --preset windows-ci-x64` configure step
once, not just building under the new preset name.

`.\tools\build-helper-windows.ps1` already configures with `windows-ci-<Target>`
internally, so warning-as-error is active through that wrapper too even though
its build step names the plain preset.

macOS/Linux: same pattern with `macos-ci`/`ubuntu-ci-x86_64` presets, or
`.github/scripts/build-macos` / `build-ubuntu` (already CI-preset-based) —
these can only be exercised on those platforms, not from Windows.

## Run / manually test a change

```powershell
.\tools\build-helper-windows.ps1 ; .\tools\install-windows.ps1 ; .\tools\run-obs-debug-windows.cmd
```

`install-windows.ps1` copies `release/RelWithDebInfo/distroav/` into
`%ProgramData%\obs-studio\plugins\distroav` — it **requires admin elevation** and
will self-elevate via a UAC prompt. Run each step separately when iterating rather
than the chained one-liner, so a build failure doesn't fall through to installing
stale output.

## Lint / format (CI-enforced on Linux/macOS runners; scripted locally on any OS)

`clang-format` (C/C++/ObjC, config in [`.clang-format`](.clang-format) — the file
says v16+, but CI/`build-aux` pin exactly 19.1.1) and `gersemi` (CMake files,
config in [`.gersemirc`](.gersemirc)) run in CI on every PR via
`check-format.yaml`, on `ubuntu-24.04` runners only.

```powershell
.github\scripts\run-clang-format.ps1       # Windows
```
```bash
.github/scripts/run-clang-format.sh        # Linux/macOS
```

These auto-install clang-format 19.x (via pip) if missing, format only
changed/new/staged files (or `--base <ref>` for a whole branch), and fix a
missing trailing EOF newline; add `--check` to verify without modifying. No
equivalent wrapper exists yet for `gersemi` — install it (`pip install
gersemi`) and run it directly, or use `build-aux/.run-format.zsh` (zsh; also
covers clang-format) on Linux/macOS/WSL.

## Architecture (see [docs/agent_docs/architecture.md](docs/agent_docs/architecture.md) for detail)

- [`src/plugin-main.cpp`](src/plugin-main.cpp) — module entry point, registers
  the source/output/filter types below with libobs.
- [`src/ndi-source.cpp`](src/ndi-source.cpp) — NDI→OBS source (largest file).
- [`src/main-output.cpp`](src/main-output.cpp) / [`src/preview-output.cpp`](src/preview-output.cpp) — OBS→NDI program/preview outputs.
- [`src/ndi-filter.cpp`](src/ndi-filter.cpp) — per-source "dedicated NDI output" filter.
- [`src/ndi-finder.cpp`](src/ndi-finder.cpp) — locates/loads the NDI runtime at startup.
- [`src/config.cpp`](src/config.cpp) — plugin settings, persisted to OBS's `global.ini`.
- [`src/forms/`](src/forms) — Qt Widgets dialogs (`.ui` + code-behind).
- [`src/obs-support/`](src/obs-support) — OBS C-API/Qt glue helpers.
- NDI send/receive runs on dedicated worker threads, separate from the Qt UI
  thread — see the threading note in `docs/agent_docs/architecture.md` before
  touching source/output code; cross-thread state bugs here have a real history.

## Performance (top priority)

This plugin transports live audio and video in real time between OBS and NDI.
Pipeline performance — latency, frame drops, jitter — is the top priority
architectural concern, above code cleanliness or convenience. Any design
decision touching a frame/audio callback (allocation, locking, logging,
copies, indirection) should default to the cheapest option that's correct,
and be justified against this before anything else. See "Performance-sensitive
paths" in [docs/agent_docs/architecture.md](docs/agent_docs/architecture.md)
for exactly which files/callbacks this applies to.

## Conventions (not enforced by clang-format/gersemi)

- Naming: `.github/CONTRIBUTING.md` says `snake_case` for C-style names,
  `CamelCase` for C++ class/method names; the wiki's [Code Style](https://github.com/DistroAV/DistroAV/wiki/3.-Development#code-styles)
  page adds a role-based nuance — methods `camelCase`, variables
  `snake_case`, and *defer to the third-party library's own convention*
  when interfacing OBS/Qt/NDI/libcurl/stdlib code — and admits current code
  is "somewhat scattered." Match whichever the surrounding code already
  uses; don't invent a stricter rule than either source states.
- Indentation: tabs, 8 columns wide; ~80 col soft line limit (per [`.github/CONTRIBUTING.md`](.github/CONTRIBUTING.md)).
- `buildspec.json`'s `version` field is the single source of truth for the
  plugin version; bump it in its own commit, separate from feature/fix work.

## Boundaries

**Always fine, no need to ask:**
- Editing files under `src/`, `data/locale/` (translation strings), `docs/`.
- Building locally (`cmake --build build_x64 --preset windows-ci-x64`).
- Reading anything in `.deps/`, `build_x64/`, `release/` for reference (all gitignored, regenerated, never hand-edit).

**Ask first:**
- Changing `CMakeLists.txt`, `CMakePresets.json`, `buildspec.json`, or anything
  under `.github/` (workflows, actions, scripts) — these affect CI and release
  packaging for every platform, not just a local build.
- Bumping the plugin version in `buildspec.json`.
- Installing the built plugin system-wide (`tools/install-windows.ps1` requires
  admin elevation and overwrites the user's live OBS plugin).
- Editing `.clang-format` / `.gersemirc` (repo-wide style contract).

**Never touch:**
- [`lib/ndi/`](lib/ndi) — vendored third-party NDI SDK headers under their own license.
- `.deps/`, `build_x64/`, `build_macos/`, `build_x86_64/`, `release*/` — generated, gitignored.
- Creating or making git commits (`git commit`) or pushing — agents must leave
  all committing to the user, regardless of how the change was made or how
  confident the change is.

## Known gaps

- The macOS/Ubuntu build commands are corroborated by the project's wiki
  (Development page) and by the CI scripts, but nothing on Windows can
  actually exercise them — sanity-check them on those platforms before
  relying on them.
- `.github/scripts/run-clang-format.{ps1,sh}` install and run clang-format
  19.x cross-platform (see "Lint / format" above); `gersemi` has no
  equivalent wrapper script.
