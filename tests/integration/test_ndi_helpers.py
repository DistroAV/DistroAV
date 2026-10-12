# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Grant Likely <grant.likely@secretlab.ca>
"""Covers the NDI test sender and receiver the other modules are built on, on
their own, with no OBS: if these are wrong, every other test is.
"""

import pytest
from ndi_helpers import TestReceiver, TestSender, unique_name


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
