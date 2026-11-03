# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Grant Likely <grant.likely@secretlab.ca>
"""Holds the NDI source's PTZ API to obs-ptz's document of it (docs/ptz-device-api.md
there): every proc and signal the source says it has, in ptz_get_api_info, is
declared as the document declares it, a proc is callable from the thread the
document gives and needs the access it gives, and the version is the document's.

The document is found from DISTROAV_PTZ_API_DOC, or beside the controller plugin if
that is in an obs-ptz build folder. The tests skip without it.
"""

import os
import re
from pathlib import Path

import pytest

from test_ptz_device import PTZ_API, add_camera, camera, sender  # noqa: F401


def find_doc():
    given = os.environ.get("DISTROAV_PTZ_API_DOC")
    if given:
        return Path(given)
    controller = os.environ.get("DISTROAV_PTZ_CONTROLLER")
    if controller and len(Path(controller).parents) > 3:
        guess = Path(controller).resolve().parents[3] / "docs" / "ptz-device-api.md"
        if guess.is_file():
            return guess
    return None


@pytest.fixture
def doc():
    path = find_doc()
    if path is None:
        pytest.skip("DISTROAV_PTZ_API_DOC is not set, and there is no obs-ptz document beside the controller")
    return path.read_text()


def section(text, title):
    parts = re.split(r"^## (.+)$", text, flags=re.MULTILINE)
    return dict(zip((t.strip() for t in parts[1::2]), parts[2::2]))[title]


def headings(body):
    """{name: (decl, text of its section)} for the `### ` headings of a section"""
    found = {}
    parts = re.split(r"^### `(.+)`$", body, flags=re.MULTILINE)
    for decl, text in zip(parts[1::2], parts[2::2]):
        found[re.search(r"(ptz_\w+)\(", decl).group(1)] = (decl, text)
    return found


def test_the_version_is_the_documents(doc):
    match = re.search(r"This is version \*\*(\d+)\.(\d+)\*\* of the PTZ API", doc)
    assert {"major": int(match.group(1)), "minor": int(match.group(2))} == PTZ_API


def test_every_proc_is_declared_as_the_document_declares_it(ptz, camera, doc):  # noqa: F811
    documented = headings(section(doc, "Per-device proc_handler"))
    info = ptz.call("ptz_get_api_info", camera.uuid)["return"]
    assert info["procs"]
    for proc in info["procs"]:
        assert proc["name"] in documented, f"{proc['name']} is not in the document"
        decl, text = documented[proc["name"]]
        assert proc["decl"] == decl
        thread = "device" if "Device thread." in text else "any"
        assert proc["thread"] == thread, proc["name"]
        access = re.search(r"^Access: (\w+)\.", text, flags=re.MULTILINE)
        assert proc["access"] == (access.group(1) if access else "control"), proc["name"]


def test_every_signal_is_declared_as_the_document_declares_it(ptz, camera, doc):  # noqa: F811
    documented = headings(section(doc, "Per-device signal_handler"))
    info = ptz.call("ptz_get_api_info", camera.uuid)["return"]
    assert info["signals"]
    for signal in info["signals"]:
        assert signal["name"] in documented, f"{signal['name']} is not in the document"
        assert signal["decl"] == documented[signal["name"]][0]


def test_what_the_document_requires_of_every_device_is_there(ptz, camera, doc):  # noqa: F811
    """The procs a device has whatever its features: the document's, less those of a feature this one lacks"""
    documented = headings(section(doc, "Per-device proc_handler"))
    info = ptz.call("ptz_get_api_info", camera.uuid)["return"]
    have = {p["name"] for p in info["procs"]}
    needs_feature = {name for name, (_, text) in documented.items() if re.search(r"^Needs the `", text, re.MULTILINE)}
    missing = set(documented) - have - needs_feature - {"ptz_get_api_info"}
    # What a device may leave out: the thumbnail it makes none of, and a relative move it cannot do
    assert not missing - {"ptz_move_rel"}, missing
