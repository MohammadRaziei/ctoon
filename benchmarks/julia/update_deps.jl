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
# Both instantiate() AND update("CToon") are needed here, in this
# order, not just one or the other:
#
#   - Pkg.instantiate() does the FIRST-EVER resolve for an environment
#     with no Manifest.toml yet (e.g. a fresh CTOON_JULIA_DEPOT — see
#     CMakeLists.txt's comment on why that's isolated per build tree).
#     Pkg.update("CToon") alone can't do this: it errors with "expected
#     package `CToon` to be registered" when CToon isn't in the
#     Manifest yet at all, since update() upgrades an existing pinned
#     entry rather than performing an initial resolve.
#   - Pkg.update("CToon") is what actually re-resolves against current
#     master on every subsequent run. A `[sources]` entry pinned to
#     rev="master" is only resolved into a specific commit ONCE --
#     after Manifest.toml exists, plain `Pkg.instantiate()` just
#     reinstalls whatever commit is already locked there, even if
#     master has moved on since (this bit a real benchmark run: three
#     pushed fixes in a row kept silently testing the same stale
#     pre-fix commit). update("CToon") re-resolves that one dependency
#     against the ref in [sources] -- "master" -- every time, so this
#     benchmark always measures current master.
import Pkg
Pkg.instantiate()
Pkg.update("CToon")
