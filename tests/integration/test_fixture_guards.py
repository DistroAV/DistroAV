# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Grant Likely <grant.likely@secretlab.ca>
"""Covers the fixture's own guards, with no OBS: what it takes to be told a
run is not testing what it says, or is not the OBS it believes it quit.
"""

import signal
import subprocess
import time

import pytest
from conftest import IS_WIN, ObsProcess, World, loaded_plugin_problem

posix_only = pytest.mark.skipif(IS_WIN, reason="the fake OBS is a bash script that traps signals")


def fake_obs_exiting_with(tmp_path, code, **kwargs):
    """An ObsProcess whose process quits with `code` when told to; returns
    once it has said that it will"""
    obs = make_obs(tmp_path, **kwargs)
    ready = tmp_path / "ready"
    obs.proc = subprocess.Popen(
        ["bash", "-c", f"trap 'exit {code}' TERM INT; touch {ready}; while :; do sleep 0.1; done"],
        start_new_session=True,
        # a run started in the background has SIGINT ignored, and bash cannot trap what is
        preexec_fn=lambda: signal.signal(signal.SIGINT, signal.SIG_DFL))
    deadline = time.time() + 10
    while not ready.exists():
        assert time.time() < deadline, "the fake OBS never started"
        time.sleep(0.05)
    return obs


def make_obs(tmp_path, **kwargs):
    return ObsProcess("obs", tmp_path, tmp_path / "obs.log", 1, "distroav-it-x", **kwargs)


def test_obs_is_started_next_to_any_other_instance(tmp_path):
    # Without --multi OBS asks, in a dialog nobody answers, when any other
    # OBS on the machine is running, the user's own included
    assert "--multi" in make_obs(tmp_path).command()


@posix_only
def test_obs_that_exits_with_a_failure_on_quit_is_not_clean_when_that_is_required(tmp_path):
    obs = fake_obs_exiting_with(tmp_path, 3, require_clean_exit=True)
    with pytest.warns(UserWarning, match="status 3"):
        assert obs.stop(timeout=10) is False
    assert obs.last_returncode == 3


@posix_only
def test_obs_that_exits_with_a_failure_on_quit_is_recorded_and_warned_of_otherwise(tmp_path):
    obs = fake_obs_exiting_with(tmp_path, 3)
    with pytest.warns(UserWarning, match="status 3"):
        assert obs.stop(timeout=10) is True
    assert obs.last_returncode == 3


@posix_only
def test_obs_that_exits_with_success_on_quit_is_clean(tmp_path):
    assert fake_obs_exiting_with(tmp_path, 0).stop(timeout=10) is True


def test_a_world_whose_obs_is_down_says_so_and_cleans_up_quietly(tmp_path):
    world = World(make_obs(tmp_path), tmp_path, tmp_path)
    world._scenes.append("left-over")
    world.cleanup_scenes()
    with pytest.raises(RuntimeError, match="OBS is not running"):
        world.ws.call("GetVersion")


def test_a_plugin_skipped_for_another_of_its_name_is_reported():
    log = ("12:00:00.000: Skipping plugin 'distroav.so' at '/home/u/.config/obs-studio/plugins/distroav/bin/"
           "64bit', already loaded from '/usr/lib/x86_64-linux-gnu/obs-plugins/distroav.so'\n")
    assert "already loaded from" in loaded_plugin_problem(log)


def test_a_log_without_a_skipped_plugin_has_no_problem():
    assert loaded_plugin_problem("12:00:00.000: [DistroAV] plugin loaded successfully\n") is None


def test_obs_keeps_its_profile_where_the_platform_puts_it(tmp_path):
    from conftest import obs_config_root

    assert obs_config_root(tmp_path, "Linux") == tmp_path / ".config" / "obs-studio"
    assert obs_config_root(tmp_path, "Darwin") == tmp_path / "Library" / "Application Support" / "obs-studio"


def test_obs_keeps_its_profile_beside_a_portable_install_on_windows(tmp_path):
    from conftest import obs_config_root

    assert obs_config_root(tmp_path, "Windows") == tmp_path / "config" / "obs-studio"


def test_a_portable_install_gets_a_fresh_profile_and_keeps_the_last_one(tmp_path):
    from conftest import portable_home

    install = tmp_path / "obs"
    binary = install / "bin" / "64bit" / "obs64.exe"
    binary.parent.mkdir(parents=True)
    binary.touch()
    (install / "config").mkdir()
    (install / "config" / "last-run").touch()
    (install / "config.prev").mkdir()
    (install / "config.prev" / "older").touch()

    home = portable_home(binary)

    assert home == install
    assert (install / "portable_mode.txt").exists()
    assert not (install / "config").exists()  # OBS makes a new one
    assert (install / "config.prev" / "last-run").exists()
    assert not (install / "config.prev" / "older").exists()


def test_windows_asks_obs_to_close_and_only_then_ends_it():
    from conftest import windows_quit_command

    assert windows_quit_command(42) == ["taskkill", "/PID", "42", "/T"]
    assert windows_quit_command(42, force=True) == ["taskkill", "/PID", "42", "/T", "/F"]


def test_a_portable_windows_install_gets_the_plugin_beside_its_own(tmp_path):
    from conftest import PLUGIN_LIB, install_plugin_windows

    build = tmp_path / "build"
    (build / "distroav" / "locale").mkdir(parents=True)
    (build / PLUGIN_LIB).write_text("dll")
    (build / "distroav" / "locale" / "en-US.ini").write_text("x")
    install = tmp_path / "obs"
    (install / "obs-plugins" / "64bit").mkdir(parents=True)

    install_plugin_windows(install, build)

    assert (install / "obs-plugins" / "64bit" / PLUGIN_LIB).read_text() == "dll"
    assert (install / "data" / "obs-plugins" / "distroav" / "locale" / "en-US.ini").exists()
