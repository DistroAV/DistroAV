# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Grant Likely <grant.likely@secretlab.ca>
"""Covers the dedicated NDI output filters: ndi_filter publishes the video of
the source it is on, on its own, apart from the program mix, and
ndi_audiofilter publishes its source's audio.
"""

from ndi_helpers import SourceWatcher, TestReceiver, TestSender, dominant_channel, unique_name

# OBS colour settings are 0xAABBGGRR
GREEN = 0xFF00FF00
RED = 0xFF0000FF

NAME_SETTING = "ndi_filter_ndiname"


def is_green(color):
    return dominant_channel(color) == "g"


def is_red(color):
    return dominant_channel(color) == "r"


def test_video_filter_publishes_its_source_alone(obs_world):
    name = unique_name("flt-video")
    scene = obs_world.create_scene()
    # The red source is over the green one, so the program mix is red
    green = obs_world.add_color_source(scene, GREEN)
    obs_world.add_color_source(scene, RED)
    obs_world.add_filter(green, "ndi_filter", **{NAME_SETTING: name})
    obs_world.ws.call("SetCurrentProgramScene", {"sceneName": scene})

    with TestReceiver(name) as filtered, TestReceiver(obs_world.main_output_name) as mix:
        filtered.connect(timeout=20)
        mix.connect(timeout=20)
        filtered.wait_for_color(is_green)
        mix.wait_for_color(is_red)


def test_audio_filter_publishes_its_source_audio(obs_world):
    name = unique_name("flt-audio")
    with TestSender(unique_name("flt-audio-in")) as tx:
        tx.send_tone()
        source = obs_world.add_ndi_source(obs_world.create_scene(), tx.ndi_name)
        obs_world.wait_for_audio(source)
        obs_world.add_filter(source, "ndi_audiofilter", **{NAME_SETTING: name})

        with TestReceiver(name) as rx:
            rx.connect(timeout=20, wait_for_video=False)
            assert rx.has_audio(timeout=10)


def test_removing_the_filter_stops_the_stream(obs_world):
    name = unique_name("flt-remove")
    scene = obs_world.create_scene()
    source = obs_world.add_color_source(scene, GREEN)
    flt = obs_world.add_filter(source, "ndi_filter", **{NAME_SETTING: name})
    obs_world.ws.call("SetCurrentProgramScene", {"sceneName": scene})

    with TestReceiver(name) as rx:
        rx.connect(timeout=20)

    with SourceWatcher(name) as watcher:
        obs_world.ws.call("RemoveSourceFilter", {"sourceName": source, "filterName": flt})
        watcher.wait_gone(timeout=15)
