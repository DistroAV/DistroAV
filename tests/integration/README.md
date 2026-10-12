<!-- Copyright (C) 2026 Grant Likely <grant.likely@secretlab.ca> -->

# DistroAV integration tests

These tests boot a real OBS Studio with the DistroAV plugin loaded, and check
what the plugin does with NDI streams.
They are strictly external: nothing under `src/` is built differently for them.
OBS is driven only through obs-websocket and the OBS sources, outputs and
filters API.
NDI senders and receivers written for the tests (cyndilib) stand in for the
network.

## Running

Build the plugin first, then:

```bash
scripts/run-integration-tests.sh
```

Arguments go to pytest, so `scripts/run-integration-tests.sh -k smoke -v` runs
part of it.
The script keeps a virtualenv in `$DISTROAV_IT_VENV` (default: a temporary
folder) and installs `requirements.txt` into it.

| Variable | Meaning |
|---|---|
| `DISTROAV_OBS_BINARY` | The OBS to run. Default: the `obs` on the `PATH`, or on macOS the newest release in `/Applications` (not a release candidate). |
| `DISTROAV_PLUGIN_DIR` | Linux: the folder holding `distroav.so` and `distroav/locale`. Default: a `build_*/rundir/<config>` of this checkout. |
| `DISTROAV_PLUGIN_BUNDLE` | macOS: the `distroav.plugin` to test. Default: a `build_*/rundir/<config>/distroav.plugin` of this checkout. |
| `DISTROAV_IT_VENV` | Where the virtualenv lives. |
| `DISTROAV_OBS_START_TIMEOUT` | Seconds OBS has to start and come up (default 30; it takes 2 to 6). |
| `DISTROAV_KEEP_HOME` | Set to keep OBS's isolated home and log after the run. |
| `DISTROAV_ALLOW_STRAYS` | Set to run although an OBS from an earlier run is still alive. |
| `DISTROAV_REQUIRE_CLEAN_QUIT` | Set to fail a run in which OBS exits with a failure status when it quits. |

The plugin needs the NDI runtime installed, as it does for any use.
The tests themselves use cyndilib, which brings its own copy.
On Linux OBS needs a display, so `DISPLAY` must be set; `xvfb-run` works.

## How it works

1. `conftest.py` makes an isolated `$HOME`, writes obs-websocket's settings into
   it, copies the built plugin into its plugin folder, and starts OBS on it.
   This is the only config the suite writes by hand.
   Scenes and sources are made live over obs-websocket, so the suite does not
   depend on OBS's scene-collection format.
2. Each test uses `obsws.py` (a minimal obs-websocket v5 client) through the
   `obs_world` fixture.
3. OBS is started once per session, and the scenes a test made are removed after
   it.

## Quitting

OBS is told to quit with SIGTERM, which it handles as a normal quit, and it has
to exit within 30 seconds or it is killed and the test fails.
Its exit status is recorded, and a failure status is warned of.
It is an error only with `DISTROAV_REQUIRE_CLEAN_QUIT=1`, because the OBS 32.1.0
package on Ubuntu 26.04 aarch64 segfaults on quit with no plugin loaded at all
(measured: nine of nine runs, with and without DistroAV).
Where OBS quits with status 0, set it, and a crash on quit while an output is
active fails the run.

## Stray OBS processes

An OBS left running by an earlier run still holds the ports and NDI names the
tests expect.
The tests then fail only some of the time, in ways that look like nothing.
So the session refuses to start while there is one, and lists them to stop.
It also says if the run itself left one behind.

## Platforms

Linux and macOS.
The tests that start OBS are skipped, with a reason, on Windows until its launch
details are written and tried.
Windows arm64 has no cyndilib wheel, so the helpers cannot be installed there at
all.

### macOS

Build the plugin first, from this checkout, so that it is the code under test:

```bash
cmake --preset macos -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build_macos --config RelWithDebInfo
```

Then `scripts/run-integration-tests.sh` finds
`build_macos/rundir/RelWithDebInfo/distroav.plugin` by itself.
A build older than a fix the suite tests fails that test, which is the suite
working: the persistence tests need #1472.

Four things are particular to macOS, and handled by the fixture:

- OBS finds its profile through a system call that ignores `$HOME`, so the
  fixture sets `CFFIXED_USER_HOME`, which it does honor.
  A run never reads or changes your own profile.
- A build straight from CMake has no code signature, and an arm64 Mac will not
  load code without one.
  The fixture copies the bundle into the temporary profile and signs the copy ad
  hoc, so that the build is left as it was.
- OBS asks for a first run wizard and for permissions, in modal dialogs no one
  answers, so the profile says both were done.
- OBS is told to quit with SIGINT, which is a normal quit in every version since
  23.2; SIGTERM only is since 32.1.
