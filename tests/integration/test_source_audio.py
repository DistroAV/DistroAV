# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Grant Likely <grant.likely@secretlab.ca>
"""Covers an NDI source in OBS carrying the sender's audio, seen as the
level OBS meters on the input (obs-websocket's InputVolumeMeters).
"""

from ndi_helpers import TestSender, unique_name


def test_source_carries_the_sent_tone(obs_world):
    with TestSender(unique_name("rx-tone")) as tx:
        tx.send_tone()
        source = obs_world.add_ndi_source(obs_world.create_scene(), tx.ndi_name)
        peak = obs_world.wait_for_audio(source)
        assert peak > 0.01


def test_source_is_silent_when_the_sender_sends_no_audio(obs_world):
    with TestSender(unique_name("rx-quiet")) as tx:
        tx.set_color((0, 255, 0))
        source = obs_world.add_ndi_source(obs_world.create_scene(), tx.ndi_name)
        obs_world.wait_for_color(source, lambda c: c[1] > 200)
        assert obs_world.audio_peak(source, seconds=2) < 0.001
