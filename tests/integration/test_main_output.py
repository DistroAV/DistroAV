# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Grant Likely <grant.likely@secretlab.ca>
"""Covers the NDI Main Output: that the program mix is published under the
configured name, follows the program scene, and stops and starts with its
output.
"""

from ndi_helpers import SourceWatcher, TestReceiver, dominant_channel

OUTPUT = "NDI Main Output"

# OBS colour settings are 0xAABBGGRR
GREEN = 0xFF00FF00
RED = 0xFF0000FF


def is_green(color):
    return dominant_channel(color) == "g"


def is_red(color):
    return dominant_channel(color) == "r"


def scene_of_colour(world, colour):
    scene = world.create_scene()
    world.add_color_source(scene, colour)
    return scene


def wait_for_received(receiver, predicate, timeout=15):
    receiver.wait_for_color(predicate, timeout)


def test_main_output_is_published_under_its_name(obs_world):
    with TestReceiver(obs_world.main_output_name) as rx:
        rx.connect(timeout=20)


def test_main_output_follows_the_program_scene(obs_world):
    green = scene_of_colour(obs_world, GREEN)
    red = scene_of_colour(obs_world, RED)
    with TestReceiver(obs_world.main_output_name) as rx:
        obs_world.ws.call("SetCurrentProgramScene", {"sceneName": green})
        rx.connect(timeout=20)
        wait_for_received(rx, is_green)

        obs_world.ws.call("SetCurrentProgramScene", {"sceneName": red})
        wait_for_received(rx, is_red)


def test_main_output_leaves_the_network_when_stopped_and_returns_when_started(obs_world, output_on):
    # Reachable before it is stopped, so that its going is a change
    with TestReceiver(obs_world.main_output_name) as rx:
        rx.connect(timeout=20)

    with SourceWatcher(obs_world.main_output_name) as watcher:
        obs_world.ws.call("StopOutput", {"outputName": OUTPUT})
        watcher.wait_gone(timeout=15)

    obs_world.ws.call("StartOutput", {"outputName": OUTPUT})
    with TestReceiver(obs_world.main_output_name) as rx:
        rx.connect(timeout=20)


def test_obs_exits_cleanly_with_the_output_active(obs_world):
    with TestReceiver(obs_world.main_output_name) as rx:
        rx.connect(timeout=20)

    obs_world.restart_obs()

    with TestReceiver(obs_world.main_output_name) as rx:
        rx.connect(timeout=30)
