# AGENTS.md

DistroAV is a C++ OBS Studio plugin (native module, `distroav.dll`/`.so`/`.plugin`)
that sends/receives video+audio over NDI. It links against libobs, Qt6 Widgets,
and the NDI SDK (vendored headers in [`lib/ndi/`](lib/ndi)). No test suite exists;
correctness is verified by building and exercising the plugin inside OBS.

For anything not covered here, see [docs/agent_docs/architecture.md](docs/agent_docs/architecture.md)
and [docs/agent_docs/build-system.md](docs/agent_docs/build-system.md).

## Build (Windows — verified in this checkout)

A configured `build_x64/` already exists in this repo (first-configure fetches
OBS source + prebuilt deps into `.deps/`, which is large and slow; skip it when
`build_x64/` is already present).

```powershell
cmake --build build_x64 --preset windows-x64
```

This was run and confirmed to succeed end-to-end in this checkout, producing
`build_x64/RelWithDebInfo/distroav.dll` and copying it into `build_x64/rundir`.

From scratch (no `build_x64/` yet):

```powershell
cmake --preset windows-x64
cmake --build build_x64 --preset windows-x64
```

Equivalent wrapper (also handles `CI`/`GITHUB_EVENT_NAME` env defaults the
underlying script expects): `.\tools\build-helper-windows.ps1`.

macOS/Linux use the same pattern with `macos`/`ubuntu-x86_64` presets, or
`.github/scripts/build-macos` / `.github/scripts/build-ubuntu` — **unverified in
this session** (this checkout only has Windows tooling available).

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

## Conventions (not enforced by clang-format/gersemi)

- Naming: `.github/CONTRIBUTING.md` says `snake_case` for C-style names,
  `CamelCase` for C++ class/method names; the wiki's [Code Style](https://github.com/DistroAV/DistroAV/wiki/3.-Development#code-styles)
  page adds a role-based nuance — methods `camelCase`, variables
  `snake_case`, and *defer to the third-party library's own convention*
  when interfacing OBS/Qt/NDI/libcurl/stdlib code — and admits current code
  is "somewhat scattered." Match whichever the surrounding code already
  uses; don't invent a stricter rule than either source states.
- Indentation: tabs, 8 columns wide; ~80 col soft line limit (per [`.github/CONTRIBUTING.md`](.github/CONTRIBUTING.md)).
- Commit messages: 50-char title / blank line / 72-col-wrapped body, present
  tense, prefixed with a scope when there's an obvious one (`CI:`, `UI:`,
  `Source:`, `PluginUpdate:` are all attested in history) — but a large fraction
  of real commits skip the scope prefix entirely, so don't invent one that
  doesn't fit.
- `buildspec.json`'s `version` field is the single source of truth for the
  plugin version; bump it in its own commit, separate from feature/fix work.

## Boundaries

**Always fine, no need to ask:**
- Editing files under `src/`, `data/locale/` (translation strings), `docs/`.
- Building locally (`cmake --build build_x64 --preset windows-x64`).
- Reading anything in `.deps/`, `build_x64/`, `release/` for reference (all gitignored, regenerated, never hand-edit).

**Ask first:**
- Changing `CMakeLists.txt`, `CMakePresets.json`, `buildspec.json`, or anything
  under `.github/` (workflows, actions, scripts) — these affect CI and release
  packaging for every platform, not just this checkout.
- Bumping the plugin version in `buildspec.json`.
- Installing the built plugin system-wide (`tools/install-windows.ps1` requires
  admin elevation and overwrites the user's live OBS plugin).
- Editing `.clang-format` / `.gersemirc` (repo-wide style contract).

**Never touch:**
- [`lib/ndi/`](lib/ndi) — vendored third-party NDI SDK headers under their own license.
- `.deps/`, `build_x64/`, `build_macos/`, `build_x86_64/`, `release*/` — generated, gitignored.
- Root-level `CLAUDE_HANDOFF.md`, `adapter-table-columns.md`, `drift-fix.patch`,
  `ReceiverStats.md`, `SenderStats.md` — gitignored personal scratch files left in
  this working tree from prior sessions/branches, not part of the tracked project
  (see "Uncertain" below).

## Uncertain / please confirm

- **`CLAUDE_HANDOFF.md`** etc.: checked this checkout — not present here,
  confirming they were local/personal scratch files specific to whichever
  working tree first wrote that note, not something every checkout has. The
  **`networkmonitor` branch is real and actively developed**
  (`origin/networkmonitor`, last commit 2026-08-27, newer than `master`'s),
  so the underlying question stands: should that branch carry its own
  AGENTS.md guidance for `ndi-network-report.*` once it lands, or fold in
  here now?
- I could only verify the **Windows** build path end-to-end. The
  macOS/Ubuntu commands are corroborated by the project's own wiki
  (Development page) in addition to the CI scripts, but still not executed
  by me — a sanity check on those platforms is worth doing before trusting
  them blindly.
- `clang-format`/`gersemi` invocation is no longer a guess: I wrote and
  tested `.github/scripts/run-clang-format.{ps1,sh}` (currently untracked —
  add them to this branch if you want them kept) which install and run
  clang-format 19.x cross-platform; see "Lint / format" above. `gersemi`
  still has no wrapper script.
- Commit-scope prefixes (`CI:`, `UI:`, `Source:`, etc.): confirmed accurate
  against `.github/CONTRIBUTING.md`, but still inconsistently used in real
  `git log` history — kept as a loose convention, not a hard rule, per the
  file's own wording ("Typical scopes," not "required scopes").
- The `!AGENTS.md` / `!/docs` `.gitignore` additions are confirmed present
  in this branch's `.gitignore` already — no outstanding action there.
- **New in this pass**: the `ERR-*` numbered error-code convention
  (`obs_log(LOG_ERROR, "ERR-4xx - ...")`) and the `--distroav-*` CLI test
  flags (`src/config.cpp`) were missing from this branch's docs entirely —
  added both to `docs/agent_docs/architecture.md`, cross-checked against
  current `src/` and the wiki's Troubleshooting page. Worth a skim since the
  wiki's error-code catalog has drifted from code in a few places (noted
  inline there).
