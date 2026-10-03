"""`ctoon.manager config ...` -- show / change the manager's settings."""

from __future__ import annotations

from ..core import config


def _show(args) -> int:
    width = max(len(k) for k in config.KEYS)
    for key, (value, source) in config.effective().items():
        print(f"{key:<{width}} = {value}   ({source})")
    return 0


def _path(args) -> int:
    print(config.path())
    return 0


def _get(args) -> int:
    print(config.get(args.key))
    return 0


def _set(args) -> int:
    config.set(args.key, args.value)
    print(f"{args.key} = {args.value}")
    return 0


def _unset(args) -> int:
    config.unset(args.key)
    print(f"{args.key} = {config.get(args.key)}   (default)")
    return 0


def add_parser(subparsers) -> None:
    p = subparsers.add_parser("config", help="show or change settings")
    sub = p.add_subparsers(dest="action", metavar="<action>", required=True)

    sub.add_parser("show", help="all settings and where each value comes from").set_defaults(func=_show)
    sub.add_parser("path", help="print the config file path").set_defaults(func=_path)

    g = sub.add_parser("get", help="print one setting")
    g.add_argument("key")
    g.set_defaults(func=_get)

    s = sub.add_parser("set", help="change a setting")
    s.add_argument("key")
    s.add_argument("value")
    s.set_defaults(func=_set)

    u = sub.add_parser("unset", help="go back to the default of a setting")
    u.add_argument("key")
    u.set_defaults(func=_unset)
