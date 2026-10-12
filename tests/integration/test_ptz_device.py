# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Grant Likely <grant.likely@secretlab.ca>
"""Covers the NDI source as a PTZ device, which is the obs-ptz PTZ Device API
(docs/ptz-device-api.md there) on the source's own proc_handler and
signal_handler: that a controller finds it, what it says it can do, and that
what the controller asks for reaches the sender as NDI PTZ commands.

The controller is obs-ptz, loaded in the test OBS (DISTROAV_PTZ_CONTROLLER) and
used over its websocket binding, and the sender is the PtzSender, which says it
has PTZ and records what it is sent. Both are needed, and the tests skip without
them. The directions are NDI's own: its pan speed is -1.0 for right and its
focus speed -1.0 for further where the API's are +1.0, its absolute zoom is 0.0
zoomed in where the API's is wide, and its absolute focus is 0.0 for infinity
where the API's is near.
"""

import time
from collections import namedtuple

import pytest

from ndi_helpers import unique_name
from ptz_helpers import PtzSender

# What this suite is written for: a device at another major version is not used
PTZ_API = {"major": 0, "minor": 10}
KEEP_ACTIVE = 0
ALL_FEATURES = {"pan", "tilt", "zoom", "focus", "pan_abs", "tilt_abs", "zoom_abs", "focus_abs", "autofocus",
                "wb_onepush", "presets"}

Camera = namedtuple("Camera", "name uuid")


@pytest.fixture
def sender():
    s = PtzSender(unique_name("ptz-cam"))
    yield s
    s.stop()


def add_camera(world, ptz, ndi_name="", **settings):
    settings.setdefault("ndi_ptz", True)
    name = world.add_ndi_source(world.create_scene(), ndi_name, ndi_behavior=KEEP_ACTIVE, **settings)
    uuid = ptz.device_uuid(name)
    ptz.wait_for_device(uuid)
    return Camera(name, uuid)


@pytest.fixture
def camera(obs_world, ptz, sender):
    """The NDI source on the sender, once the controller lists it with PTZ features"""
    cam = add_camera(obs_world, ptz, sender.source_name())
    ptz.wait_for_state(cam.uuid, lambda s: s.get("features"), timeout=30)
    sender.clear()
    return cam


def last_command(sender, kind):
    found = [c for c in sender.commands() if c["kind"] == kind]
    return found[-1]["values"] if found else None


def wait_for_command(world, sender, kind, expected=None, timeout=5):
    def seen():
        values = last_command(sender, kind)
        return values is not None and (expected is None or values == expected)

    world.wait_for(seen, timeout=timeout, interval=0.05)
    return last_command(sender, kind)


def set_settings(world, source, settings):
    world.ws.call("SetInputSettings", {"inputName": source, "inputSettings": settings, "overlay": True})


def test_a_source_with_no_connection_is_a_device(obs_world, ptz):
    cam = add_camera(obs_world, ptz)
    entry = ptz.listed(cam.uuid)
    assert entry["source"] == cam.name
    assert entry["api"] == PTZ_API
    version = ptz.call("ptz_get_api_version", cam.uuid)
    assert {"major": version["major"], "minor": version["minor"]} == PTZ_API
    state = ptz.state(cam.uuid)
    assert state["connected"] is False
    assert state["source"] == cam.name
    assert state["features"] == {}


def test_features_are_what_ndi_ptz_can_do(ptz, camera):
    state = ptz.state(camera.uuid)
    assert state["connected"] is True
    assert {k for k, v in state["features"].items() if v} == ALL_FEATURES
    # The features of an older version, one for pan and tilt together, are not in it
    for absent in ("pantilt", "pantilt_abs", "pantilt_rel", "pan_rel", "tilt_rel", "home", "home_set", "power",
                   "diagnostics", "tally_light", "move_completion"):
        assert absent not in state["features"]


def test_a_sender_without_ptz_gives_no_features(obs_world, ptz):
    s = PtzSender(unique_name("ptz-cam-none"), ptz=False)
    try:
        cam = add_camera(obs_world, ptz, s.source_name())
        time.sleep(3)
        state = ptz.state(cam.uuid)
        assert state["features"] == {}
        assert state["connected"] is False
    finally:
        s.stop()


def test_the_api_says_what_the_device_has(ptz, camera):
    info = ptz.call("ptz_get_api_info", camera.uuid)["return"]
    procs = {p["name"]: p for p in info["procs"]}
    # Not the procs of what it cannot do
    for absent in ("ptz_home_recall", "ptz_home_save", "ptz_move_rel"):
        assert absent not in procs
    assert {"ptz_move", "ptz_move_abs", "ptz_stop", "ptz_get_state", "ptz_preset_get_thumbnail"} <= set(procs)
    assert (procs["ptz_get_state"]["thread"], procs["ptz_get_state"]["access"]) == ("any", "read")
    assert procs["ptz_move"]["decl"] == "void ptz_move(float pan, float tilt, float zoom, float focus)"
    assert procs["ptz_get_parent_source"]["access"] == "none"
    assert procs["ptz_preset_create"]["thread"] == "any"
    signals = {s["name"] for s in info["signals"]}
    assert {"ptz_state_changed", "ptz_preset_recalled", "ptz_preset_create_done"} <= signals
    # A device does not register the signals of the global handler
    assert not {"ptz_device_add", "ptz_device_remove"} & signals


def test_a_source_made_after_the_controller_is_announced(obs_world, ptz):
    ptz.mark()
    cam = add_camera(obs_world, ptz)
    obs_world.wait_for(lambda: ptz.events("ptz_device_add", cam.uuid), timeout=10)


def test_switching_ptz_off_and_on_removes_and_adds_the_device(obs_world, ptz):
    cam = add_camera(obs_world, ptz)
    ptz.mark()
    set_settings(obs_world, cam.name, {"ndi_ptz": False})
    obs_world.wait_for(lambda: ptz.events("ptz_device_remove", cam.uuid), timeout=10)
    ptz.wait_for_device(cam.uuid, present=False)

    set_settings(obs_world, cam.name, {"ndi_ptz": True})
    obs_world.wait_for(lambda: ptz.events("ptz_device_add", cam.uuid), timeout=10)
    ptz.wait_for_device(cam.uuid)


def test_a_source_with_ptz_off_is_not_a_device(obs_world, ptz):
    ptz.mark()
    name = obs_world.add_ndi_source(obs_world.create_scene(), "", ndi_behavior=KEEP_ACTIVE, ndi_ptz=False)
    uuid = ptz.device_uuid(name)
    obs_world.wait_for(lambda: ptz.listed(uuid) is None, timeout=5)
    assert not ptz.events("ptz_device_add", uuid)


def test_removing_the_source_removes_the_device(obs_world, ptz):
    cam = add_camera(obs_world, ptz)
    ptz.mark()
    obs_world.ws.call("RemoveInput", {"inputName": cam.name})
    obs_world.wait_for(lambda: ptz.events("ptz_device_remove", cam.uuid), timeout=10)
    ptz.wait_for_device(cam.uuid, present=False)


def test_a_move_gives_the_other_axis_what_it_had(obs_world, ptz, camera, sender):
    ptz.call("ptz_move", camera.uuid, pan=0.5)
    # NDI's pan speed is -1.0 for right, where the API's is +1.0
    assert wait_for_command(obs_world, sender, "pan_tilt_speed") == {"pan_speed": -0.5, "tilt_speed": 0.0}

    ptz.call("ptz_move", camera.uuid, tilt=0.25)
    wait_for_command(obs_world, sender, "pan_tilt_speed", {"pan_speed": -0.5, "tilt_speed": 0.25})

    ptz.call("ptz_stop", camera.uuid)
    wait_for_command(obs_world, sender, "pan_tilt_speed", {"pan_speed": 0.0, "tilt_speed": 0.0})
    # a stop is every axis: pan alone after it has tilt at zero
    ptz.call("ptz_move", camera.uuid, pan=0.25)
    wait_for_command(obs_world, sender, "pan_tilt_speed", {"pan_speed": -0.25, "tilt_speed": 0.0})


def test_stop_zeroes_every_axis(obs_world, ptz, camera, sender):
    ptz.call("ptz_move", camera.uuid, pan=0.5, tilt=0.5, zoom=0.5)
    wait_for_command(obs_world, sender, "zoom_speed", {"zoom_speed": 0.5})

    sender.clear()
    ptz.call("ptz_stop", camera.uuid)
    wait_for_command(obs_world, sender, "pan_tilt_speed", {"pan_speed": 0.0, "tilt_speed": 0.0})
    wait_for_command(obs_world, sender, "zoom_speed", {"zoom_speed": 0.0})


def test_focus_speed_is_the_way_its_position_goes(obs_world, ptz, camera, sender):
    ptz.call("ptz_move", camera.uuid, focus=0.5)
    # a positive speed raises the position, towards infinity; NDI's focus speed is the other way
    assert wait_for_command(obs_world, sender, "focus_speed")["distance"] == -0.5


def test_an_absolute_move_is_in_ndis_directions(obs_world, ptz, camera, sender):
    ptz.call("ptz_move_abs", camera.uuid, pan=0.25, tilt=-0.5, zoom=0.75, focus=0.75)
    wait_for_command(obs_world, sender, "zoom")
    wait_for_command(obs_world, sender, "focus")
    assert last_command(sender, "pan_tilt") == {"pan": 0.25, "tilt": -0.5}
    # NDI's 0.0 is zoomed in, where the API's is wide
    assert last_command(sender, "zoom") == {"zoom": 0.25}
    # and its 0.0 is infinity, where the API's is near
    assert last_command(sender, "focus") == {"mode": "manual", "distance": 0.25}


def test_pan_alone_does_not_move_when_the_camera_has_no_position(obs_world, ptz, camera, sender):
    """The other axis would be where the camera is, which the state would say, and NDI says nothing"""
    assert "pan" not in ptz.state(camera.uuid)
    ptz.call("ptz_move_abs", camera.uuid, pan=0.5)
    ptz.call("ptz_move_abs", camera.uuid, zoom=0.5)
    wait_for_command(obs_world, sender, "zoom")
    assert last_command(sender, "pan_tilt") is None


def test_the_speed_cap_and_inversion_are_applied_to_each_axis(obs_world, ptz, camera, sender):
    set_settings(obs_world, camera.name, {"pan_speed_max": 0.5, "pan_invert": True})
    time.sleep(0.5)
    ptz.call("ptz_move", camera.uuid, pan=1.0, tilt=-1.0)
    # right at full speed is -1.0 in NDI, capped to -0.5, and the inversion turns it round; tilt has its own cap
    assert wait_for_command(obs_world, sender, "pan_tilt_speed") == {"pan_speed": 0.5, "tilt_speed": -1.0}


def test_the_cap_of_an_older_version_is_for_pan_and_tilt(obs_world, ptz, camera, sender):
    set_settings(obs_world, camera.name, {"pantilt_speed_max": 0.5})
    time.sleep(0.5)
    ptz.call("ptz_move", camera.uuid, pan=1.0, tilt=-1.0)
    assert wait_for_command(obs_world, sender, "pan_tilt_speed") == {"pan_speed": -0.5, "tilt_speed": -0.5}


def test_the_newest_of_quick_moves_is_what_is_sent(obs_world, ptz, camera, sender):
    for i in range(1, 11):
        ptz.call("ptz_move", camera.uuid, pan=i / 10)
    wait_for_command(obs_world, sender, "pan_tilt_speed", {"pan_speed": -1.0, "tilt_speed": 0.0})


def test_autofocus_and_white_balance_reach_the_sender(obs_world, ptz, camera, sender):
    ptz.call("ptz_trigger", camera.uuid, name="wb_onepush")
    wait_for_command(obs_world, sender, "white_balance", {"mode": "one_shot"})
    ptz.call("ptz_request_state", camera.uuid, state={"focus_af_enabled": True})
    wait_for_command(obs_world, sender, "focus", {"mode": "auto"})
    obs_world.wait_for(lambda: ptz.state(camera.uuid).get("focus_af_enabled") is True, timeout=5, interval=0.2)


def test_the_source_sliders_reach_the_sender(obs_world, ptz, sender):
    """The source's own pan, tilt and zoom settings, which were the whole of its PTZ before it was a device"""
    cam = add_camera(obs_world, ptz, sender.source_name())
    set_settings(obs_world, cam.name, {"ndi_ptz": True, "ndi_pan": 0.25, "ndi_tilt": -0.5, "ndi_zoom": 0.75})
    # The sliders as they were are sent first, when PTZ is on from the start
    wait_for_command(obs_world, sender, "zoom", {"zoom": 0.75}, timeout=15)
    assert last_command(sender, "pan_tilt") == {"pan": 0.25, "tilt": -0.5}


def test_the_state_follows_the_source_being_renamed(obs_world, ptz, camera):
    ptz.mark()
    renamed = f"{camera.name}-renamed"
    obs_world.ws.call("SetInputName", {"inputName": camera.name, "newInputName": renamed})
    obs_world.wait_for(lambda: ptz.state(camera.uuid)["source"] == renamed, timeout=5, interval=0.2)
    obs_world.wait_for(lambda: [e for e in ptz.events("ptz_state_changed", camera.uuid)
                                if e["changed"].get("source") == renamed], timeout=5)
    assert ptz.listed(camera.uuid)["source"] == renamed


def test_presets_use_camera_slots_and_recall_sends_the_slot(obs_world, ptz, camera, sender):
    listed = ptz.preset_list(camera.uuid)
    assert listed["stores"] == {"camera": True}
    assert listed["enumerable"] is False

    first = ptz.create_preset(camera.uuid, "one")
    second = ptz.create_preset(camera.uuid, "two")
    assert (first, second) == ("camera:0", "camera:1")
    wait_for_command(obs_world, sender, "store_preset", {"index": 1})

    ptz.call("ptz_preset_recall", camera.uuid, id=first)
    assert wait_for_command(obs_world, sender, "recall_preset")["index"] == 0

    presets = ptz.preset_list(camera.uuid)["presets"]
    assert {p["name"] for p in presets.values()} == {"one", "two"}


def test_all_the_slots_used_refuses_another_and_delete_frees_one(ptz, camera):
    made = [ptz.create_preset(camera.uuid, f"p{i}") for i in range(100)]
    assert made[0] == "camera:0" and made[-1] == "camera:99"
    assert ptz.create_preset(camera.uuid, "one too many") == ""

    ptz.call("ptz_preset_delete", camera.uuid, id="camera:7")
    assert ptz.create_preset(camera.uuid, "again") == "camera:7"


def test_presets_a_source_is_made_with_are_listed(obs_world, ptz):
    # Over obs-websocket, which makes the source off OBS's UI thread
    cam = add_camera(obs_world, ptz, presets=[{"id": "camera:3", "name": "Wide"}, {"id": "camera:1", "name": "Stage"}],
                     preset_order=[{"id": "camera:3"}, {"id": "camera:1"}])
    listed = ptz.preset_list(cam.uuid)
    assert [o["id"] for o in listed["order"]] == ["camera:3", "camera:1"]
    assert listed["presets"]["camera:3"]["name"] == "Wide"
    assert listed["presets"]["camera:3"]["thumbnail"] == ""


def test_there_is_no_thumbnail_to_get_or_to_take(ptz, camera):
    first = ptz.create_preset(camera.uuid, "one")
    got = ptz.call("ptz_preset_get_thumbnail", camera.uuid, id=first)
    assert (got["mime"], got["data"]) == ("", "")
    ptz.call("ptz_preset_capture_thumbnail", camera.uuid, id=first, if_arrived=False)
    assert ptz.preset_list(camera.uuid)["presets"][first]["thumbnail"] == ""


def test_features_are_announced_again_after_the_sender_restarts(ptz, camera, sender):
    name = sender.name
    ptz.mark()
    sender.stop()
    ptz.wait_for_state(camera.uuid, lambda s: not s["features"], timeout=30)

    revived = PtzSender(name)
    try:
        ptz.wait_for_state(camera.uuid, lambda s: s["features"], timeout=30)
        assert [e for e in ptz.events("ptz_state_changed", camera.uuid) if "features" in e["changed"]]
        assert ptz.preset_list(camera.uuid)["stores"] == {"camera": True}
    finally:
        revived.stop()


def test_a_stop_while_the_sender_is_away_is_sent_when_it_is_back(obs_world, ptz, camera, sender):
    ptz.call("ptz_move", camera.uuid, pan=0.5)
    wait_for_command(obs_world, sender, "pan_tilt_speed")
    name = sender.name
    sender.stop()
    ptz.wait_for_state(camera.uuid, lambda s: not s["features"], timeout=30)

    ptz.call("ptz_stop", camera.uuid)
    revived = PtzSender(name)
    try:
        wait_for_command(obs_world, revived, "pan_tilt_speed", {"pan_speed": 0.0, "tilt_speed": 0.0}, timeout=30)
    finally:
        revived.stop()


def recalled(ptz, device):
    return [e["id"] for e in ptz.events("ptz_preset_recalled", device)]


def test_a_recall_is_announced_once_it_has_had_time_to_arrive(obs_world, ptz, camera, sender):
    """NDI says nothing of the camera arriving, so the source guesses, as the API says of a device
    without move_completion: 3 seconds after the recall"""
    first = ptz.create_preset(camera.uuid, "one")
    ptz.mark()
    ptz.call("ptz_preset_recall", camera.uuid, id=first)
    time.sleep(1.5)
    assert recalled(ptz, camera.uuid) == []
    obs_world.wait_for(lambda: recalled(ptz, camera.uuid) == [first], timeout=5)


def test_a_move_before_it_arrives_means_it_never_does(ptz, camera):
    first = ptz.create_preset(camera.uuid, "one")
    ptz.mark()
    ptz.call("ptz_preset_recall", camera.uuid, id=first)
    ptz.call("ptz_move", camera.uuid, pan=0.5)
    time.sleep(4.5)
    assert recalled(ptz, camera.uuid) == []


def test_only_the_last_recall_is_announced(obs_world, ptz, camera):
    first = ptz.create_preset(camera.uuid, "one")
    second = ptz.create_preset(camera.uuid, "two")
    ptz.mark()
    ptz.call("ptz_preset_recall", camera.uuid, id=first)
    ptz.call("ptz_preset_recall", camera.uuid, id=second)
    obs_world.wait_for(lambda: recalled(ptz, camera.uuid), timeout=6)
    time.sleep(1)
    assert recalled(ptz, camera.uuid) == [second]
