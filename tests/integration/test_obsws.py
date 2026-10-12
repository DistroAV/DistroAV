# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Grant Likely <grant.likely@secretlab.ca>
"""Covers the obs-websocket client's wait for a server that is still
starting: OBS accepts the connection before it is ready, and then closes it,
and the client has to try again rather than fail on the first.
"""

import json
import threading

from obsws import connect_when_ready
from websockets.sync.server import serve


def test_connect_when_ready_retries_a_server_that_closes_during_startup():
    attempts = []

    def handler(connection):
        attempts.append(1)
        if len(attempts) <= 2:
            return  # closes the connection before it says Hello
        connection.send(json.dumps({"op": 0, "d": {"rpcVersion": 1}}))
        connection.recv()  # Identify
        connection.send(json.dumps({"op": 2, "d": {"negotiatedRpcVersion": 1}}))
        connection.recv()

    with serve(handler, "127.0.0.1", 0) as server:
        port = server.socket.getsockname()[1]
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        client = connect_when_ready(f"ws://127.0.0.1:{port}", None, timeout=15)
        client.close()
        server.shutdown()

    assert len(attempts) == 3
