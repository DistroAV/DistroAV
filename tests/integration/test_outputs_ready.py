# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Grant Likely <grant.likely@secretlab.ca>
"""Covers that the NDI outputs exist by the time OBS is said to be ready, so
that a test can ask for them straight after a restart.
"""

OUTPUTS = {"NDI Main Output", "NDI Preview Output"}


def test_ndi_outputs_are_listed_as_soon_as_a_restart_returns(obs_world):
    for _ in range(3):
        obs_world.restart_obs()
        listed = {o["outputName"] for o in obs_world.ws.call("GetOutputList")["outputs"]}
        assert OUTPUTS <= listed
