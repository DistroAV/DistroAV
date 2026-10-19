<!-- Copyright (C) 2026 Grant Likely <grant.likely@secretlab.ca> -->

# DistroAV integration tests

These tests boot a real OBS Studio with the DistroAV plugin loaded, and check
what the plugin does with NDI streams.
They are strictly external: nothing under `src/` is built differently for them.
OBS is driven only through obs-websocket and the OBS sources, outputs and
filters API.
NDI senders and receivers written for the tests (cyndilib) stand in for the
network.

## Quickstart

This tests the plugin built from your checkout, so it is the code you changed
that is under test.

You need Python 3.10 or later (on Windows an x64 Python, as cyndilib has no
wheel for arm64), OBS Studio, the NDI runtime, and the build tools DistroAV
itself needs.
Build the plugin, then run the suite, from the checkout:

**macOS**

```bash
cmake --preset macos -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build_macos --config RelWithDebInfo
scripts/run-integration-tests.sh
```

It uses the newest OBS release in `/Applications`, and the
`build_macos/rundir/RelWithDebInfo/distroav.plugin` just built.

**Linux**

```bash
cmake --preset ubuntu-x86_64
cmake --build build_x86_64
scripts/run-integration-tests.sh
```

Run it from a logged-in desktop session, as OBS shows a window.
It uses the `obs` on your `PATH`, and the plugin in the `build_*/rundir` folder
of the build.

**Windows (PowerShell)**

```powershell
cmake --preset windows-x64
cmake --build build_x64 --config RelWithDebInfo
robocopy "C:\Program Files\obs-studio" C:\obs-test /E
$env:DISTROAV_OBS_BINARY = "C:\obs-test\bin\64bit\obs64.exe"
scripts\run-integration-tests.ps1
```

The suite makes the OBS it runs portable and keeps its profile in that folder,
so it has to be a copy, not the OBS you use.
It puts the plugin you built into that copy.
If the NDI runtime is not installed, set `NDI_RUNTIME_DIR_V6` to a folder with
`Processing.NDI.Lib.x64.dll`.

The first run makes a virtualenv and installs the requirements, which needs the
network.
A run takes a couple of minutes, and OBS windows come and go while it does.
Arguments after the script go to pytest: `-k smoke` runs part of it, `-v` lists
each test.

Your own OBS profile is not read or changed: the suite runs OBS on a profile of
its own.

If it does not pass:

- "the plugin's NDI outputs were never made": OBS started but DistroAV did not
  load.
  The error says which log to read.
- A test of behavior an older DistroAV does not have fails.
  Persistence over obs-websocket needs a DistroAV with #1472, for example.
  That is a finding, not a fault of the suite.
- "OBS left running by an earlier run": quit the OBS the message names, or set
  `DISTROAV_ALLOW_STRAYS=1`.

### Testing the installed DistroAV instead

To test the DistroAV a machine already has, with nothing built, set
`DISTROAV_INSTALLED_PLUGIN=1`:

```bash
DISTROAV_INSTALLED_PLUGIN=1 scripts/run-integration-tests.sh
```

On Linux OBS loads the plugin from where the package put it.
On macOS the suite uses the one in
`~/Library/Application Support/obs-studio/plugins` or in
`/Library/Application Support/obs-studio/plugins`.
On Windows set `$env:DISTROAV_INSTALLED_PLUGIN = "1"` before the PowerShell
steps above, minus the build: the plugin is taken from
`%ProgramData%\obs-studio\plugins`, where its installer puts it, if the OBS
copy does not have it already.

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
| `DISTROAV_INSTALLED_PLUGIN` | Set to test the DistroAV that is installed, not a build. |
| `DISTROAV_PLUGIN_DIR` | Linux: the folder holding `distroav.so` and `distroav/locale`. Default: a `build_*/rundir/<config>` of this checkout. |
| `NDI_RUNTIME_DIR_V6` | Windows: the folder holding `Processing.NDI.Lib.x64.dll`, if the NDI runtime is not installed (cyndilib ships one, in its `wrapper\bin`). |
| `DISTROAV_PLUGIN_BUNDLE` | macOS: the `distroav.plugin` to test. Default: a `build_*/rundir/<config>/distroav.plugin` of this checkout. |
| `DISTROAV_IT_VENV` | Where the virtualenv lives. |
| `DISTROAV_OBS_START_TIMEOUT` | Seconds OBS has to start and come up (default 30; it takes 2 to 6). |
| `DISTROAV_KEEP_HOME` | Set to keep OBS's isolated home and log after the run. |
| `DISTROAV_ALLOW_STRAYS` | Set to run although an OBS from an earlier run is still alive. |
| `DISTROAV_REQUIRE_CLEAN_QUIT` | Set to fail a run in which OBS exits with a failure status when it quits. |

The plugin needs the NDI runtime installed, as it does for any use.
The tests themselves use cyndilib, which brings its own copy.
On Linux OBS needs a display, so `DISPLAY` must be set.

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

## NDI test sender and receiver

`ndi_helpers.py` has `TestSender`, which sends a solid colour and, once asked, a
sine tone, and `TestReceiver`, which reports the size, colour and audio of what
it receives.
Their own tests (`test_ndi_helpers.py`) need no OBS.
Every name carries a per-run prefix from `unique_name()`, so a test cannot find
a source from the LAN or from another run.
A receiver finds a source by its full `HOST (name)` form or by its bare name.
`SourceWatcher` is opened on a source before what is to make it go, since a
finder made afterwards cannot tell a source that has gone from one that was
never there.

Two things about cyndilib shape them.
A frame cannot be altered while it is on a sender, so
`TestSender.set_resolution()` makes a new sender under the same name; the stream
goes away and comes back.
Audio is read on a receiver of its own, because cyndilib's frame sync returns
silence where its plain receiver returns what was sent.

## What the tests look at

A source's picture is read with obs-websocket's `GetSourceScreenshot`
(`World.source_color()`, `World.wait_for_color()`).
Its audio is read as the level OBS meters on the input, through obs-websocket's
`InputVolumeMeters` events on a connection of its own (`World.audio_peak()`,
`World.wait_for_audio()`).
An input with no meter reading at all is an error, not silence, so a test of
"quiet" cannot pass because the meter never spoke.
Both are polled to a deadline, not waited for with a sleep.

## The plugin's own settings

The fixture writes the `[NDIPlugin]` section of OBS's `global.ini` before
launch, with the Main Output on under a name made for the run
(`World.main_output_name`).
The output is then published as soon as OBS has loaded, and can be stopped and
started over obs-websocket as "NDI Main Output".
`World.add_color_source()` gives a scene a known picture to publish.

## Filters

`World.add_filter()` puts an `ndi_filter` or `ndi_audiofilter` on a source and
names its NDI output, with the plugin's `ndi_filter_ndiname` setting.
The video filter test stacks a red source over a green one and puts the filter
on the green: the filter's output is green while the program mix is red, which
shows it publishes its source alone.
An audio filter's stream has no picture, so a receiver connects to it with
`wait_for_video=False`.

## What is covered

| Module | Covers |
|---|---|
| `test_smoke.py` | The plugin loads and registers its source and filter kinds; OBS restarts on the same profile. |
| `test_source_receive.py` | An NDI source shows what a sender sends, at its size, and follows a change of both. |
| `test_source_audio.py` | A source carries the sender's audio, and is silent when there is none. |
| `test_source_behavior.py` | Keeping or clearing the last frame when the sender goes (`ndi_behavior_timeout`), and recovering when it returns. |
| `test_main_output.py` | The Main Output is published, follows the program scene, stops and starts, and OBS quits cleanly with it on. |
| `test_filter_output.py` | `ndi_filter` publishes its source alone; `ndi_audiofilter` publishes audio; removing the filter ends the stream. |
| `test_preview_output.py` | In studio mode the Preview Output publishes the preview scene. |
| `test_settings_persistence.py` | Source settings and the main output's on/off, also when set over obs-websocket, survive a restart. |
| `test_ndi_helpers.py`, `test_strays.py` | The suite's own helpers and guard, with no OBS. |

Not covered: the `ndi_behavior` setting (what the source does while not
visible), the dialogs, the update check, and PTZ and tally.
Nothing here asserts latency or dropped frames: those are timing properties and
noisy on shared hosts.

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

Linux, macOS and Windows.

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

OBS's own colour source is not as bright on every platform: white comes out as
113 in OBS 32.2's own screenshot of the scene on macOS.
The tests that look at a picture OBS drew therefore compare which colour it is
(`dominant_channel()`), not how bright.

### Windows

`scripts/run-integration-tests.ps1` runs the suite; arguments go to pytest.

- **A copy of OBS made for testing.**
  A portable OBS keeps its profile in the folder it is in, which is the only way
  to give a run a profile of its own on Windows, as OBS uses your own
  `%APPDATA%` otherwise.
  Unzip an OBS release somewhere (not the one you use), and point
  `DISTROAV_OBS_BINARY` at its `bin\64bit\obs64.exe`.
  The suite makes it portable, keeps the last run's profile as `config.prev`,
  and puts the plugin beside OBS's own in `obs-plugins\64bit`, since a portable
  OBS does not scan the profile for plugins.
- **The plugin and the NDI runtime.**
  Build with `cmake --preset windows-x64`, then
  `cmake --build build_x64 --config RelWithDebInfo`.
  The plugin needs the NDI runtime, installed or in `NDI_RUNTIME_DIR_V6`.
- **The architecture is one.**
  OBS, the plugin and the Python that runs the tests are all x64, or all arm64:
  Windows arm64 runs x64 programs, and cyndilib has a wheel only for x64, so on
  an arm64 machine use an x64 Python and x64 OBS.
- **The session has a desktop.**
  OBS shows a window, so the suite runs in a logged-in session, not from a
  service.

OBS is asked to close, with `taskkill`, and ended with `taskkill /F` if it has
not in 30 seconds.

