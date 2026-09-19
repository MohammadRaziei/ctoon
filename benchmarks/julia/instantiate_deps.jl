# Resolves this environment's Project.toml — including the [sources]
# pin on CToon (see Project.toml's own comment) — and builds any
# package that needs it (CToon's deps/build.jl among them). A real
# script file rather than `-e "import Pkg; Pkg.instantiate()"`
# deliberately: a CMake COMMAND string containing both a semicolon and
# nested escaped quotes gets mangled by the Unix Makefiles generator —
# same reason fetch_deps.jl (this script's predecessor, before the
# CToon dependency moved into Project.toml's [sources] table) was a
# file instead of an inline `-e`.
import Pkg
Pkg.instantiate()
