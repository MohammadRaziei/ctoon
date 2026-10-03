"""`python -m ctoon.manager matlab detect` against a fake MATLAB install."""

from __future__ import annotations

import json
import os
import platform
import stat
import sys

import pytest

from ctoon.manager import main
from ctoon.manager.langs.matlab import probe

pytestmark = pytest.mark.skipif(
    sys.platform == "win32", reason="fake install uses shell-script stubs"
)

VERSION_XML = (
    '<?xml version="1.0" encoding="UTF-8"?>'
    "<MathWorks_version_info><version>24.1.0.2537033</version>"
    "<release>{release}</release></MathWorks_version_info>"
)


def _make_install(root, release="R2024a", with_mex=True, xml=None):
    bin_dir = root / "MATLAB" / release / "bin"
    bin_dir.mkdir(parents=True)
    names = ["matlab"] + (["mex"] if with_mex else [])
    for name in names:
        p = bin_dir / name
        p.write_text("#!/bin/sh\nexit 0\n")
        p.chmod(p.stat().st_mode | stat.S_IXUSR)
    # The gist looks for VersionInfo.xml in <root>/Contents/ on macOS and in
    # <root>/ everywhere else; mirror that so the fake install is valid on
    # whichever OS the tests run (platform.system() is read at call time, so
    # a test that patches it gets that OS's layout).
    xml_dir = bin_dir.parent / "Contents" if platform.system() == "Darwin" else bin_dir.parent
    xml_dir.mkdir(exist_ok=True)
    (xml_dir / "VersionInfo.xml").write_text(
        xml if xml is not None else VERSION_XML.format(release=release)
    )
    return bin_dir


@pytest.fixture
def env(tmp_path, monkeypatch):
    """PATH with only a symlinked `matlab` (like /usr/local/bin/matlab)."""
    link_dir = tmp_path / "linkbin"
    link_dir.mkdir()
    monkeypatch.setenv("PATH", str(link_dir))
    monkeypatch.setenv("CTOON_STATE", str(tmp_path / "state.json"))
    return tmp_path, link_dir


def _link(bin_dir, link_dir):
    os.symlink(bin_dir / "matlab", link_dir / "matlab")


def test_detect_finds_matlab_release_and_mex(env):
    tmp_path, link_dir = env
    bin_dir = _make_install(tmp_path)
    _link(bin_dir, link_dir)

    info = probe.detect()

    assert info["found"] is True
    assert info["executable"] == os.path.realpath(bin_dir / "matlab")
    assert info["root"] == str(bin_dir.parent.resolve())
    assert info["release"] == "R2024a"
    assert info["mex"] == str(bin_dir / "mex")
    assert info["meets_minimum"] is True


def test_detect_not_found(env, monkeypatch):
    # No matlab on PATH; make the default install roots empty too.
    monkeypatch.setattr(probe._gist.platform, "system", lambda: "Plan9")
    info = probe.detect()
    assert info["found"] is False
    assert "could not be found" in info["error"]


@pytest.mark.parametrize("system", ["Linux", "Darwin"])
def test_detect_release_xml_layout_per_os(env, monkeypatch, system):
    # Both layouts get exercised on every CI OS, not only on the matching one.
    monkeypatch.setattr(probe._gist.platform, "system", lambda: system)
    tmp_path, link_dir = env
    bin_dir = _make_install(tmp_path)
    _link(bin_dir, link_dir)
    assert probe.detect()["release"] == "R2024a"


def test_detect_missing_mex(env):
    tmp_path, link_dir = env
    bin_dir = _make_install(tmp_path, with_mex=False)
    _link(bin_dir, link_dir)
    info = probe.detect()
    assert info["found"] is True and info["mex"] is None


def test_detect_too_old(env):
    tmp_path, link_dir = env
    bin_dir = _make_install(tmp_path, release="R2012b")
    _link(bin_dir, link_dir)
    assert probe.detect()["meets_minimum"] is False


def test_detect_broken_xml(env):
    tmp_path, link_dir = env
    bin_dir = _make_install(tmp_path, xml="<oops")
    _link(bin_dir, link_dir)
    info = probe.detect()
    assert info["found"] is True
    assert info["release"] is None and "VersionInfo.xml" in info["error"]


@pytest.mark.parametrize(
    "release, expected",
    [("R2014b", True), ("R2021a", True), ("R2013b", False), ("R2014a", False),
     ("9.9", None), ("", None), (None, None)],
)
def test_release_ordering(release, expected):
    assert probe.meets(release, "R2014b") is expected


def test_cli_detect_json_saves_state(env, capsys):
    tmp_path, link_dir = env
    bin_dir = _make_install(tmp_path)
    _link(bin_dir, link_dir)

    rc = main(["matlab", "detect", "--json"])

    assert rc == 0
    printed = json.loads(capsys.readouterr().out)
    saved = json.loads((tmp_path / "state.json").read_text())
    assert saved["schema"] == 1
    assert saved["matlab"]["release"] == printed["release"] == "R2024a"
    assert "detected_at" in saved["matlab"]


def test_cli_exit_code_nonzero_when_not_usable(env, monkeypatch):
    monkeypatch.setattr(probe._gist.platform, "system", lambda: "Plan9")
    assert main(["matlab", "detect", "--no-save"]) == 1
    assert not (env[0] / "state.json").exists()
