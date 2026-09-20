# Resolves this environment's Project.toml — including the [sources]
# pin on CToon (see Project.toml's own comment) — and builds any
# package that needs it (CToon's deps/build.jl among them). A real
# script file rather than `-e "import Pkg; Pkg.update(...)"` deliberately:
# a CMake COMMAND string containing both a semicolon and nested escaped
# quotes gets mangled by the Unix Makefiles generator — same reason
# fetch_deps.jl (this script's predecessor, before the CToon dependency
# moved into Project.toml's [sources] table) was a file instead of an
# inline `-e`.
#
# Pkg.update, not Pkg.instantiate: a `[sources]` entry pinned to
# rev="master" is only resolved into a specific commit ONCE, the first
# time this environment's Manifest.toml gets generated -- after that,
# plain `Pkg.instantiate()` just reinstalls whatever commit is already
# locked in Manifest.toml, even if master has moved on since (this bit
# a real benchmark run: three pushed fixes in a row kept silently
# testing the same stale pre-fix commit). `Pkg.update("CToon")`
# re-resolves that one dependency against the ref in [sources] --
# "master" -- every time, so this benchmark always measures current
# master, which is the whole point of pinning it that way in the first
# place. update() instantiates as part of resolving, so this replaces
# the instantiate() call rather than needing both.
import Pkg
Pkg.update("CToon")
