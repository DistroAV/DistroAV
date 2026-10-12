"""Minimal obs-websocket v5 client.

Just enough of the protocol (the Hello/Identify handshake with its SHA256
challenge, Request/RequestResponse, and Events) to drive OBS from a test:
create scenes and sources, call a request, and wait for an event. Not a
general-purpose client -- see
https://github.com/obsproject/obs-websocket/blob/master/docs/generated/protocol.md
for the protocol it implements a slice of.

Copyright 2026 Grant Likely <grant.likely@secretlab.ca>
SPDX-License-Identifier: GPL-2.0-or-later
"""

import base64
import collections
import hashlib
import itertools
import json
import threading
import time

from websockets.exceptions import ConnectionClosed
from websockets.sync.client import connect

OP_HELLO = 0
OP_IDENTIFY = 1
OP_IDENTIFIED = 2
OP_EVENT = 5
OP_REQUEST = 6
OP_REQUEST_RESPONSE = 7

# obs-websocket's EventSubscription bits. A vendor's events have their own,
# which a client that asks for no more than General is not sent.
EVENT_GENERAL = 1 << 0
EVENT_VENDORS = 1 << 9
# The audio levels of the inputs, which are sent all the time once asked for
EVENT_INPUT_VOLUME_METERS = 1 << 16


class ObsWebSocketError(RuntimeError):
    def __init__(self, message, status=None):
        super().__init__(message)
        self.status = status or {}


# How many events are kept for wait_event(): the oldest go first
EVENTS_KEPT = 20000


class Client:
    def __init__(self, url, password=None, timeout=10, event_subscriptions=0, keepalive=20):
        # keepalive: how often the connection is pinged, and how long an answer is waited for
        options = dict(open_timeout=timeout, ping_interval=keepalive, ping_timeout=keepalive)
        try:
            self._ws = connect(url, legacy=True, **options)
        except TypeError:
            # websockets before 17 has no `legacy`: its connect() is the
            # client already, and passes the argument on to the socket
            self._ws = connect(url, **options)
        self._ids = itertools.count(1)

        hello = json.loads(self._ws.recv(timeout=timeout))
        if hello.get("op") != OP_HELLO:
            raise ObsWebSocketError(f"expected Hello, got {hello}")

        identify = {"rpcVersion": hello["d"]["rpcVersion"], "eventSubscriptions": event_subscriptions}
        auth = hello["d"].get("authentication")
        if auth and password is not None:
            secret = base64.b64encode(hashlib.sha256((password + auth["salt"]).encode()).digest())
            identify["authentication"] = base64.b64encode(
                hashlib.sha256(secret + auth["challenge"].encode()).digest()).decode()
        self._ws.send(json.dumps({"op": OP_IDENTIFY, "d": identify}))

        ack = json.loads(self._ws.recv(timeout=timeout))
        if ack.get("op") != OP_IDENTIFIED:
            raise ObsWebSocketError(f"Identify failed: {ack}")

        # Everything that comes from here on is read by a thread of its own, as
        # it comes, whether or not anything is waiting for it. A connection that
        # is not read stops answering its pings once what is unread fills what
        # holds it, and is closed for that: a test that never looks at its events
        # would lose the connection to them.
        self._changed = threading.Condition()
        self._responses = {}
        self._events = collections.deque(maxlen=EVENTS_KEPT)
        self._closed = None
        self._reader = threading.Thread(target=self._read, daemon=True)
        self._reader.start()

    def _read(self):
        while True:
            try:
                msg = json.loads(self._ws.recv())
            except Exception as e:  # closed, by either end
                with self._changed:
                    self._closed = e
                    self._changed.notify_all()
                return
            with self._changed:
                if msg.get("op") == OP_EVENT:
                    # when it came, for a test of how long something took
                    msg["d"]["at"] = time.monotonic()
                    self._events.append(msg["d"])
                elif msg.get("op") == OP_REQUEST_RESPONSE:
                    self._responses[msg["d"].get("requestId")] = msg["d"]
                self._changed.notify_all()

    def call(self, request_type, request_data=None, timeout=10):
        request_id = str(next(self._ids))
        payload = {"op": OP_REQUEST, "d": {"requestType": request_type, "requestId": request_id}}
        if request_data is not None:
            payload["d"]["requestData"] = request_data
        try:
            self._ws.send(json.dumps(payload))
        except ConnectionClosed as e:
            raise ConnectionError(f"{request_type}: the connection closed: {e!r}") from e

        with self._changed:
            answered = self._changed.wait_for(lambda: request_id in self._responses or self._closed, timeout=timeout)
            if request_id not in self._responses:
                if self._closed:
                    raise ConnectionError(f"{request_type}: the connection closed: {self._closed!r}")
                assert not answered
                raise TimeoutError(f"{request_type}: no answer within {timeout}s")
            d = self._responses.pop(request_id)
        status = d["requestStatus"]
        if not status["result"]:
            raise ObsWebSocketError(f"{request_type} failed: {status}", status=status)
        return d.get("responseData") or {}

    def vendor(self, vendor, request, data=None, timeout=10):
        """Calls a vendor's request, and returns what the vendor answered. An
        "error" in the answer, which is how a vendor says it could not, is raised,
        with the answer as the error's status."""
        response = self.call("CallVendorRequest",
                             {"vendorName": vendor, "requestType": request, "requestData": data or {}},
                             timeout=timeout)
        answer = response.get("responseData") or {}
        if "error" in answer:
            raise ObsWebSocketError(f"{vendor} {request} failed: {answer['error']}: {answer.get('message', '')}",
                                    status=answer)
        return answer

    def _next_event(self, timeout):
        """The oldest event that has not been returned; None if none comes in time"""
        with self._changed:
            self._changed.wait_for(lambda: self._events or self._closed, timeout=max(timeout, 0))
            return self._events.popleft() if self._events else None

    def wait_event(self, event_type, predicate=None, timeout=10):
        """The data of the next `event_type` event for which predicate(data) is
        true, of those that have come since the client connected and have not
        been returned. An event that does not match is dropped."""
        deadline = time.time() + timeout
        while True:
            event = self._next_event(deadline - time.time())
            if event is None:
                raise TimeoutError(f"no {event_type} event within {timeout}s")
            if event.get("eventType") != event_type:
                continue
            data = event.get("eventData") or {}
            if predicate is None or predicate(data):
                return data

    def events(self, event_type, settle=0.2):
        """The data of every `event_type` event that has come and not been
        returned, with those that come in the next `settle` seconds. Takes them all."""
        found = []
        deadline = time.time() + settle
        while True:
            event = self._next_event(deadline - time.time())
            if event is None and (time.time() >= deadline or self._closed):
                return found
            if event is not None and event.get("eventType") == event_type:
                found.append(event.get("eventData") or {})

    def take(self, event_type):
        """Every `event_type` event that has come and not been returned, without
        waiting for more: each as {"eventData", "at"}, "at" being when it came, on
        time.monotonic()'s clock. Takes them all."""
        with self._changed:
            taken = list(self._events)
            self._events.clear()
        return [{"eventData": e.get("eventData") or {}, "at": e["at"]} for e in taken
                if e.get("eventType") == event_type]

    def close(self):
        self._ws.close()


def connect_when_ready(url, password=None, timeout=30, event_subscriptions=0, alive=lambda: True):
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
            return Client(url, password=password, event_subscriptions=event_subscriptions)
        except Exception as e:  # websockets raises its own, OSError, EOFError...
            last_error = e
            time.sleep(0.2)
    raise TimeoutError(f"no obs-websocket handshake within {timeout}s: {last_error}")
