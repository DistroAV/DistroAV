# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Grant Likely <grant.likely@secretlab.ca>
"""Covers the PTZ sender (ptz_helpers.py): that it says it can do PTZ, that a
receiver sees that, and that the commands a receiver sends are recorded as they
were sent. Needs no OBS."""

import time

import pytest

from ndi_helpers import unique_name
from ptz_helpers import PtzSender, find_ndi_library

pytestmark = pytest.mark.skipif(find_ndi_library() is None, reason="no NDI runtime library")


def wait_for(predicate, timeout=10):
    deadline = time.time() + timeout
    while time.time() < deadline:
        if predicate():
            return True
        time.sleep(0.1)
    return False


@pytest.fixture
def sender():
    s = PtzSender(unique_name("ptz-sender"))
    yield s
    s.stop()


def test_a_receiver_sees_ptz_support(sender):
    rx = sender.receiver()
    try:
        assert wait_for(rx.ptz_supported)
    finally:
        rx.close()


def test_commands_are_recorded_as_sent(sender):
    rx = sender.receiver()
    try:
        assert wait_for(rx.ptz_supported)
        rx.pan_tilt_speed(0.5, -0.25)
        rx.zoom(0.3)
        rx.store_preset(5)
        assert wait_for(lambda: len(sender.commands()) >= 3)
    finally:
        rx.close()
    assert sender.commands() == [
        {"kind": "pan_tilt_speed", "values": {"pan_speed": 0.5, "tilt_speed": -0.25}},
        {"kind": "zoom", "values": {"zoom": 0.3}},
        {"kind": "store_preset", "values": {"index": 5}},
    ]


def test_a_sender_without_ptz_is_not_supported():
    s = PtzSender(unique_name("ptz-sender-none"), ptz=False)
    rx = s.receiver()
    try:
        time.sleep(2)
        assert not rx.ptz_supported()
    finally:
        rx.close()
        s.stop()
