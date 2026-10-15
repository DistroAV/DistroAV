# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Grant Likely <grant.likely@secretlab.ca>
"""Covers the NDI Preview Output: in studio mode it publishes the preview
scene, apart from the program mix. Each test turns studio mode on and off
again itself, so that no other test finds OBS in it.
"""

import pytest
from ndi_helpers import TestReceiver, dominant_channel

# OBS colour settings are 0xAABBGGRR
GREEN = 0xFF00FF00
RED = 0xFF0000FF


def is_green(color):
    return dominant_channel(color) == "g"


def is_red(color):
    return dominant_channel(color) == "r"


@pytest.fixture
def studio_mode(obs_world):
    obs_world.ws.call("SetStudioModeEnabled", {"studioModeEnabled": True})
    try:
        yield
    finally:
        obs_world.ws.call("SetStudioModeEnabled", {"studioModeEnabled": False})


def test_preview_output_publishes_the_preview_scene(obs_world, studio_mode):
    preview = obs_world.create_scene()
    obs_world.add_color_source(preview, GREEN)
    program = obs_world.create_scene()
    obs_world.add_color_source(program, RED)
    obs_world.ws.call("SetCurrentProgramScene", {"sceneName": program})
    obs_world.ws.call("SetCurrentPreviewScene", {"sceneName": preview})

    with TestReceiver(obs_world.preview_output_name) as rx_preview, \
            TestReceiver(obs_world.main_output_name) as rx_main:
        rx_preview.connect(timeout=20)
        rx_main.connect(timeout=20)
        rx_preview.wait_for_color(is_green)
        rx_main.wait_for_color(is_red)
