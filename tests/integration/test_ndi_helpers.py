# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Grant Likely <grant.likely@secretlab.ca>
"""Covers the NDI test sender and receiver the other modules are built on, on
their own, with no OBS: if these are wrong, every other test is.
"""

import pytest
from ndi_helpers import TestReceiver, SourceWatcher, TestSender, dominant_channel, unique_name


def test_receiver_sees_what_sender_sends():
    with TestSender(unique_name("loop")) as tx, TestReceiver(tx.ndi_name) as rx:
        tx.set_color((255, 0, 0))
        rx.connect()
        assert rx.dimensions() == (320, 180)
        r, g, b = rx.mean_color()
        assert r > 200 and g < 50 and b < 50


def test_receiver_hears_tone():
    with TestSender(unique_name("tone")) as tx, TestReceiver(tx.ndi_name) as rx:
        tx.send_tone()
        rx.connect()
        assert rx.has_audio()


def test_receiver_hears_no_audio_when_none_is_sent():
    with TestSender(unique_name("silent")) as tx, TestReceiver(tx.ndi_name) as rx:
        rx.connect()
        assert not rx.has_audio()


def test_receiver_follows_a_resolution_change():
    with TestSender(unique_name("resize")) as tx, TestReceiver(tx.ndi_name) as rx:
        rx.connect()
        tx.set_resolution(640, 360)
        rx.wait_for_dimensions((640, 360))


def test_unique_names_carry_the_run_prefix_and_label():
    name = unique_name("x")
    assert name.startswith("distroav-it-") and name.endswith("-x")


def test_a_receiver_does_not_find_a_sender_of_another_name():
    with TestSender(unique_name("a")), TestReceiver(unique_name("b")) as rx:
        with pytest.raises(TimeoutError):
            rx.connect(timeout=3)


def test_dominant_channel_is_the_colour_whatever_the_brightness():
    assert dominant_channel((255, 0, 0)) == "r"
    assert dominant_channel((0, 113, 3)) == "g"
    assert dominant_channel((10, 20, 200)) == "b"
    assert dominant_channel((113, 113, 113)) is None
    assert dominant_channel((20, 5, 5)) is None  # too dark to say


def test_a_watcher_sees_a_source_go_even_when_it_goes_at_once():
    name = unique_name("gone")
    tx = TestSender(name)
    tx.start()
    with SourceWatcher(name) as watcher:
        tx.stop()  # gone before anything is waited for
        watcher.wait_gone(timeout=20)


def test_a_watcher_cannot_be_opened_on_a_source_that_is_not_there():
    with pytest.raises(TimeoutError):
        with SourceWatcher(unique_name("never"), timeout=3):
            pass
