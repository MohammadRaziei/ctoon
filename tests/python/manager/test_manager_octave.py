"""`ctoon.manager octave <action>` against the Octave that is really installed.

Same philosophy as test_manager_matlab.py: detection tests read the real
machine, no fake installs. `install` builds the binding for real, from a
source distribution packed out of THIS checkout the same way mex.yml packs the
release one, so these tests also prove that a user following the release
instructions ends up with a working binding. The "download" path talks to a
local fake release host, like test_manager_list.py. Run by hand
(tests/python/manager/ is not collected by CI):

    pytest tests/python/manager/test_manager_octave.py

Skipped when Octave is missing; the install tests also need mkoctfile and a C
compiler (building only -- using the result needs neither).
"""

from __future__ import annotations

import functools
import http.server
import json
import os
import re
import subprocess
import threading
import zipfile
from pathlib import Path

import pytest

from ctoon.manager import main
from ctoon.manager.core import release
from ctoon.manager.core.errors import ManagerError
from ctoon.manager.langs.octave import cli, probe

REPO = Path(__file__).resolve().parents[3]
INFO = probe.detect()

needs_octave = pytest.mark.skipif(not INFO["found"], reason="GNU Octave is not installed on this machine")
can_build = bool(INFO["found"] and INFO["mkoctfile"] and INFO["compiler"] and INFO["meets_minimum"] is not False)
needs_build = pytest.mark.skipif(not can_build, reason="Octave + mkoctfile + a C compiler are needed to build")

VERSION_RE = re.compile(r"^\d+\.\d+(\.\d+)?$")


@pytest.fixture(autouse=True)
def _isolated(tmp_path_factory, monkeypatch):
    # never read or write a developer's real config / state file
    d = tmp_path_factory.mktemp("cfg")
    monkeypatch.setenv("CTOON_CONFIG", str(d / "config.json"))
    monkeypatch.setenv("CTOON_STATE", str(d / "state.json"))


def _pack_source_dist(out_dir: Path, version: str = "9.9.9") -> Path:
    """The same layout as the `dist` job in .github/workflows/mex.yml."""
    name = f"ctoon-matlab-{version}-src"
    m = REPO / "src" / "bindings" / "matlab"
    files = {
        f"{name}/{p.relative_to(m).as_posix()}": p
        for p in (m / "ctoon").glob("*.m")
    }
    for f in ("ctoon_build.m", "ctoon_install.m", "ctoon_clean.m", "ctoon_mex.c"):
        files[f"{name}/{f}"] = m / f
    files[f"{name}/ctoon.c"] = REPO / "src" / "ctoon.c"
    files[f"{name}/ctoon.h"] = REPO / "include" / "ctoon.h"
    files[f"{name}/LICENSE"] = REPO / "LICENSE"

    out = out_dir / f"{name}.zip"
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
        for arc, src in sorted(files.items()):
            z.write(src, arc)
    return out


@pytest.fixture(scope="module")
def source_zip(tmp_path_factory) -> Path:
    return _pack_source_dist(tmp_path_factory.mktemp("srcdist"))


def _octave(expr: str) -> subprocess.CompletedProcess:
    return subprocess.run(
        [INFO["executable"], "--no-gui", "--norc", "--quiet", "--eval", expr],
        capture_output=True, text=True, timeout=120,
    )


# ---------------------------------------------------------------------------
# detect
# ---------------------------------------------------------------------------

@needs_octave
def test_executable_is_a_real_octave_cli():
    exe = Path(INFO["executable"])
    assert exe.is_file()
    assert os.access(exe, os.X_OK)
    assert exe.stem == "octave-cli"
    # the detector reports the resolved path, never a symlink
    assert str(exe) == os.path.realpath(exe)


@needs_octave
def test_root_is_the_parent_of_bin():
    exe = Path(INFO["executable"])
    assert Path(INFO["root"]) == exe.parent.parent
    assert Path(INFO["root"]).is_dir()


@needs_octave
def test_version_is_what_octave_itself_reports():
    assert VERSION_RE.match(INFO["version"])
    proc = _octave("disp(OCTAVE_VERSION)")
    assert proc.returncode == 0
    assert proc.stdout.strip() == INFO["version"]


@needs_octave
def test_mkoctfile_when_present_is_a_real_file():
    if INFO["mkoctfile"] is None:
        pytest.skip("no mkoctfile (Octave without development files)")
    assert Path(INFO["mkoctfile"]).is_file()
    assert Path(INFO["mkoctfile"]).stem == "mkoctfile"
    if INFO["compiler"] is not None:
        assert Path(INFO["compiler"]).is_file()


@needs_octave
def test_install_meets_the_minimum_version():
    assert INFO["min_version"] == probe.MIN_VERSION
    assert INFO["meets_minimum"] is True
    assert INFO["error"] is None


@needs_octave
def test_cli_json_matches_detect_and_records_state(tmp_path, monkeypatch, capsys):
    state_file = tmp_path / "state.json"
    monkeypatch.setenv("CTOON_STATE", str(state_file))

    assert main(["octave", "detect", "--json"]) == 0

    printed = json.loads(capsys.readouterr().out)
    assert printed == INFO
    saved = json.loads(state_file.read_text())
    assert saved["schema"] == 1
    assert {k: v for k, v in saved["octave"].items() if k != "detected_at"} == INFO
    assert "detected_at" in saved["octave"]


@needs_octave
def test_cli_human_output_names_the_real_paths(capsys):
    assert main(["octave", "detect"]) == 0
    out = capsys.readouterr().out
    for value in (INFO["executable"], INFO["root"], INFO["version"]):
        assert value in out


@needs_octave
def test_no_save_leaves_no_state(tmp_path, monkeypatch):
    state_file = tmp_path / "state.json"
    monkeypatch.setenv("CTOON_STATE", str(state_file))
    assert main(["octave", "detect", "--no-save"]) == 0
    assert not state_file.exists()


def test_not_found_is_reported_and_fails(monkeypatch, capsys):
    monkeypatch.setattr(probe, "find_octave", lambda: None)
    assert main(["octave", "detect", "--no-save"]) == 1
    assert "not found" in capsys.readouterr().out


# Pure string logic -- no filesystem involved, so it runs everywhere.
@pytest.mark.parametrize(
    "version, expected",
    [("8.1", True), ("8.1.0", True), ("8.4.0", True), ("9.2.0", True), ("11.3.0", True),
     ("8.0.9", False), ("7.3.0", False), ("6.4.0", False),
     ("8", None), ("dev", None), ("", None), (None, None)],
)
def test_version_ordering(version, expected):
    assert probe.meets(version, "8.1") is expected


# ---------------------------------------------------------------------------
# install
# ---------------------------------------------------------------------------

@needs_build
def test_install_from_local_zip_builds_a_working_binding(source_zip, tmp_path, capsys):
    prefix = tmp_path / "ctoon-octave"
    assert main(["octave", "install", "--zip", str(source_zip), "--prefix", str(prefix)]) == 0
    out = capsys.readouterr().out
    assert f"addpath('{prefix}')" in out

    # Octave resolves a private/ folder inside +ctoon wrongly, so the gateway
    # sits next to the package: addpath(prefix) must be enough.
    assert (prefix / "ctoon_mex.mex").is_file()
    assert (prefix / "+ctoon" / "encode.m").is_file()

    proc = _octave(
        f"addpath('{prefix}'); warning('off', 'Octave:shadowed-function'); "
        "s = ctoon.encode(struct('name', 'Alice', 'age', 30)); disp(s); "
        "v = ctoon.decode(s); assert(strcmp(v.name, 'Alice'));"
    )
    assert proc.returncode == 0, proc.stderr
    assert "name: Alice" in proc.stdout


@needs_build
def test_installed_binding_needs_no_compiler(source_zip, tmp_path, monkeypatch):
    prefix = tmp_path / "ctoon-octave"
    assert main(["octave", "install", "--zip", str(source_zip), "--prefix", str(prefix)]) == 0

    # Running the result must not touch any compiler: break them all.
    monkeypatch.setenv("CC", "/nonexistent")
    monkeypatch.setenv("CXX", "/nonexistent")
    monkeypatch.setenv("PATH", os.path.dirname(INFO["executable"]) + os.pathsep + "/nonexistent")
    proc = _octave(f"addpath('{prefix}'); warning('off', 'Octave:shadowed-function'); disp(ctoon.dumps(1.5));")
    assert proc.returncode == 0, proc.stderr
    assert proc.stdout.strip() == "1.5"


@needs_build
def test_install_records_state(source_zip, tmp_path, monkeypatch):
    state_file = tmp_path / "state.json"
    monkeypatch.setenv("CTOON_STATE", str(state_file))
    prefix = tmp_path / "ctoon-octave"
    assert main(["octave", "install", "--zip", str(source_zip), "--prefix", str(prefix)]) == 0
    saved = json.loads(state_file.read_text())["octave"]
    assert saved["install_dir"] == str(prefix)
    assert saved["installed_from"] == str(source_zip)
    assert saved["version"] == INFO["version"]


@needs_build
def test_reinstall_replaces_the_previous_build(source_zip, tmp_path):
    prefix = tmp_path / "ctoon-octave"
    assert main(["octave", "install", "--zip", str(source_zip), "--prefix", str(prefix)]) == 0
    stale = prefix / "stale.txt"
    stale.write_text("left over from the previous install")
    assert main(["octave", "install", "--zip", str(source_zip), "--prefix", str(prefix)]) == 0
    assert not stale.exists()
    assert (prefix / "+ctoon" / "encode.m").is_file()


class _Quiet(http.server.SimpleHTTPRequestHandler):
    def log_message(self, *a):  # keep pytest output clean
        pass


@pytest.fixture
def host(tmp_path, monkeypatch):
    """Serve tmp_path as the releases/ root: <root>/download/<tag>/<file>."""
    handler = functools.partial(_Quiet, directory=str(tmp_path))
    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    monkeypatch.setattr(release, "base_url", lambda: f"http://127.0.0.1:{server.server_address[1]}")
    monkeypatch.setattr(release, "installed_version", lambda: "9.9.9")

    def publish(tag, files: dict[str, bytes]):
        d = tmp_path / "download" / tag
        d.mkdir(parents=True, exist_ok=True)
        for name, data in files.items():
            (d / name).write_bytes(data)
        (d / "lists.txt").write_text("".join(f"{n}\n" for n in sorted(files)))

    yield publish
    server.shutdown()
    server.server_close()  # pyproject has filterwarnings=error: an unclosed socket fails the run
    thread.join(timeout=5)


@needs_build
def test_install_downloads_the_source_dist_of_the_release(host, source_zip, tmp_path, capsys):
    host("v9.9.9", {
        source_zip.name: source_zip.read_bytes(),
        "ctoon-matlab-9.9.9-linux-x86_64.zip": b"prebuilt MATLAB binary: Octave must not pick this one",
    })
    prefix = tmp_path / "ctoon-octave"
    assert main(["octave", "install", "--prefix", str(prefix)]) == 0
    assert f"/download/v9.9.9/{source_zip.name}" in capsys.readouterr().out
    assert (prefix / "+ctoon" / "encode.m").is_file()


def test_release_without_a_source_dist_is_an_error(host):
    host("v9.9.9", {"ctoon-matlab-9.9.9-linux-x86_64.zip": b"x", "ctoon-0.8.3.tar.gz": b"x"})
    with pytest.raises(ManagerError, match="source distribution"):
        cli._find_source_name("v9.9.9")


def test_release_without_lists_txt_is_an_error(host):
    with pytest.raises(ManagerError, match="lists.txt"):
        cli._find_source_name("v1.2.3")


@needs_build
def test_install_reports_an_error_for_a_missing_zip(tmp_path, capsys):
    assert main(["octave", "install", "--zip", str(tmp_path / "nope.zip"), "--prefix", str(tmp_path / "p")]) == 1
    assert "no such file" in capsys.readouterr().err


def test_install_without_octave_says_so(monkeypatch, tmp_path, capsys):
    monkeypatch.setattr(probe, "find_octave", lambda: None)
    assert main(["octave", "install", "--zip", "x.zip", "--prefix", str(tmp_path / "p")]) == 1
    assert "octave-cli not found" in capsys.readouterr().err


@needs_octave
def test_install_without_mkoctfile_explains_that_only_the_build_needs_it(monkeypatch, tmp_path, capsys):
    monkeypatch.setattr(probe, "find_mkoctfile", lambda bin_dir: None)
    assert main(["octave", "install", "--zip", "x.zip", "--prefix", str(tmp_path / "p")]) == 1
    err = capsys.readouterr().err
    assert "mkoctfile not found" in err
    assert "Only the build needs it" in err


@needs_octave
def test_install_without_a_compiler_explains_that_only_the_build_needs_it(monkeypatch, tmp_path, capsys):
    if INFO["mkoctfile"] is None:
        pytest.skip("no mkoctfile on this machine")
    monkeypatch.setattr(probe, "compiler_of", lambda mkoctfile: None)
    assert main(["octave", "install", "--zip", "x.zip", "--prefix", str(tmp_path / "p")]) == 1
    err = capsys.readouterr().err
    assert "no C compiler" in err
    assert "Only the build needs one" in err


# ---------------------------------------------------------------------------
# unpacking (pure Python -- runs everywhere)
# ---------------------------------------------------------------------------

def test_extract_finds_the_dist_folder(tmp_path):
    z = _pack_source_dist(tmp_path)
    folder = cli._extract(str(z), str(tmp_path / "out"))
    assert Path(folder).name == "ctoon-matlab-9.9.9-src"
    assert (Path(folder) / "ctoon_build.m").is_file()


def test_extract_rejects_paths_that_escape_the_folder(tmp_path):
    evil = tmp_path / "evil.zip"
    with zipfile.ZipFile(evil, "w") as z:
        z.writestr("../escaped.txt", "x")
    with pytest.raises(ManagerError, match="unsafe path"):
        cli._extract(str(evil), str(tmp_path / "out"))
    assert not (tmp_path / "escaped.txt").exists()


def test_extract_rejects_a_zip_that_is_not_a_ctoon_dist(tmp_path):
    other = tmp_path / "other.zip"
    with zipfile.ZipFile(other, "w") as z:
        z.writestr("readme.txt", "hello")
    with pytest.raises(ManagerError, match="not a CToon MATLAB source distribution"):
        cli._extract(str(other), str(tmp_path / "out"))


def test_extract_rejects_a_file_that_is_not_a_zip(tmp_path):
    bad = tmp_path / "bad.zip"
    bad.write_bytes(b"this is not a zip")
    with pytest.raises(ManagerError, match="not a valid zip"):
        cli._extract(str(bad), str(tmp_path / "out"))


def test_octave_string_quoting():
    assert cli._q("/tmp/it's here") == "'/tmp/it''s here'"
