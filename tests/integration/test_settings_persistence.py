# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Grant Likely <grant.likely@secretlab.ca>
"""Covers what the plugin keeps across OBS quitting and starting again: a
source's settings, and whether the main output is on, including when that
was changed over obs-websocket (which saves the plugin's config, #1472).
"""

import pytest
from ndi_helpers import TestReceiver

OUTPUT = "NDI Main Output"

# What the plugin's source settings take; see src/ndi-source.cpp
SETTINGS = {
    "ndi_behavior": 2,  # pause when not visible
    "ndi_behavior_timeout": 0,  # clear after the sender goes
    "ndi_bw_mode": 1,  # lowest bandwidth
    "ndi_sync": 1,  # NDI timestamp
    "latency": 1,  # low
}


def test_source_settings_survive_a_restart(obs_world):
    source = obs_world.add_ndi_source(obs_world.create_scene(), "NOWHERE (no-such-sender)", **SETTINGS)
    obs_world.restart_obs()

    settings = obs_world.ws.call("GetInputSettings", {"inputName": source})["inputSettings"]
    for key, value in SETTINGS.items():
        assert settings[key] == value, key
    assert settings["ndi_source_name"] == "NOWHERE (no-such-sender)"


def test_main_output_stopped_over_websocket_stays_off_after_a_restart(obs_world, output_on):
    obs_world.ws.call("StopOutput", {"outputName": OUTPUT})
    obs_world.restart_obs()

    # The plugin's own account of it, then the network's: the receiver alone
    # would pass if the output came up later than its window, or if discovery
    # in this process were not working at all
    assert not obs_world.ws.call("GetOutputStatus", {"outputName": OUTPUT})["outputActive"]
    with pytest.raises(TimeoutError):
        with TestReceiver(obs_world.main_output_name) as rx:
            rx.connect(timeout=8)
    assert not obs_world.ws.call("GetOutputStatus", {"outputName": OUTPUT})["outputActive"]


def test_main_output_started_over_websocket_is_on_after_a_restart(obs_world, output_on):
    obs_world.ws.call("StopOutput", {"outputName": OUTPUT})
    obs_world.restart_obs()
    obs_world.ws.call("StartOutput", {"outputName": OUTPUT})
    obs_world.restart_obs()

    with TestReceiver(obs_world.main_output_name) as rx:
        rx.connect(timeout=20)
