"""`python -m ctoon.manager list` against a local fake release host."""

from __future__ import annotations

import functools
import http.server
import threading

import pytest

from ctoon.manager import main
from ctoon.manager.commands import listing
from ctoon.manager.core import release


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
    base = f"http://127.0.0.1:{server.server_address[1]}"
    monkeypatch.setenv("CTOON_RELEASE_BASE", base)
    monkeypatch.setattr(release, "installed_version", lambda: "0.8.3")

    def publish(tag, text):
        d = tmp_path / "download" / tag
        d.mkdir(parents=True, exist_ok=True)
        (d / "lists.txt").write_text(text)

    publish.base = base
    yield publish
    server.shutdown()
    server.server_close()  # pyproject has filterwarnings=error: an unclosed socket fails the run
    thread.join(timeout=5)


def test_beta_lists_files_and_warns(host, capsys):
    host("__beta__", "ctoon-0.8.3.tar.gz\nctoon-matlab-0.8.3-linux-x86_64.zip\n")
    assert main(["list", "--beta"]) == 0
    out, err = capsys.readouterr()
    assert out.splitlines() == ["ctoon-0.8.3.tar.gz", "ctoon-matlab-0.8.3-linux-x86_64.zip"]
    assert "NOT a stable version" in err


def test_beta_missing_is_error_and_says_not_available(host, capsys):
    assert main(["list", "--beta"]) == 1
    out, err = capsys.readouterr()
    assert out == ""
    assert "beta version is not available yet" in err
    assert "NOT a stable version" in err  # warning still shown


def test_stable_uses_installed_version_tag_without_warning(host, capsys):
    host("v0.8.3", "a.whl\nb.crate\n")
    assert main(["list"]) == 0
    out, err = capsys.readouterr()
    assert out.splitlines() == ["a.whl", "b.crate"]
    assert "warning" not in err


def test_stable_missing_lists_txt(host, capsys):
    assert main(["list"]) == 1
    assert "release v0.8.3 has no lists.txt" in capsys.readouterr().err


def test_explicit_tag_and_urls(host, capsys):
    host("v1.0.0", "x.zip\n")
    assert main(["list", "--tag", "v1.0.0", "--urls"]) == 0
    assert capsys.readouterr().out.strip() == f"{host.base}/download/v1.0.0/x.zip"


def test_parse_skips_blank_comments_and_paths():
    text = "a.zip\n\n# note\n  b.zip  \n../evil\nsub/c.zip\n..\n"
    assert listing.parse(text) == ["a.zip", "b.zip"]


def test_empty_list_is_error(host, capsys):
    host("__beta__", "\n# nothing\n")
    assert main(["list", "--beta"]) == 1
    assert "is empty" in capsys.readouterr().err


def test_unreachable_host(monkeypatch, capsys):
    monkeypatch.setenv("CTOON_RELEASE_BASE", "http://127.0.0.1:9")  # closed port
    monkeypatch.setattr(release, "installed_version", lambda: "0.8.3")
    assert main(["list", "--beta"]) == 1
    assert "could not reach" in capsys.readouterr().err


def test_beta_and_tag_are_mutually_exclusive():
    with pytest.raises(SystemExit) as e:
        main(["list", "--beta", "--tag", "v1"])
    assert e.value.code == 2
