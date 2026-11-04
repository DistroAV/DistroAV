# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Grant Likely <grant.likely@secretlab.ca>
"""What the PTZ tests need that cyndilib cannot do, and a controller.

PtzSender is an NDI sender that says it can do PTZ and records the PTZ
commands its receivers send it: cyndilib can neither advertise PTZ nor read
the metadata a receiver sends a sender, so this goes to the NDI runtime
itself, through ctypes, and prefers the copy cyndilib ships.

PtzController drives the plugin's PTZ API from outside OBS, the way another
plugin would, through the obs-websocket binding that obs-ptz (loaded in the
test OBS: see DISTROAV_PTZ_CONTROLLER in the README) registers as the vendor
"obs-ptz": a request for each proc of the API, and an event for each signal.
obs-websocket cannot call a source's proc_handler itself, so a controller in
OBS is what makes the call.
"""

import ctypes as C
import json
import platform
import threading
import time
import xml.etree.ElementTree as ET
from pathlib import Path

FOURCC_BGRA = int.from_bytes(b"BGRA", "little")
FRAME_METADATA = 3
RECV_COLOR_FASTEST = 100
RECV_BANDWIDTH_HIGHEST = 100

_lib = None
_lib_lock = threading.Lock()


class _SendCreate(C.Structure):
    _fields_ = [("name", C.c_char_p), ("groups", C.c_char_p), ("clock_video", C.c_bool), ("clock_audio", C.c_bool)]


class _Video(C.Structure):
    _fields_ = [("xres", C.c_int), ("yres", C.c_int), ("fourcc", C.c_int), ("rate_n", C.c_int), ("rate_d", C.c_int),
                ("aspect", C.c_float), ("format", C.c_int), ("timecode", C.c_int64), ("data", C.c_void_p),
                ("stride", C.c_int), ("metadata", C.c_char_p), ("timestamp", C.c_int64)]


class _Meta(C.Structure):
    _fields_ = [("length", C.c_int), ("timecode", C.c_int64), ("data", C.c_void_p)]


class _Source(C.Structure):
    _fields_ = [("name", C.c_char_p), ("url", C.c_char_p)]


class _RecvCreate(C.Structure):
    _fields_ = [("source", _Source), ("color", C.c_int), ("bandwidth", C.c_int), ("fields", C.c_bool),
                ("name", C.c_char_p)]


def find_ndi_library():
    """The NDI runtime library to use: cyndilib's own, else an installed one; or None"""
    names = {"Darwin": ["libndi.dylib"], "Linux": ["libndi.so.6", "libndi.so"],
             "Windows": ["Processing.NDI.Lib.x64.dll"]}[platform.system()]
    candidates = []
    try:
        import cyndilib
        root = Path(cyndilib.__file__).parent
        for sub in ("wrapper/bin", ".dylibs", "wrapper"):
            candidates += [root / sub / n for n in names]
    except ImportError:
        pass
    candidates += [Path("/Library/NDI SDK for Apple/lib/macOS/libndi.dylib"), Path("/usr/local/lib/libndi.dylib"),
                   Path("/usr/lib/libndi.so.6"), Path("/usr/local/lib/libndi.so.6")]
    for candidate in candidates:
        if candidate.exists():
            return str(candidate)
    return None


def _ndi():
    global _lib
    with _lib_lock:
        if _lib is not None:
            return _lib
        path = find_ndi_library()
        if path is None:
            raise RuntimeError("no NDI runtime library")
        L = C.CDLL(path)
        L.NDIlib_initialize.restype = C.c_bool
        L.NDIlib_send_create.restype = C.c_void_p
        L.NDIlib_send_create.argtypes = [C.POINTER(_SendCreate)]
        L.NDIlib_send_destroy.argtypes = [C.c_void_p]
        L.NDIlib_send_capture.restype = C.c_int
        L.NDIlib_send_capture.argtypes = [C.c_void_p, C.POINTER(_Meta), C.c_uint32]
        L.NDIlib_send_free_metadata.argtypes = [C.c_void_p, C.POINTER(_Meta)]
        L.NDIlib_send_add_connection_metadata.argtypes = [C.c_void_p, C.POINTER(_Meta)]
        L.NDIlib_send_send_video_v2.argtypes = [C.c_void_p, C.POINTER(_Video)]
        L.NDIlib_find_create_v2.restype = C.c_void_p
        L.NDIlib_find_destroy.argtypes = [C.c_void_p]
        L.NDIlib_find_wait_for_sources.argtypes = [C.c_void_p, C.c_uint32]
        L.NDIlib_find_wait_for_sources.restype = C.c_bool
        L.NDIlib_find_get_current_sources.restype = C.POINTER(_Source)
        L.NDIlib_find_get_current_sources.argtypes = [C.c_void_p, C.POINTER(C.c_uint32)]
        L.NDIlib_recv_create_v3.restype = C.c_void_p
        L.NDIlib_recv_create_v3.argtypes = [C.POINTER(_RecvCreate)]
        L.NDIlib_recv_destroy.argtypes = [C.c_void_p]
        L.NDIlib_recv_ptz_is_supported.restype = C.c_bool
        L.NDIlib_recv_ptz_is_supported.argtypes = [C.c_void_p]
        for fn, n in (("pan_tilt_speed", 2), ("pan_tilt", 2), ("zoom", 1)):
            getattr(L, "NDIlib_recv_ptz_" + fn).argtypes = [C.c_void_p] + [C.c_float] * n
        L.NDIlib_recv_ptz_store_preset.argtypes = [C.c_void_p, C.c_int]
        if not L.NDIlib_initialize():
            raise RuntimeError("NDIlib_initialize failed")
        _lib = L
        return L


def _number(text):
    f = float(text)
    return int(f) if f.is_integer() and "." not in text else f


class PtzReceiver:
    """A bare NDI receiver of one sender, to send PTZ commands from"""

    def __init__(self, lib, name):
        self._lib = lib
        self._rx = None
        find = lib.NDIlib_find_create_v2(None)
        try:
            deadline = time.time() + 15
            while self._rx is None and time.time() < deadline:
                lib.NDIlib_find_wait_for_sources(find, 500)
                n = C.c_uint32(0)
                sources = lib.NDIlib_find_get_current_sources(find, C.byref(n))
                for i in range(n.value):
                    if name.encode() in sources[i].name:
                        create = _RecvCreate(sources[i], RECV_COLOR_FASTEST, RECV_BANDWIDTH_HIGHEST, False,
                                             b"ptz-test-receiver")
                        self._rx = lib.NDIlib_recv_create_v3(C.byref(create))
                        break
        finally:
            lib.NDIlib_find_destroy(find)
        if self._rx is None:
            raise RuntimeError(f"NDI source {name!r} was not found")

    def ptz_supported(self):
        return bool(self._lib.NDIlib_recv_ptz_is_supported(self._rx))

    def pan_tilt_speed(self, pan, tilt):
        self._lib.NDIlib_recv_ptz_pan_tilt_speed(self._rx, pan, tilt)

    def zoom(self, value):
        self._lib.NDIlib_recv_ptz_zoom(self._rx, value)

    def store_preset(self, index):
        self._lib.NDIlib_recv_ptz_store_preset(self._rx, index)

    def close(self):
        if self._rx is not None:
            self._lib.NDIlib_recv_destroy(self._rx)
            self._rx = None


class PtzSender:
    """Sends video as `name`, with PTZ if ptz, and records the PTZ commands it
    is sent: commands() is [{"kind", "values"}...] in arrival order, with the
    kind and attributes of NDI's own metadata element, less its "ntk_ptz_"."""

    def __init__(self, name, ptz=True):
        self.name = name
        self._lib = _ndi()
        create = _SendCreate(name.encode(), None, True, False)
        self._send = self._lib.NDIlib_send_create(C.byref(create))
        if ptz:
            caps = b'<ndi_capabilities ntk_ptz="true"/>'
            meta = _Meta(len(caps) + 1, 0, C.cast(C.c_char_p(caps), C.c_void_p))
            self._lib.NDIlib_send_add_connection_metadata(self._send, C.byref(meta))
        self._lock = threading.Lock()
        self._commands = []
        self._frames = 0
        self._running = True
        self._threads = [threading.Thread(target=self._video, daemon=True),
                         threading.Thread(target=self._capture, daemon=True)]
        for t in self._threads:
            t.start()

    def _video(self):
        buf = C.create_string_buffer(64 * 64 * 4)
        frame = _Video(64, 64, FOURCC_BGRA, 30, 1, 1.0, 1, 0, C.cast(buf, C.c_void_p), 64 * 4, None, 0)
        while self._running:
            # clocked, so this paces itself at the frame rate
            self._lib.NDIlib_send_send_video_v2(self._send, C.byref(frame))
            with self._lock:
                self._frames += 1

    def _capture(self):
        while self._running:
            meta = _Meta()
            if self._lib.NDIlib_send_capture(self._send, C.byref(meta), 100) == FRAME_METADATA:
                text = C.string_at(meta.data).decode()
                self._lib.NDIlib_send_free_metadata(self._send, C.byref(meta))
                try:
                    element = ET.fromstring(text)
                except ET.ParseError:
                    continue
                if not element.tag.startswith("ntk_ptz_"):
                    continue
                values = {k: v if k == "mode" else _number(v) for k, v in element.attrib.items()}
                with self._lock:
                    self._commands.append({"kind": element.tag[len("ntk_ptz_"):], "values": values})

    def receiver(self):
        return PtzReceiver(self._lib, self.name)

    def source_name(self, timeout=15):
        """The name NDI gives the source, "HOST (name)", for a source to be told"""
        find = self._lib.NDIlib_find_create_v2(None)
        try:
            deadline = time.time() + timeout
            while time.time() < deadline:
                self._lib.NDIlib_find_wait_for_sources(find, 500)
                n = C.c_uint32(0)
                sources = self._lib.NDIlib_find_get_current_sources(find, C.byref(n))
                for i in range(n.value):
                    if self.name.encode() in sources[i].name:
                        return sources[i].name.decode()
        finally:
            self._lib.NDIlib_find_destroy(find)
        raise RuntimeError(f"NDI source {self.name!r} was not found")

    def commands(self):
        with self._lock:
            return list(self._commands)

    def clear(self):
        with self._lock:
            self._commands.clear()

    def frames_sent(self):
        with self._lock:
            return self._frames

    def stop(self):
        if not self._running:
            return
        self._running = False
        for t in self._threads:
            t.join(timeout=5)
        self._lib.NDIlib_send_destroy(self._send)


VENDOR = "obs-ptz"
VENDOR_EVENTS = 1 << 9


class PtzController:
    """The PTZ API as a controller plugin has it, over obs-ptz's websocket binding.
    A device is named by its UUID: ptz_list_devices gives them, and for the NDI
    source it is the source's own, which device_uuid() finds from its name.

    It listens for the binding's events from the moment it is made, and keeps
    them: events() looks over what has come since mark()."""

    def __init__(self, world):
        self.world = world
        self._listener = world.new_client(VENDOR_EVENTS)
        self._seen = []
        self._from = 0

    def close(self):
        self._listener.close()

    def call(self, proc, device=None, **fields):
        """Calls a proc of the API, on the device if one is given, and returns its
        answer: what it gave back as `return`, each `out` field, and each ptr
        passed in as the proc left it. An error the binding answers with is raised."""
        data = dict(fields)
        if device is not None:
            data["device"] = device
        return self.world.ws.vendor(VENDOR, proc, data, timeout=20)

    def device_uuid(self, source_name):
        """The UUID of the source of that name, which is its device's"""
        for item in self.world.ws.call("GetInputList")["inputs"]:
            if item["inputName"] == source_name:
                return item["inputUuid"]
        raise AssertionError(f"no input called {source_name!r}")

    def devices(self):
        return self.call("ptz_list_devices")["devices"]

    def listed(self, device):
        return next((d for d in self.devices() if d["device"] == device), None)

    def wait_for_device(self, device, present=True, timeout=10):
        """Until the controller lists the device (or no longer does); the entry"""
        self.world.wait_for(lambda: (self.listed(device) is not None) == present, timeout=timeout, interval=0.2)
        return self.listed(device)

    def state(self, device):
        return self.call("ptz_get_state", device)["state"]

    def wait_for_state(self, device, predicate, timeout=5):
        last = []

        def matched():
            last[:] = [self.state(device)]
            return predicate(last[0])

        try:
            self.world.wait_for(matched, timeout=timeout, interval=0.2)
        except AssertionError:
            raise AssertionError(f"device {device!r} state never matched; last seen: {last}") from None
        return last[0]

    def preset_list(self, device):
        return self.call("ptz_preset_get_list", device)["return"]

    def create_preset(self, device, name="", store="camera", timeout=10):
        """ptz_preset_create, then waits for ptz_preset_create_done: the id of the preset
        made, or "" if the device took no request or made none"""
        request = self.call("ptz_preset_create", device, name=name, store=store)["return"]
        if not request:
            return ""
        found = []

        def done():
            found[:] = [e for e in self.events("ptz_preset_create_done", device, since_mark=False)
                        if e.get("request") == request]
            return bool(found)

        self.world.wait_for(done, timeout=timeout)
        return found[0]["id"]

    def _collect(self):
        self._seen += [e["eventData"] for e in self._listener.take("VendorEvent")
                       if (e["eventData"] or {}).get("vendorName") == VENDOR]

    def mark(self):
        """Events are looked over from here on"""
        self._collect()
        self._from = len(self._seen)

    def events(self, name, device=None, since_mark=True):
        """The data of the `name` events that have come, of the device if one is given"""
        self._collect()
        found = self._seen[self._from if since_mark else 0:]
        return [e["eventData"] for e in found
                if e["eventType"] == name and (device is None or e["eventData"].get("device") == device)]
