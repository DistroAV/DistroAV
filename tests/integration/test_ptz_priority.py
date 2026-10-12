# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Grant Likely <grant.likely@secretlab.ca>
"""Covers a PTZ device taking priority over NDI PTZ: while a PTZ Control
filter that is enabled is on the same source as the NDI source, the NDI source
turns its PTZ off, as the API has a device do: it answers no version, says
ptz_device_remove, and does nothing until it says ptz_device_add again. A
controller then offers only the filter. When the filter is removed or
disabled it is on again.

The filter is obs-ptz's Virtual PTZ one, which needs no camera. The device of a
filter is named after its source, so the NDI source's device is found by the UUID
it had before the filter was added. Uses the fixtures of test_ptz_device.py.
"""

import time

import pytest

from obsws import ObsWebSocketError
from test_ptz_device import add_camera, camera, last_command, sender, set_settings, wait_for_command  # noqa: F401

FILTER_KIND = "ca.secretlab.obs-ptz.virtual-ptz"
FILTER_NAME = "Virtual PTZ"


def add_filter(world, source, enabled=True):
    world.ws.call("CreateSourceFilter", {"sourceName": source, "filterName": FILTER_NAME,
                                         "filterKind": FILTER_KIND, "filterSettings": {}})
    if not enabled:
        set_filter_enabled(world, source, False)


def set_filter_enabled(world, source, enabled):
    world.ws.call("SetSourceFilterEnabled", {"sourceName": source, "filterName": FILTER_NAME,
                                             "filterEnabled": enabled})


def remove_filter(world, source):
    try:
        world.ws.call("RemoveSourceFilter", {"sourceName": source, "filterName": FILTER_NAME})
    except ObsWebSocketError:
        pass


def is_stop(command):
    values = command["values"]
    return (command["kind"] in ("pan_tilt_speed", "zoom_speed", "focus_speed") and
            all(v == 0 for k, v in values.items() if k != "mode"))


def gone(ptz, cam):
    """The controller no longer has the NDI source's device"""
    ptz.wait_for_device(cam.uuid, present=False)


def test_a_ptz_filter_turns_the_ndi_source_off(obs_world, ptz, camera, sender):  # noqa: F811
    ptz.mark()
    add_filter(obs_world, camera.name)
    gone(ptz, camera)
    assert ptz.events("ptz_device_remove", camera.uuid)
    # the device is the filter's alone: the one the controller has under that source
    assert [d for d in ptz.devices() if d["source"] == camera.name and d["device"] != camera.uuid]

    sender.clear()
    with pytest.raises(ObsWebSocketError):
        ptz.call("ptz_move", camera.uuid, pan=0.5)
    set_settings(obs_world, camera.name, {"ndi_ptz": True, "ndi_pan": 0.5, "ndi_tilt": 0.25, "ndi_zoom": 0.5})
    time.sleep(1.5)
    # What may still arrive is the stop the source sent as it turned off, a speed of zero on each axis
    assert all(is_stop(c) for c in sender.commands()), sender.commands()


def test_a_move_in_progress_is_stopped_once(obs_world, ptz, camera, sender):  # noqa: F811
    ptz.call("ptz_move", camera.uuid, pan=0.5, tilt=0.25)
    wait_for_command(obs_world, sender, "pan_tilt_speed", {"pan_speed": -0.5, "tilt_speed": 0.25})

    sender.clear()
    add_filter(obs_world, camera.name)
    gone(ptz, camera)
    time.sleep(1)
    assert [c for c in sender.commands() if c["kind"] == "pan_tilt_speed"] == [
        {"kind": "pan_tilt_speed", "values": {"pan_speed": 0.0, "tilt_speed": 0.0}}]


@pytest.mark.parametrize("how", ["remove", "disable"])
def test_control_comes_back(obs_world, ptz, camera, sender, how):  # noqa: F811
    add_filter(obs_world, camera.name)
    gone(ptz, camera)

    ptz.mark()
    if how == "remove":
        remove_filter(obs_world, camera.name)
    else:
        set_filter_enabled(obs_world, camera.name, False)
    ptz.wait_for_device(camera.uuid)
    obs_world.wait_for(lambda: ptz.events("ptz_device_add", camera.uuid), timeout=5)
    ptz.wait_for_state(camera.uuid, lambda s: s["features"], timeout=10)
    sender.clear()
    ptz.call("ptz_move", camera.uuid, pan=0.25)
    wait_for_command(obs_world, sender, "pan_tilt_speed", {"pan_speed": -0.25, "tilt_speed": 0.0})


def test_a_disabled_filter_never_turns_it_off(obs_world, ptz, camera, sender):  # noqa: F811
    add_filter(obs_world, camera.name, enabled=False)
    time.sleep(1.5)
    assert ptz.listed(camera.uuid)
    assert ptz.state(camera.uuid)["features"]
    ptz.call("ptz_move", camera.uuid, pan=0.5)
    wait_for_command(obs_world, sender, "pan_tilt_speed", {"pan_speed": -0.5, "tilt_speed": 0.0})


def test_toggling_the_filter_leaves_the_source_working(obs_world, ptz, camera, sender):  # noqa: F811
    """The sender's own frame rate is only checked to see that the receiver does not stall
    it: the frames OBS receives are not counted"""
    before = sender.frames_sent()
    deadline = time.time() + 3
    toggles = 0
    while time.time() < deadline:
        add_filter(obs_world, camera.name)
        remove_filter(obs_world, camera.name)
        toggles += 1
    assert toggles >= 20
    # 30 fps for three seconds, with room for the sender starting late
    assert sender.frames_sent() - before > 60
    ptz.wait_for_device(camera.uuid)
    ptz.wait_for_state(camera.uuid, lambda s: s["features"], timeout=10)


def test_moves_racing_the_filter_leave_a_stop_last(obs_world, ptz, camera, sender):  # noqa: F811
    sender.clear()
    for _ in range(40):
        ptz.call("ptz_move", camera.uuid, pan=0.5)
    add_filter(obs_world, camera.name)
    for _ in range(40):
        try:
            ptz.call("ptz_move", camera.uuid, pan=0.5)
        except ObsWebSocketError:
            break  # it is off: the controller has no such device
    gone(ptz, camera)
    time.sleep(1.5)
    moves = [c["values"] for c in sender.commands() if c["kind"] == "pan_tilt_speed"]
    assert moves[-1] == {"pan_speed": 0.0, "tilt_speed": 0.0}


def test_deleting_a_source_with_a_filter_is_clean(obs_world, ptz, sender):  # noqa: F811
    for _ in range(15):
        name = obs_world.add_ndi_source(obs_world.create_scene(), sender.source_name(), ndi_behavior=0)
        add_filter(obs_world, name)
        obs_world.ws.call("RemoveInput", {"inputName": name})
    assert obs_world.ws.call("GetVersion")["obsVersion"]
