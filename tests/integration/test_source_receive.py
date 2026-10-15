# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Grant Likely <grant.likely@secretlab.ca>
"""Covers an NDI source in OBS receiving what a sender sends: that it
connects, and that what it shows is the sender's picture, at the sender's
size, following the sender when that changes.
"""

from ndi_helpers import TestSender, unique_name


def is_green(color):
    r, g, b = color
    return g > 200 and r < 50 and b < 50


def is_blue(color):
    r, g, b = color
    return b > 200 and r < 50 and g < 50


def test_source_shows_the_sent_colour(obs_world):
    with TestSender(unique_name("rx-color")) as tx:
        tx.set_color((0, 255, 0))
        source = obs_world.add_ndi_source(obs_world.create_scene(), tx.ndi_name)
        obs_world.wait_for_color(source, is_green)


def test_source_has_the_sent_size(obs_world):
    with TestSender(unique_name("rx-size"), width=320, height=180) as tx:
        tx.set_color((0, 255, 0))
        source = obs_world.add_ndi_source(obs_world.create_scene(), tx.ndi_name)
        obs_world.wait_for_color(source, is_green)
        assert obs_world.source_screenshot(source).size == (320, 180)


def test_source_follows_the_sender_to_a_new_size_and_colour(obs_world):
    with TestSender(unique_name("rx-change"), width=320, height=180) as tx:
        tx.set_color((0, 255, 0))
        source = obs_world.add_ndi_source(obs_world.create_scene(), tx.ndi_name)
        obs_world.wait_for_color(source, is_green)

        tx.set_color((0, 0, 255))
        tx.set_resolution(640, 360)
        obs_world.wait_for(lambda: obs_world.source_screenshot(source).size == (640, 360), timeout=15)
        obs_world.wait_for_color(source, is_blue)
