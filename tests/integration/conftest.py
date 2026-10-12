# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Grant Likely <grant.likely@secretlab.ca>
"""Fixtures that boot one real OBS Studio, with the plugin built from this
checkout loaded, for the whole test session.

OBS runs with an isolated $HOME, so the user's own profile is untouched,
and with obs-websocket enabled by a config file written before launch.
That file and the plugin's own settings are the only things this suite
writes by hand: scenes and sources are made live over obs-websocket, so
the suite does not depend on OBS's scene-collection JSON schema, which
changes between versions.

The plugin is looked for in $DISTROAV_PLUGIN_DIR, or in a build_*/rundir
folder of this checkout: a folder that holds distroav.so and
distroav/locale, which is what the CMake build leaves there. OBS is
$DISTROAV_OBS_BINARY, or the obs on the PATH.

On macOS the plugin is a bundle: $DISTROAV_PLUGIN_BUNDLE, or a
build_*/rundir/<config>/distroav.plugin of this checkout, and OBS is
$DISTROAV_OBS_BINARY, or the newest release in /Applications.
"""

import base64
import os
import platform
import shutil
import signal
import socket
import subprocess
import sys
import tempfile
import time
import uuid
import warnings
from io import BytesIO
from pathlib import Path

import pytest
from obsws import EVENT_INPUT_VOLUME_METERS, Client, ObsWebSocketError, connect_when_ready
from PIL import Image, ImageStat

REPO_ROOT = Path(__file__).resolve().parents[2]

IS_MAC = platform.system() == "Darwin"
IS_WIN = platform.system() == "Windows"

# What the build calls the plugin
PLUGIN_LIB = "distroav.dll" if IS_WIN else "distroav.so"

# What every OBS this suite starts is given for obs-websocket's password,
# so that one left running by an earlier run can be told from the user's own
STRAY_MARKER = "distroav-it-"

# obs-websocket RequestStatus::NotReady, returned while OBS's frontend is
# still starting up, even after the websocket handshake has completed
OBS_NOT_READY = 207

# The names the plugin gives its outputs
NDI_OUTPUTS = {"NDI Main Output", "NDI Preview Output"}


def obs_processes():
    """(pid, parent pid, how long it has run, command) of each OBS this suite
    started on this computer, whoever ran it. None where there is no ps to
    ask."""
    try:
        out = subprocess.run(["ps", "-eo", "pid=,ppid=,etime=,command="], capture_output=True, text=True,
                             check=True).stdout
    except (OSError, subprocess.CalledProcessError):
        return None
    found = []
    for line in out.splitlines():
        fields = line.split(None, 3)
        if len(fields) < 4 or int(fields[0]) == os.getpid():
            continue
        argv = fields[3].split()
        # obs itself, not a shell whose command line mentions it
        if os.path.basename(argv[0]).lower().startswith("obs") and any(a.startswith(STRAY_MARKER) for a in argv):
            found.append((int(fields[0]), int(fields[1]), fields[2], fields[3]))
    return found


# The OBSes there were when the session started, which it didn't start and
# isn't to blame for
_strays = []


def pytest_sessionstart(session):
    """Stop, before booting anything, if an OBS is left over from an earlier
    run. One that is still running holds the NDI names and the ports the
    tests expect, and they then fail in ways that look like nothing at all,
    only some of the time. Set DISTROAV_ALLOW_STRAYS=1 to run anyway."""
    found = obs_processes()
    if not found:
        return
    _strays.extend(pid for pid, *_ in found)
    lines = "\n".join(f"  {pid} (parent {ppid}, running {etime}): {command[:100]}"
                      for pid, ppid, etime, command in found)
    message = (f"{len(found)} OBS left running by an earlier run:\n{lines}\n"
               "Stop them (kill " + " ".join(str(pid) for pid, *_ in found) + ") and run again, "
               "or set DISTROAV_ALLOW_STRAYS=1 to run with them.")
    if os.environ.get("DISTROAV_ALLOW_STRAYS"):
        print(f"\nWARNING: {message}", file=sys.stderr)
    else:
        pytest.exit(message, returncode=3)


def pytest_sessionfinish(session, exitstatus):
    """Say if the session left an OBS running, which the next run is then
    stopped by"""
    left = [f for f in obs_processes() or [] if f[0] not in _strays]
    if left:
        lines = "\n".join(f"  {pid}: {command[:100]}" for pid, _, _, command in left)
        print(f"\nWARNING: this run left {len(left)} OBS running:\n{lines}", file=sys.stderr)


def pytest_collection_modifyitems(config, items):
    """Skip what needs OBS where the suite is not yet written to run it"""
    if platform.system() in ("Linux", "Darwin", "Windows"):
        return
    skip = pytest.mark.skip(reason=f"OBS is launched only on Linux, macOS and Windows, not on {platform.system()}")
    for item in items:
        if "obs_world" in item.fixturenames:
            item.add_marker(skip)


def loaded_plugin_problem(log_text: str):
    """What is wrong if OBS skipped our distroav for another of its name (a
    system-installed one, say), which would have the whole suite test the wrong
    build; None if nothing is."""
    for line in log_text.splitlines():
        if "Skipping plugin" in line and "distroav" in line:
            return line.strip()
    return None


def newest_mac_obs():
    """The newest release OBS in /Applications, not a release candidate or a
    beta: those are built against other Qt libraries than a plugin built from
    the pinned dependencies"""
    apps = [p for p in Path("/Applications").glob("OBS [0-9]*.app")
            if not any(tag in p.name for tag in ("rc", "beta"))]
    apps.sort(key=lambda p: [int(n) for n in p.name.split()[1].removesuffix(".app").split(".") if n.isdigit()])
    return str(apps[-1] / "Contents" / "MacOS" / "OBS") if apps else None


def pytest_report_header(config):
    if IS_MAC:
        given = os.environ.get("DISTROAV_PLUGIN_BUNDLE")
        found = sorted(REPO_ROOT.glob("build_*/rundir/*/distroav.plugin"))
        return f"distroav plugin under test: {given or (found[0] if found else 'none found')}"
    given = os.environ.get("DISTROAV_PLUGIN_DIR")
    found = sorted(p.parent for p in REPO_ROOT.glob(f"build_*/rundir/*/{PLUGIN_LIB}"))
    return f"distroav plugin under test: {given or (found[0] if found else 'none found')}"


def obs_start_timeout() -> float:
    """How long OBS has to start, answer on the websocket and make its NDI
    outputs. It takes 2 to 6 seconds on every machine this was tried on,
    including an emulated x64 Windows, so 30 is generous and a hang is not
    waited out for minutes."""
    return float(os.environ.get("DISTROAV_OBS_START_TIMEOUT", "30"))


def free_port():
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def find_plugin_bundle():
    """macOS: the distroav.plugin to test"""
    given = os.environ.get("DISTROAV_PLUGIN_BUNDLE")
    candidates = [Path(given)] if given else sorted(REPO_ROOT.glob("build_*/rundir/*/distroav.plugin"))
    for candidate in candidates:
        if candidate.is_dir():
            return candidate
    pytest.exit("No built plugin found: set DISTROAV_PLUGIN_BUNDLE to the distroav.plugin to test "
                "(a build_*/rundir/<config> folder holds it).", returncode=4)


def find_plugin_dir():
    given = os.environ.get("DISTROAV_PLUGIN_DIR")
    candidates = [Path(given)] if given else sorted(p.parent for p in REPO_ROOT.glob(f"build_*/rundir/*/{PLUGIN_LIB}"))
    for candidate in candidates:
        if (candidate / PLUGIN_LIB).is_file() and (candidate / "distroav" / "locale").is_dir():
            return candidate
    pytest.exit(f"No built plugin found: set DISTROAV_PLUGIN_DIR to the folder holding {PLUGIN_LIB} "
                "and distroav/locale (a build_*/rundir/<config> folder).", returncode=4)


def obs_config_root(home: Path, system=None) -> Path:
    """Where OBS keeps its profile for a home folder"""
    system = system or platform.system()
    if system == "Darwin":
        return home / "Library" / "Application Support" / "obs-studio"
    if system == "Windows":
        # A portable install keeps its profile in the folder it is in
        return home / "config" / "obs-studio"
    return home / ".config" / "obs-studio"


def portable_home(binary: Path) -> Path:
    """Windows: the folder of the OBS install at `binary` (bin/64bit/obs64.exe),
    made portable and given a profile of its own. OBS uses the user's own
    %APPDATA% however the environment says otherwise, unless portable_mode.txt
    is next to its bin folder, when it keeps everything in a config folder
    there. So the install is one made for testing, and the profile the last
    run left is kept as config.prev for one run, as the one before is gone."""
    home = Path(binary).resolve().parents[2]
    (home / "portable_mode.txt").touch()
    config = home / "config"
    previous = home / "config.prev"
    if config.exists():
        if previous.exists():
            shutil.rmtree(previous)
        config.rename(previous)
    return home


def windows_quit_command(pid: int, force: bool = False):
    """Without /F taskkill asks the program's windows to close, which for OBS
    is a normal quit; with it, it ends the program"""
    return ["taskkill", "/PID", str(pid), "/T"] + (["/F"] if force else [])


def install_plugin_bundle(config_dir: Path, bundle: Path):
    """macOS: a copy of the bundle in the profile's plugin folder, signed
    ad hoc. A build straight from CMake has no signature at all, and an arm64
    Mac will not load code that has none; a copy is signed, rather than the
    build, so that what was built is left as it was."""
    plugins = config_dir / "plugins"
    plugins.mkdir(parents=True, exist_ok=True)
    target = plugins / "distroav.plugin"
    shutil.copytree(bundle, target, symlinks=True)
    result = subprocess.run(["codesign", "--force", "--sign", "-", "--options", "runtime", str(target)],
                            capture_output=True, text=True)
    if result.returncode != 0:
        pytest.exit(f"could not sign the plugin bundle: {result.stderr.strip()}", returncode=4)


def install_plugin_windows(install: Path, plugin_dir: Path):
    """Windows: the plugin beside OBS's own, in the install, which is where a
    portable OBS looks: it does not scan the profile's plugin folder. The
    install is one made for testing, and what is there is replaced."""
    dll_dir = install / "obs-plugins" / "64bit"
    data_dir = install / "data" / "obs-plugins" / "distroav"
    dll_dir.mkdir(parents=True, exist_ok=True)
    shutil.copy2(plugin_dir / PLUGIN_LIB, dll_dir / PLUGIN_LIB)
    if data_dir.exists():
        shutil.rmtree(data_dir)
    data_dir.mkdir(parents=True)
    shutil.copytree(plugin_dir / "distroav" / "locale", data_dir / "locale")


def install_plugin(config_dir: Path, plugin_dir: Path):
    """Put the build into the isolated profile's own plugin folder, as a real
    copy: OBS's plugin scan skips a symlinked .so"""
    root = config_dir / "plugins" / "distroav"
    (root / "bin" / "64bit").mkdir(parents=True)
    shutil.copy2(plugin_dir / PLUGIN_LIB, root / "bin" / "64bit" / PLUGIN_LIB)
    shutil.copytree(plugin_dir / "distroav" / "locale", root / "data" / "locale")


def write_obs_config(config_dir: Path, ws_port, ws_password, main_output_name):
    """Pre-seed obs-websocket's settings, so that it is enabled on first launch,
    and the plugin's, with its Main Output on under a name of this run's"""
    config_dir.mkdir(parents=True, exist_ok=True)
    # The plugin's strings are en-US's: the language OBS is in must not be
    # one with a file of its own that takes over
    # On macOS and Windows, OBS opens its first-run wizard, a modal dialog, unless it has
    # run before (FirstRun), and another asking for permissions it will never
    # be given, unless it has shown that already (a dialog version, so set it
    # high enough to cover any a later OBS has)
    (config_dir / "user.ini").write_text("[General]\nLanguage=en-US\n" + ("FirstRun=true\n" if IS_MAC or IS_WIN else ""))
    (config_dir / "global.ini").write_text(
        ("[General]\nMacOSPermissionsDialogLastShown=1000\n\n" if IS_MAC else "")
        + "[OBSWebSocket]\n"
        "FirstLoad=false\n"
        "ServerEnabled=true\n"
        f"ServerPort={ws_port}\n"
        "AlertsEnabled=false\n"
        "AuthRequired=false\n"
        f"ServerPassword={ws_password}\n"
        "\n[NDIPlugin]\n"
        "MainOutputEnabled=true\n"
        f"MainOutputName={main_output_name}\n"
        "PreviewOutputEnabled=false\n")


class ObsProcess:
    """One OBS, started on a home folder and stoppable and startable again on
    the same one."""

    def __init__(self, binary, home: Path, log_path: Path, ws_port, ws_password, require_clean_exit=False):
        self.binary = binary
        self.log_path = log_path
        self.ws_port = ws_port
        self.ws_password = ws_password
        self.env = dict(os.environ, HOME=str(home))
        if IS_MAC:
            # OBS asks the system for the account's home, which ignores
            # $HOME; Core Foundation honors this
            self.env["CFFIXED_USER_HOME"] = str(home)
        self.proc = None
        self.ws = None
        self.last_returncode = None
        # Whether quitting with a failure status (a crash on quit) is an unclean
        # quit. Off by default: see DISTROAV_REQUIRE_CLEAN_QUIT in the README.
        self.require_clean_exit = require_clean_exit

    def command(self):
        # --multi: without it OBS asks, in a dialog no one answers, whenever
        # any other OBS is running on the machine, the user's own included
        return [self.binary, "--multi", "--disable-updater", "--disable-shutdown-check", "--websocket_port",
                str(self.ws_port), "--websocket_password", self.ws_password]

    def start(self, timeout=None):
        timeout = obs_start_timeout() if timeout is None else timeout
        with open(self.log_path, "ab") as log:
            # Its own process group, so that whatever OBS starts goes when it does
            # A run started in the background has SIGINT ignored, which a child
            # inherits, and which would make OBS ignore the signal that quits it
            if IS_WIN:
                # OBS finds its data folder from where it is run, not from where it is
                self.proc = subprocess.Popen(self.command(), env=self.env, stdout=log, stderr=subprocess.STDOUT,
                                             cwd=Path(self.binary).parent,
                                             creationflags=subprocess.CREATE_NEW_PROCESS_GROUP)
            else:
                self.proc = subprocess.Popen(self.command(), env=self.env, stdout=log, stderr=subprocess.STDOUT,
                                             start_new_session=True,
                                             preexec_fn=lambda: signal.signal(signal.SIGINT, signal.SIG_DFL))
        deadline = time.time() + timeout
        try:
            self.ws = connect_when_ready(f"ws://127.0.0.1:{self.ws_port}", self.ws_password,
                                         timeout=timeout, alive=lambda: self.proc.poll() is None)
        except Exception as e:
            self.stop()
            raise RuntimeError(f"OBS did not come up: {e}; see {self.log_path}") from e

        # The server answers before the frontend has loaded its scene
        # collection, and refuses requests as not ready until it has
        while time.time() < deadline:
            try:
                self.ws.call("GetSceneList")
                break
            except ObsWebSocketError as e:
                if e.status.get("code") != OBS_NOT_READY:
                    raise
                time.sleep(0.2)
        else:
            self.stop()
            raise RuntimeError(f"OBS's frontend never became ready; see {self.log_path}")

        # The plugin makes its outputs from a call it queues when the frontend
        # has finished loading, so they come a moment after OBS is ready
        while time.time() < deadline:
            listed = {o["outputName"] for o in self.ws.call("GetOutputList")["outputs"]}
            if NDI_OUTPUTS <= listed:
                break
            time.sleep(0.1)
        else:
            self.stop()
            raise RuntimeError(f"the plugin's NDI outputs were never made; see {self.log_path}")

        problem = loaded_plugin_problem(self.log_path.read_text(errors="replace"))
        if problem:
            self.stop()
            raise RuntimeError(f"OBS is not running the plugin under test: {problem}")

    def stop(self, timeout=30):
        """Quits OBS the way a user does, and says whether it exited by itself"""
        if self.ws is not None:
            try:
                self.ws.close()
            except Exception:
                pass
            self.ws = None
        if self.proc is None:
            return True
        # SIGINT where OBS closes its main window on it, which is every version
        # since 23.2; SIGTERM is a normal quit only since 32.1
        if IS_WIN:
            subprocess.run(windows_quit_command(self.proc.pid), capture_output=True)
        else:
            try:
                os.killpg(self.proc.pid, signal.SIGINT if IS_MAC else signal.SIGTERM)
            except ProcessLookupError:
                pass
        try:
            self.proc.wait(timeout)
            self.last_returncode = self.proc.returncode
            clean = not self.require_clean_exit or self.proc.returncode == 0
            if self.proc.returncode != 0:
                warnings.warn(f"OBS exited with status {self.proc.returncode} when told to quit "
                              f"(a negative one is the signal that killed it); see {self.log_path}")
        except subprocess.TimeoutExpired:
            clean = False
            if IS_WIN:
                subprocess.run(windows_quit_command(self.proc.pid, force=True), capture_output=True)
            else:
                os.killpg(self.proc.pid, signal.SIGKILL)
            self.proc.wait(10)
        self.proc = None
        return clean


class World:
    """What a test is given: OBS, driven over obs-websocket"""

    def __init__(self, obs: ObsProcess, config_dir: Path, scratch: Path, main_output_name: str):
        self._obs = obs
        self.main_output_name = main_output_name
        self.config_dir = config_dir
        self.scratch = scratch
        self._scenes = []
        self._inputs = []

    @property
    def ws(self) -> Client:
        if self._obs.ws is None:
            raise RuntimeError("OBS is not running: an earlier restart of it failed, and that is the failure "
                               "to look at")
        return self._obs.ws

    @property
    def log_path(self) -> Path:
        return self._obs.log_path

    def restart_obs(self):
        """Quit OBS and start it again on the same profile"""
        clean = self._obs.stop()
        # Start again whatever the quit was like, so that one bad quit is one
        # failure and not the end of every test after it
        self._obs.start()
        assert clean, (f"OBS did not quit cleanly (exit status {self._obs.last_returncode}; None means "
                       "it had to be killed)")

    def wait_for(self, predicate, timeout=5, interval=0.1):
        """Poll until predicate() is true. One that raises counts as not yet,
        but the last such exception is re-raised at the deadline, so a bug in
        the predicate surfaces instead of a bare timeout."""
        deadline = time.time() + timeout
        last_exc = None
        while time.time() < deadline:
            try:
                if predicate():
                    return
            except Exception as e:
                last_exc = e
            time.sleep(interval)
        if last_exc is not None:
            raise last_exc
        raise AssertionError("condition never became true")

    def new_client(self, event_subscriptions=0) -> Client:
        """A connection of its own, for what the shared one cannot be used for
        (events). The caller closes it."""
        return Client(f"ws://127.0.0.1:{self._obs.ws_port}", password=self._obs.ws_password,
                      event_subscriptions=event_subscriptions)

    def add_ndi_source(self, scene: str, ndi_name: str, **settings) -> str:
        """An ndi_source of that NDI name (`HOST (name)`) in `scene`, which is
        made the program scene, so that the source is active and connects"""
        name = f"it-source-{uuid.uuid4().hex[:8]}"
        self.ws.call("CreateInput", {"sceneName": scene, "inputName": name, "inputKind": "ndi_source",
                                     "inputSettings": {"ndi_source_name": ndi_name, **settings}})
        self._inputs.append(name)
        self.ws.call("SetCurrentProgramScene", {"sceneName": scene})
        return name

    def add_color_source(self, scene: str, colour: int) -> str:
        """A colour source over `scene`'s canvas; `colour` is OBS's 0xAABBGGRR"""
        name = f"it-color-{uuid.uuid4().hex[:8]}"
        self.ws.call("CreateInput", {"sceneName": scene, "inputName": name, "inputKind": "color_source_v3",
                                     "inputSettings": {"color": colour, "width": 1280, "height": 720}})
        self._inputs.append(name)
        return name

    def add_filter(self, source: str, kind: str, **settings) -> str:
        """A filter of `kind` on the source (or scene); its name. OBS removes it
        with the source."""
        name = f"it-filter-{uuid.uuid4().hex[:8]}"
        self.ws.call("CreateSourceFilter", {"sourceName": source, "filterName": name, "filterKind": kind,
                                            "filterSettings": settings})
        return name

    def source_screenshot(self, source: str) -> Image.Image:
        """What OBS has of the source now, at the source's own size"""
        data = self.ws.call("GetSourceScreenshot", {"sourceName": source, "imageFormat": "png"})["imageData"]
        return Image.open(BytesIO(base64.b64decode(data.split(",", 1)[1]))).convert("RGB")

    def source_color(self, source: str):
        """(r, g, b) averaged over the source's picture"""
        return tuple(round(v) for v in ImageStat.Stat(self.source_screenshot(source)).mean)

    def wait_for_color(self, source: str, predicate, timeout=15):
        self.wait_for(lambda: predicate(self.source_color(source)), timeout=timeout, interval=0.25)

    def _meter_peaks(self, source: str, seconds: float):
        """Each peak (linear, 1.0 full scale) OBS meters on the input over
        `seconds`"""
        client = self.new_client(EVENT_INPUT_VOLUME_METERS)
        try:
            deadline = time.time() + seconds
            while time.time() < deadline:
                try:
                    data = client.wait_event("InputVolumeMeters", timeout=max(deadline - time.time(), 0.01))
                except TimeoutError:
                    return
                for item in data.get("inputs", []):
                    if item.get("inputName") == source:
                        yield max((ch[1] for ch in item.get("inputLevelsMul", [])), default=0.0)
        finally:
            client.close()

    def audio_peak(self, source: str, seconds: float = 2) -> float:
        """The loudest peak OBS metered on the input in `seconds`. No reading
        at all is an error, not silence: a meter that never reports would make
        any test of quiet pass."""
        peaks = list(self._meter_peaks(source, seconds))
        assert peaks, f"OBS metered no audio level for {source} in {seconds}s"
        return max(peaks)

    def wait_for_audio(self, source: str, timeout=15, threshold=0.01) -> float:
        """The first peak above `threshold` that OBS meters on the input"""
        for peak in self._meter_peaks(source, timeout):
            if peak > threshold:
                return peak
        raise AssertionError(f"no audio above {threshold} on {source} within {timeout}s")

    def create_scene(self) -> str:
        name = f"it-scene-{uuid.uuid4().hex[:8]}"
        self.ws.call("CreateScene", {"sceneName": name})
        self._scenes.append(name)
        return name

    def cleanup_scenes(self):
        if self._obs.ws is None:
            self._inputs.clear()
            self._scenes.clear()
            return
        for name in self._inputs:
            try:
                self.ws.call("RemoveInput", {"inputName": name})
            except ObsWebSocketError:
                pass
        self._inputs.clear()
        for name in self._scenes:
            try:
                self.ws.call("RemoveScene", {"sceneName": name})
            except ObsWebSocketError:
                pass
        self._scenes.clear()


@pytest.fixture(scope="session")
def obs_world(tmp_path_factory):
    if platform.system() == "Linux" and not os.environ.get("DISPLAY"):
        pytest.exit("OBS needs a display: set DISPLAY (xvfb-run works).", returncode=4)
    binary = os.environ.get("DISTROAV_OBS_BINARY") or (newest_mac_obs() if IS_MAC else shutil.which("obs"))
    if not binary:
        pytest.exit("No OBS found: set DISTROAV_OBS_BINARY.", returncode=4)
    plugin = find_plugin_bundle() if IS_MAC else find_plugin_dir()

    # Windows: the OBS install is the home, since that is where a portable
    # one keeps its profile; it is one for testing, and is left in place
    home = portable_home(Path(binary)) if IS_WIN else Path(tempfile.mkdtemp(prefix="distroav-it-home-"))
    config_dir = obs_config_root(home)
    ws_port = free_port()
    ws_password = STRAY_MARKER + uuid.uuid4().hex[:8]
    main_output_name = f"distroav-it-{uuid.uuid4().hex[:6]}-main"
    write_obs_config(config_dir, ws_port, ws_password, main_output_name)
    if IS_WIN:
        install_plugin_windows(home, plugin)
    else:
        (install_plugin_bundle if IS_MAC else install_plugin)(config_dir, plugin)

    scratch = tmp_path_factory.mktemp("world")
    obs = ObsProcess(binary, home, scratch / "obs.log", ws_port, ws_password,
                     require_clean_exit=bool(os.environ.get("DISTROAV_REQUIRE_CLEAN_QUIT")))
    obs.start()
    world = World(obs, config_dir, scratch, main_output_name)
    try:
        yield world
    finally:
        clean = obs.stop()
        if os.environ.get("DISTROAV_KEEP_HOME"):
            print(f"\nOBS home kept at {home}, log at {obs.log_path}", file=sys.stderr)
        elif not IS_WIN:
            shutil.rmtree(home, ignore_errors=True)
        assert clean, "OBS had to be killed at the end of the session"


@pytest.fixture
def output_on(obs_world):
    """For a test that stops the main output: puts it back after, and fails
    if it will not come back, rather than leave it off for the tests after"""
    yield
    obs_world.ws.call("StartOutput", {"outputName": "NDI Main Output"}) if not obs_world.ws.call(
        "GetOutputStatus", {"outputName": "NDI Main Output"})["outputActive"] else None
    obs_world.wait_for(
        lambda: obs_world.ws.call("GetOutputStatus", {"outputName": "NDI Main Output"})["outputActive"],
        timeout=15)


@pytest.fixture(autouse=True)
def _cleanup_scenes_between_tests(request):
    yield
    if "obs_world" in request.fixturenames:
        request.getfixturevalue("obs_world").cleanup_scenes()
