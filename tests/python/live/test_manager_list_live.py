"""`ctoon.manager list` against the REAL release (not run by default).

Run after a deploy, from an environment where ctoon is installed:

    pytest -m live tests/python/live

Uses the `github-repo` config value (default MohammadRaziei/ctoon), so a fork
points it at its own releases first:  ctoon.manager config set github-repo me/ctoon
`lists.txt` is the very last thing CI publishes, so it can't be checked from
inside the pipeline that creates it -- this is the post-deploy check.
"""

from __future__ import annotations

import urllib.request

import pytest

from ctoon.manager import main

pytestmark = pytest.mark.live


def _fetchable(url: str) -> int:
    # Range GET, not HEAD: release assets redirect to a signed storage URL
    # that doesn't always answer HEAD. One byte is enough.
    req = urllib.request.Request(url, headers={"Range": "bytes=0-0", "User-Agent": "ctoon-manager-test"})
    with urllib.request.urlopen(req, timeout=30) as resp:
        resp.read(1)
        return resp.status


def test_beta_release_is_complete_and_downloadable(capsys):
    rc = main(["list", "--beta", "--urls"])
    out, err = capsys.readouterr()
    assert rc == 0, err
    assert "NOT a stable version" in err

    urls = out.split()
    assert urls, "lists.txt is empty"
    assert not any(u.endswith("/lists.txt") for u in urls), "lists.txt must not list itself"

    # first, last, and one file per extension is plenty to prove the list is real
    sample = {urls[0], urls[-1]}
    seen_ext = set()
    for u in urls:
        ext = u.rsplit(".", 1)[-1]
        if ext not in seen_ext:
            seen_ext.add(ext)
            sample.add(u)
    for u in sorted(sample):
        assert _fetchable(u) in (200, 206), u


def test_release_without_lists_txt_is_a_clean_error(capsys):
    rc = main(["list", "--tag", "v0.0.0-does-not-exist"])
    err = capsys.readouterr().err
    assert rc == 1
    assert "has no lists.txt" in err
