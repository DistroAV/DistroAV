# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Grant Likely <grant.likely@secretlab.ca>
"""Minimal obs-websocket v5 client.

Just enough of the protocol (the Hello/Identify handshake with its SHA256
challenge, and Request/RequestResponse) to drive OBS from a test: create
scenes and inputs, read settings, and start or stop outputs. Not a
general-purpose client; see
https://github.com/obsproject/obs-websocket/blob/master/docs/generated/protocol.md
for the protocol it implements a slice of.

Derived from obs-ptz's tests/obs-integration/obsws.py (GPL-2.0-or-later).
"""

import base64
import hashlib
import itertools
import json
import time

from websockets.sync.client import connect

OP_HELLO = 0
OP_IDENTIFY = 1
OP_IDENTIFIED = 2
OP_REQUEST = 6
OP_REQUEST_RESPONSE = 7


class ObsWebSocketError(RuntimeError):
    def __init__(self, message, status=None):
        super().__init__(message)
        self.status = status or {}


class Client:
    def __init__(self, url, password=None, timeout=10):
        self._ws = connect(url, open_timeout=timeout, legacy=True)
        self._ids = itertools.count(1)

        hello = json.loads(self._ws.recv(timeout=timeout))
        if hello.get("op") != OP_HELLO:
            raise ObsWebSocketError(f"expected Hello, got {hello}")

        identify = {"rpcVersion": hello["d"]["rpcVersion"], "eventSubscriptions": 0}
        auth = hello["d"].get("authentication")
        if auth and password is not None:
            secret = base64.b64encode(hashlib.sha256((password + auth["salt"]).encode()).digest())
            identify["authentication"] = base64.b64encode(
                hashlib.sha256(secret + auth["challenge"].encode()).digest()).decode()
        self._ws.send(json.dumps({"op": OP_IDENTIFY, "d": identify}))

        ack = json.loads(self._ws.recv(timeout=timeout))
        if ack.get("op") != OP_IDENTIFIED:
            raise ObsWebSocketError(f"Identify failed: {ack}")

    def call(self, request_type, request_data=None, timeout=10):
        request_id = str(next(self._ids))
        payload = {"op": OP_REQUEST, "d": {"requestType": request_type, "requestId": request_id}}
        if request_data is not None:
            payload["d"]["requestData"] = request_data
        self._ws.send(json.dumps(payload))

        while True:
            msg = json.loads(self._ws.recv(timeout=timeout))
            if msg.get("op") != OP_REQUEST_RESPONSE:
                continue  # ignore Event frames etc. while waiting for our response
            d = msg["d"]
            if d.get("requestId") != request_id:
                continue
            status = d["requestStatus"]
            if not status["result"]:
                raise ObsWebSocketError(f"{request_type} failed: {status}", status=status)
            return d.get("responseData") or {}

    def close(self):
        self._ws.close()


def connect_when_ready(url, password=None, timeout=30, alive=lambda: True):
    """A Client, once the server will complete the handshake. OBS accepts the
    connection while it is still starting, and may close it before saying
    Hello, so any failure to connect is tried again until the deadline, or
    until `alive()` says the process behind the server is gone."""
    deadline = time.time() + timeout
    last_error = None
    while time.time() < deadline:
        if not alive():
            raise ObsWebSocketError(f"the server exited while it was being waited for: {last_error}")
        try:
            return Client(url, password=password)
        except Exception as e:  # websockets raises its own, OSError, EOFError...
            last_error = e
            time.sleep(0.2)
    raise TimeoutError(f"no obs-websocket handshake within {timeout}s: {last_error}")
