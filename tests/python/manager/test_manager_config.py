"""`ctoon.manager config` and the `github-repo` setting that feeds release URLs."""

from __future__ import annotations

import json

import pytest

from ctoon.manager import main
from ctoon.manager.core import config, release


@pytest.fixture(autouse=True)
def cfg(tmp_path, monkeypatch):
    p = tmp_path / "etc" / "config.json"
    monkeypatch.setenv("CTOON_CONFIG", str(p))
    return p


def test_default_repo_is_the_upstream_one(cfg):
    assert config.get("github-repo") == "MohammadRaziei/ctoon"
    assert release.base_url() == "https://github.com/MohammadRaziei/ctoon/releases"
    assert not cfg.exists()  # reading never creates the file


def test_set_changes_release_urls_for_forks(cfg, capsys):
    assert main(["config", "set", "github-repo", "someone/ctoon-fork"]) == 0
    assert json.loads(cfg.read_text())["github-repo"] == "someone/ctoon-fork"

    assert release.base_url() == "https://github.com/someone/ctoon-fork/releases"
    assert release.asset_url("__beta__", "lists.txt") == (
        "https://github.com/someone/ctoon-fork/releases/download/__beta__/lists.txt"
    )


def test_show_reports_source(cfg, capsys):
    main(["config", "show"])
    assert "(default)" in capsys.readouterr().out
    main(["config", "set", "github-repo", "a/b"])
    capsys.readouterr()
    main(["config", "show"])
    out = capsys.readouterr().out
    assert "a/b" in out and str(cfg) in out


def test_get_path_and_unset(cfg, capsys):
    main(["config", "set", "github-repo", "a/b"])
    capsys.readouterr()
    assert main(["config", "get", "github-repo"]) == 0
    assert capsys.readouterr().out.strip() == "a/b"
    assert main(["config", "path"]) == 0
    assert capsys.readouterr().out.strip() == str(cfg)
    assert main(["config", "unset", "github-repo"]) == 0
    assert config.get("github-repo") == "MohammadRaziei/ctoon"


@pytest.mark.parametrize("bad", ["", "justname", "a/b/c", "a b/c", "/x", "x/"])
def test_set_rejects_bad_repo(cfg, bad, capsys):
    assert main(["config", "set", "github-repo", bad]) == 1
    assert "owner/name" in capsys.readouterr().err
    assert not cfg.exists()


def test_unknown_key(cfg, capsys):
    assert main(["config", "get", "nope"]) == 1
    err = capsys.readouterr().err
    assert "unknown key" in err and "github-repo" in err


def test_corrupt_config_is_reported_not_ignored(cfg, capsys):
    cfg.parent.mkdir(parents=True)
    cfg.write_text("{oops")
    assert main(["config", "show"]) == 1
    assert "not valid JSON" in capsys.readouterr().err


def test_unwritable_config_location(tmp_path, monkeypatch, capsys):
    blocker = tmp_path / "file"
    blocker.write_text("x")
    monkeypatch.setenv("CTOON_CONFIG", str(blocker / "config.json"))  # parent is a file
    assert main(["config", "set", "github-repo", "a/b"]) == 1
    assert "could not write" in capsys.readouterr().err
