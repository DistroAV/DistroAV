# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Grant Likely <grant.likely@secretlab.ca>
"""Covers that the plugin loads in a real OBS and registers its kinds."""


def test_plugin_registers_its_source_kind(obs_world):
    kinds = obs_world.ws.call("GetInputKindList")["inputKinds"]
    assert "ndi_source" in kinds


def test_plugin_registers_its_filter_kinds(obs_world):
    kinds = obs_world.ws.call("GetSourceFilterKindList")["sourceFilterKinds"]
    assert {"ndi_filter", "ndi_audiofilter"} <= set(kinds)


def test_obs_restarts_on_the_same_profile(obs_world):
    scene = obs_world.create_scene()
    obs_world.restart_obs()
    scenes = [s["sceneName"] for s in obs_world.ws.call("GetSceneList")["scenes"]]
    assert scene in scenes
