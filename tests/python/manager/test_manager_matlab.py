"""`ctoon.manager matlab detect` against the MATLAB that is really installed.

No fake installs and no monkeypatching: every test reads the real machine, so
what passes here is what a user would actually get. Skipped when MATLAB isn't
installed. Run by hand (tests/python/manager/ is not collected by CI):

    pytest tests/python/manager/test_manager_matlab.py
"""

from __future__ import annotations

import json
import os
import platform
import re
import shutil
import xml.etree.ElementTree as ET
from pathlib import Path

import pytest

from ctoon.manager import main
from ctoon.manager.langs.matlab import probe

INFO = probe.detect()

needs_matlab = pytest.mark.skipif(
    not INFO["found"], reason="MATLAB is not installed on this machine"
)


@needs_matlab
def test_executable_is_a_real_file_in_a_bin_dir():
    exe = Path(INFO["executable"])
    assert exe.is_file()
    assert os.access(exe, os.X_OK)
    assert exe.name in ("matlab", "matlab.exe")
    assert exe.parent.name == "bin"
    # the detector reports the resolved path, never a symlink
    assert str(exe) == os.path.realpath(exe)


@needs_matlab
def test_root_is_the_parent_of_bin():
    exe = Path(INFO["executable"])
    assert Path(INFO["root"]) == exe.parent.parent
    assert Path(INFO["root"]).is_dir()


@needs_matlab
def test_matlab_on_path_resolves_to_the_detected_install():
    # On a machine where `which matlab` works, it must be the same binary
    # (this follows the real /usr/local/bin/matlab -> .../bin/matlab symlink).
    on_path = shutil.which("matlab")
    if on_path is None:
        pytest.skip("matlab is not on PATH here; found through the default install dirs")
    assert os.path.realpath(on_path) == INFO["executable"]


@needs_matlab
def test_release_matches_versioninfo_xml_on_disk():
    root = Path(INFO["root"])
    # the layout the detector relies on: Contents/ on macOS, root elsewhere
    xml_path = root / "Contents" / "VersionInfo.xml" if platform.system() == "Darwin" else root / "VersionInfo.xml"
    assert xml_path.is_file(), xml_path

    # read it a second, independent way
    release = ET.parse(xml_path).getroot().find(".//release").text.strip()
    assert INFO["release"] == release
    assert re.fullmatch(r"R\d{4}[ab]", release)


@needs_matlab
def test_mex_is_next_to_the_matlab_binary():
    assert INFO["mex"] is not None, "no mex next to the matlab binary"
    mex = Path(INFO["mex"])
    assert mex.is_file()
    assert mex.parent == Path(INFO["executable"]).parent
    assert mex.name in ("mex", "mex.bat", "mex.exe")


@needs_matlab
def test_install_meets_the_minimum_release():
    assert INFO["min_release"] == probe.MIN_RELEASE
    assert INFO["meets_minimum"] is True
    assert INFO["error"] is None


@needs_matlab
def test_cli_json_matches_detect_and_records_state(tmp_path, monkeypatch, capsys):
    # CTOON_STATE only redirects where *our own* record is written.
    state_file = tmp_path / "state.json"
    monkeypatch.setenv("CTOON_STATE", str(state_file))

    assert main(["matlab", "detect", "--json"]) == 0

    printed = json.loads(capsys.readouterr().out)
    assert printed == INFO
    saved = json.loads(state_file.read_text())
    assert saved["schema"] == 1
    assert {k: v for k, v in saved["matlab"].items() if k != "detected_at"} == INFO
    assert "detected_at" in saved["matlab"]


@needs_matlab
def test_cli_human_output_names_the_real_paths(tmp_path, monkeypatch, capsys):
    monkeypatch.setenv("CTOON_STATE", str(tmp_path / "state.json"))
    assert main(["matlab", "detect"]) == 0
    out = capsys.readouterr().out
    for value in (INFO["executable"], INFO["root"], INFO["release"], INFO["mex"]):
        assert value in out


@needs_matlab
def test_no_save_leaves_no_state(tmp_path, monkeypatch):
    state_file = tmp_path / "state.json"
    monkeypatch.setenv("CTOON_STATE", str(state_file))
    assert main(["matlab", "detect", "--no-save"]) == 0
    assert not state_file.exists()


# Pure string logic -- no filesystem involved, so it runs everywhere.
@pytest.mark.parametrize(
    "release, expected",
    [("R2014b", True), ("R2021a", True), ("R2024a", True), ("R2013b", False),
     ("R2014a", False), ("9.9", None), ("", None), (None, None)],
)
def test_release_ordering(release, expected):
    assert probe.meets(release, "R2014b") is expected
