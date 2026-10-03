"""Expected failures: the message is shown to the user, exit code 1."""

from __future__ import annotations


class ManagerError(Exception):
    pass
