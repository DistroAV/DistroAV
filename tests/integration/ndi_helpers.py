# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Grant Likely <grant.likely@secretlab.ca>
"""NDI senders and receivers for the tests to stand in for the network, as
obs-ptz's ptzsim stands in for a camera.

Built on cyndilib, which brings its own libndi: these run in the test
process, apart from the copy of the NDI runtime the plugin loads in OBS.
Every name carries a per-run prefix (unique_name), so that a test cannot
find a source that is on the LAN, or one from another run.
"""

import gc
import threading
import time
import uuid

import numpy as np
from cyndilib import AudioRecvFrame, AudioSendFrame, VideoFrameSync, VideoSendFrame
from cyndilib.finder import Finder
from cyndilib.receiver import ReceiveFrameType, Receiver
from cyndilib.sender import Sender
from cyndilib.wrapper.ndi_recv import RecvBandwidth, RecvColorFormat
from cyndilib.wrapper.ndi_structs import FourCC

RUN_ID = uuid.uuid4().hex[:6]

FRAME_RATE = 30
SAMPLE_RATE = 48000
CHANNELS = 2
SAMPLES_PER_FRAME = SAMPLE_RATE // FRAME_RATE

# Quietest signal that counts as audio
AUDIO_PEAK_FLOOR = 1e-3


def dominant_channel(color, floor=50, ratio=3):
    """'r', 'g' or 'b' if one channel of (r, g, b) is at least `floor` and
    `ratio` times each of the others, else None.

    For a picture OBS drew from one of its own colour sources, whose
    brightness is not the same on every platform: white comes out as 113 on
    OBS 32.2 on macOS, in OBS's own screenshot of the scene, before NDI is
    involved. Which colour it is, is what a test of the plugin can rely on.
    """
    for index, name in enumerate("rgb"):
        others = [c for i, c in enumerate(color) if i != index]
        if color[index] >= floor and all(color[index] >= ratio * max(c, 1) for c in others):
            return name
    return None


def unique_name(label: str) -> str:
    """An NDI name only this run uses"""
    return f"distroav-it-{RUN_ID}-{label}"


def _find_source(name: str, timeout: float):
    """The Source whose full name or stream name is `name`"""
    deadline = time.time() + timeout
    with Finder() as finder:
        while time.time() < deadline:
            finder.wait_for_sources(1.0)
            for source in finder:
                if source.name == name or source.stream_name == name:
                    # A Source belongs to its finder: keep what a receiver needs
                    return source.name
    raise TimeoutError(f"NDI source {name!r} not found within {timeout}s")


class SourceWatcher:
    """Watches one source on the network: opened once the source is there,
    so that it can say when it has gone.

    Open it before what is meant to make the source go. A finder made after
    the source has already gone cannot tell that from a source that was never
    there, and a stop or a removal is over before the next call can be made.
    """

    def __init__(self, name: str, timeout: float = 20):
        self._name = name
        self._timeout = timeout
        self._finder = None

    def _present(self) -> bool:
        return any(s.valid and self._name in (s.name, s.stream_name) for s in self._finder)

    def __enter__(self):
        self._finder = Finder()
        self._finder.open()
        deadline = time.time() + self._timeout
        while not self._present():
            if time.time() > deadline:
                self._finder.close()
                raise TimeoutError(f"NDI source {self._name!r} is not on the network after {self._timeout}s")
            self._finder.wait_for_sources(0.5)
        return self

    def __exit__(self, *exc):
        self._finder.close()

    def wait_gone(self, timeout: float = 10):
        deadline = time.time() + timeout
        while self._present():
            if time.time() > deadline:
                raise TimeoutError(f"NDI source {self._name!r} still on the network after {timeout}s")
            self._finder.wait_for_sources(0.5)


class TestSender:
    """Sends a solid colour at 30 fps, and a sine tone once asked to.

    Audio is not sent at all until send_tone(), so a receiver can tell a
    stream with audio from one without.
    """

    __test__ = False  # not a test class, whatever pytest thinks of its name

    def __init__(self, name: str, width: int = 320, height: int = 180):
        self._name = name
        self._width = width
        self._height = height
        self._color = (0, 0, 0)
        self._tone_hz = None
        self._phase = 0
        self._paused = False
        self._lock = threading.Lock()
        self._stop = threading.Event()
        self._thread = None
        self._frame = None
        self._build_sender()
        self._rebuild_frame()

    @property
    def ndi_name(self) -> str:
        """The name a receiver finds it by: `HOST (name)`"""
        source = self._sender.source
        return source.name if source is not None else _find_source(self._name, 10)

    def _build_sender(self):
        """cyndilib cannot alter a frame once it is on a sender, so a change of
        resolution is a new sender"""
        self._sender = Sender(self._name)
        self._video = VideoSendFrame()
        self._video.set_resolution(self._width, self._height)
        self._video.set_frame_rate(FRAME_RATE)
        self._video.set_fourcc(FourCC.BGRA)
        self._audio = AudioSendFrame()
        self._audio.sample_rate = SAMPLE_RATE
        self._audio.num_channels = CHANNELS
        self._audio.set_max_num_samples(SAMPLES_PER_FRAME)
        self._sender.set_video_frame(self._video)
        self._sender.set_audio_frame(self._audio)

    def _rebuild_frame(self):
        r, g, b = self._color
        frame = np.empty((self._height, self._width, 4), dtype=np.uint8)
        frame[:] = (b, g, r, 255)
        self._frame = frame.reshape(-1)

    def start(self):
        self._sender.open()
        self._thread = threading.Thread(target=self._pump, name=f"sender-{self._name}", daemon=True)
        self._thread.start()

    def stop(self):
        self._stop.set()
        if self._thread is not None:
            self._thread.join(5)
            self._thread = None
        self._sender.close()

    def __enter__(self):
        self.start()
        return self

    def __exit__(self, *exc):
        self.stop()

    def set_color(self, rgb):
        with self._lock:
            self._color = tuple(rgb)
            self._rebuild_frame()

    def set_resolution(self, width: int, height: int):
        """Changes what is sent from now on. The stream goes away and comes
        back under the same name, as a new sender, because the NDI frame
        cannot be altered while it is on the wire."""
        with self._lock:
            self._sender.close()
            # The old sender and its frames are gone before the new ones are
            # made under the same name: on Windows, freeing them after the new
            # sender exists crashes the process
            self._sender = self._video = self._audio = None
            gc.collect()
            self._width, self._height = width, height
            self._build_sender()
            self._rebuild_frame()
            self._sender.open()

    def send_tone(self, hz: float = 440.0):
        with self._lock:
            self._tone_hz = hz

    def pause(self):
        with self._lock:
            self._paused = True

    def resume(self):
        with self._lock:
            self._paused = False

    def _tone(self):
        n = np.arange(self._phase, self._phase + SAMPLES_PER_FRAME)
        self._phase += SAMPLES_PER_FRAME
        wave = (0.5 * np.sin(2 * np.pi * self._tone_hz * n / SAMPLE_RATE)).astype(np.float32)
        return np.tile(wave, (CHANNELS, 1))

    def _pump(self):
        next_time = time.monotonic()
        while not self._stop.is_set():
            with self._lock:
                if not self._paused:
                    if self._tone_hz is None:
                        self._sender.write_video(self._frame)
                    else:
                        self._sender.write_video_and_audio(self._frame, self._tone())
            next_time += 1.0 / FRAME_RATE
            delay = next_time - time.monotonic()
            if delay > 0:
                time.sleep(delay)
            else:
                next_time = time.monotonic()


class TestReceiver:
    """Connects to a source by its full name or its stream name, and reports
    what it last received"""

    __test__ = False

    def __init__(self, name: str):
        self._name = name
        self._finder = None
        self._receiver = None
        self._video = VideoFrameSync()
        self._audio_receiver = None
        self._audio_frame = None
        self._source = None

    def connect(self, timeout: float = 10, wait_for_video: bool = True):
        """Find the source and connect. With `wait_for_video` False it returns
        once connected, for a stream that has no picture (audio only)."""
        deadline = time.time() + timeout
        self._finder = Finder()
        self._finder.open()
        source = None
        while source is None and time.time() < deadline:
            self._finder.wait_for_sources(0.5)
            for candidate in self._finder:
                # One the finder is still learning about is not valid yet
                if candidate.valid and self._name in (candidate.name, candidate.stream_name):
                    source = candidate
                    break
        if source is None:
            raise TimeoutError(f"NDI source {self._name!r} not found within {timeout}s")
        self._receiver = Receiver(color_format=RecvColorFormat.BGRX_BGRA, bandwidth=RecvBandwidth.highest)
        sync = self._receiver.frame_sync
        sync.set_video_frame(self._video)
        self._source = source
        self._receiver.set_source(source)
        while time.time() < deadline:
            if self._receiver.is_connected():
                if not wait_for_video:
                    return
                sync.capture_video()
                if min(self._video.get_resolution()) > 0 and self._video.get_data_size() > 0:
                    return
            time.sleep(0.1)
        raise TimeoutError(f"no frame from {self._name!r} within {timeout}s")

    def close(self):
        if self._audio_receiver is not None:
            self._audio_receiver.disconnect()
            self._audio_receiver = None
        if self._receiver is not None:
            self._receiver.disconnect()
            self._receiver = None
        if self._finder is not None:
            self._finder.close()
            self._finder = None

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()

    def _capture(self):
        self._receiver.frame_sync.capture_video()

    def dimensions(self):
        self._capture()
        return tuple(self._video.get_resolution())

    def wait_for_dimensions(self, expected, timeout: float = 10):
        deadline = time.time() + timeout
        while time.time() < deadline:
            if self.dimensions() == tuple(expected):
                return
            time.sleep(0.1)
        raise TimeoutError(f"still {self.dimensions()}, not {tuple(expected)}, after {timeout}s")

    def wait_for_color(self, predicate, timeout: float = 15):
        """Poll the picture until predicate((r, g, b)) holds"""
        deadline = time.time() + timeout
        color = None
        while time.time() < deadline:
            color = self.mean_color()
            if predicate(color):
                return color
            time.sleep(0.1)
        raise TimeoutError(f"picture was {color} after {timeout}s")

    def mean_color(self):
        """(r, g, b) averaged over the last frame"""
        self._capture()
        width, height = self._video.get_resolution()
        stride = self._video.get_line_stride()
        data = np.frombuffer(self._video, dtype=np.uint8)[: stride * height].reshape(height, stride)
        pixels = data[:, : width * 4].reshape(height, width, 4)
        b, g, r = (float(pixels[:, :, i].mean()) for i in (0, 1, 2))
        return (round(r), round(g), round(b))

    def has_audio(self, timeout: float = 3) -> bool:
        """Whether sound arrived within `timeout`. Taken on a receiver of its
        own, that takes audio only: cyndilib's frame sync hands back silence
        where the plain receiver hands back what was sent."""
        if self._audio_receiver is None:
            self._audio_frame = AudioRecvFrame()
            self._audio_receiver = Receiver(bandwidth=RecvBandwidth.audio_only)
            self._audio_receiver.set_audio_frame(self._audio_frame)
            self._audio_receiver.set_source(self._source)
        deadline = time.time() + timeout
        while time.time() < deadline:
            if self._audio_receiver.receive(ReceiveFrameType.recv_audio, 200) & ReceiveFrameType.recv_audio:
                samples = self._audio_frame.get_all_read_data()[0]
                if samples.size and float(np.abs(samples).max()) > AUDIO_PEAK_FLOOR:
                    return True
        return False
