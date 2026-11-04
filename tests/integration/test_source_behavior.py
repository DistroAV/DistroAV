# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Grant Likely <grant.likely@secretlab.ca>
"""Covers what an NDI source does when its sender goes away and comes back:
`ndi_behavior_timeout` (src/ndi-source.cpp) says whether the last frame is
kept or cleared after three seconds without one.

(`ndi_behavior`, the other setting with "behavior" in its name, is about the
source being visible, not about the sender, and is not covered here.)
"""

import time

from obsws import ObsWebSocketError
from ndi_helpers import TestSender, unique_name

TIMEOUT_CLEAR_CONTENT = 0
TIMEOUT_KEEP_CONTENT = 1

# The source clears after 3 s without a frame; allow for the poll as well
CLEAR_AFTER = 3
HOLD_FOR = CLEAR_AFTER + 3


def is_green(color):
    r, g, b = color
    return g > 200 and r < 50 and b < 50


def has_picture(world, source):
    """Whether the source shows its green picture. One OBS has no frame of at
    all to take a screenshot of counts as not showing it."""
    try:
        return is_green(world.source_color(source))
    except ObsWebSocketError:
        return False


def test_last_frame_is_kept_when_the_sender_stops(obs_world):
    tx = TestSender(unique_name("beh-keep"))
    tx.set_color((0, 255, 0))
    tx.start()
    try:
        source = obs_world.add_ndi_source(obs_world.create_scene(), tx.ndi_name,
                                          ndi_behavior_timeout=TIMEOUT_KEEP_CONTENT)
        obs_world.wait_for_color(source, is_green)
    finally:
        tx.stop()

    time.sleep(HOLD_FOR)
    assert has_picture(obs_world, source)


def test_last_frame_is_cleared_when_the_sender_stops(obs_world):
    tx = TestSender(unique_name("beh-clear"))
    tx.set_color((0, 255, 0))
    tx.start()
    try:
        source = obs_world.add_ndi_source(obs_world.create_scene(), tx.ndi_name,
                                          ndi_behavior_timeout=TIMEOUT_CLEAR_CONTENT)
        obs_world.wait_for_color(source, is_green)
    finally:
        tx.stop()

    obs_world.wait_for(lambda: not has_picture(obs_world, source), timeout=HOLD_FOR + 5, interval=0.5)


def test_source_recovers_when_the_sender_returns(obs_world):
    name = unique_name("beh-recover")
    first = TestSender(name)
    first.set_color((0, 255, 0))
    first.start()
    try:
        full_name = first.ndi_name
        source = obs_world.add_ndi_source(obs_world.create_scene(), full_name,
                                          ndi_behavior_timeout=TIMEOUT_CLEAR_CONTENT)
        obs_world.wait_for_color(source, is_green)
    finally:
        first.stop()
    obs_world.wait_for(lambda: not has_picture(obs_world, source), timeout=HOLD_FOR + 5, interval=0.5)

    with TestSender(name) as second:
        second.set_color((0, 255, 0))
        obs_world.wait_for(lambda: has_picture(obs_world, source), timeout=20, interval=0.5)
