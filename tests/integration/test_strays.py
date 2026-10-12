# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Grant Likely <grant.likely@secretlab.ca>
"""Covers the session refusing to start while an OBS from an earlier run is
still running, which would hold the ports and NDI names the tests expect.
"""

import os
import subprocess
import sys
import time
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]


def test_session_refuses_to_start_with_a_stray_obs():
    # Looks to ps like an OBS this suite started: called obs, with its marker.
    # Not a Python: a framework Python on macOS starts itself again, as itself.
    # (the `; :` keeps the inner bash from exec'ing sleep, and losing its arguments)
    fake = subprocess.Popen(["bash", "-c", 'exec -a obs bash -c "sleep 60; :" distroav-it-fake'])
    try:
        time.sleep(0.5)
        env = {k: v for k, v in os.environ.items() if k != "DISTROAV_ALLOW_STRAYS"}
        result = subprocess.run(
            [sys.executable, "-m", "pytest", "tests/integration/test_smoke.py", "-p", "no:cacheprovider"],
            cwd=REPO_ROOT, env=env, capture_output=True, text=True, timeout=60)
    finally:
        fake.kill()
        fake.wait()

    assert result.returncode == 3, result.stdout + result.stderr
    assert str(fake.pid) in result.stdout + result.stderr
