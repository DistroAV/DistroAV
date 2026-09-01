# Build system details

Background for the commands summarized in [AGENTS.md](../../AGENTS.md).

This project is based on [obs-plugintemplate](https://github.com/obsproject/obs-plugintemplate)
and periodically pulls in upstream template changes by hand (it's not a pure
fork, so this can't be a merge/rebase — see the wiki's "Updating
obs-plugintemplate" section). That's why most of `.github/`, `cmake/`, and the
top-level build files read as generic OBS-plugin boilerplate rather than
DistroAV-specific code.

## How dependencies are fetched

There is no vendored/prebuilt SDK checked into git for OBS or Qt. `buildspec.json`
pins exact versions + sha256 hashes of:

- `obs-studio` source (built from source as part of this build, in `.deps/obs-studio-<version>/`)
- `prebuilt` obs-deps (Windows/macOS only)
- `qt6` prebuilt binaries (Windows/macOS only)

On Windows/macOS, `.github/scripts/Build-Windows.ps1` / `build-macos` download and
verify these into `.deps/` on first configure, then invoke CMake with the matching
preset. Ubuntu instead installs deps from apt via `.github/scripts/utils.zsh/setup_ubuntu`
(see `.github/scripts/.Aptfile`). The NDI SDK itself is vendored directly in
[`lib/ndi/`](../../lib/ndi) (headers only, from the NDI SDK installer) — there is no
separate fetch step for it.

`.deps/` and `build_x64/` (or `build_macos/`, `build_x86_64/`) are gitignored and can
be multiple GB; a from-scratch configure re-downloads OBS source + prebuilt deps and
takes significantly longer than the incremental build described in AGENTS.md.

## Presets

`CMakePresets.json` defines a `template` base (`ENABLE_FRONTEND_API`, `ENABLE_QT`)
and per-OS presets (`windows-x64`, `macos`, `ubuntu-x86_64`), each with a `-ci`
variant that adds `CMAKE_COMPILE_WARNING_AS_ERROR=ON` and ccache. Local dev presets
do **not** warning-as-error; CI presets do — a change that's warning-clean locally
can still fail CI.

## What CI actually gates a PR on

`.github/workflows/pr-pull.yaml` runs two jobs on every PR:

1. `check-format.yaml` — clang-format (C/C++/ObjC files) and gersemi (CMake files)
   diff-check, both via `ubuntu-24.04` runners only (the composite actions
   explicitly refuse to run on a Windows runner). There is no local equivalent
   available on a Windows dev box unless you install `clang-format` (v16+, see
   `.clang-format`) and `gersemi` (`pip install gersemi`, see `.gersemirc`, or run
   them under WSL) yourself.
2. `build-project.yaml` — builds Windows x64, macOS universal, and Ubuntu x86_64
   in parallel using the `-ci` presets above. A PR labeled `Seeking Testers`
   additionally gets a signed, packaged installer built (`codesign:true`,
   `package:true` in the `check-event` job) — mention this label if a user
   wants outside testers to try a branch build before merge.

There is no unit/integration test suite in this repo — "testing" a change means
building the plugin and exercising it manually inside OBS (see the Run/Debug
section of AGENTS.md, and `docs/agent_docs/architecture.md`'s "Manual
acceptance test" for the project's documented QA checklist), because the
surface area (frame callbacks, Qt UI, NDI network I/O) isn't practically
unit-testable without OBS + real/simulated NDI sources.

## Version bump

`buildspec.json`'s `version` field is the single source of truth for the plugin
version baked into `plugin-support.c.in` and the installers. Recent history bumps
this in its own dedicated commit (e.g. "Update DistroAV version from 6.2.0 to
6.2.1") separate from feature work.
