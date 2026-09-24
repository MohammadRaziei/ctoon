# JSON.jl (a test-only dependency added specifically for this file — see
# src/bindings/julia/Project.toml's comment; CToon.jl itself never
# `using`s it) vs CToon's own JSON reader/writer (CToon.parse/CToon.dumps,
# which call ctoon.c's own cj_* parser via shim.c — see CToon.parse's own
# comment on why it, not a separate parse_json, is the JSON entry point).
#
# Every other Julia test builds native Julia values by hand or goes
# through CToon.parse() -- CToon's OWN JSON parser. None of them ever run
# a real JSON document through Julia's own, independent JSON parser and
# hand that result to CToon's TOON writer. That conversion path (CToon's
# own _from_julia(), fed a JSON.jl-produced value instead of a hand-built
# one) is the Julia analogue of the struct-array / null-as-NaN /
# logical-array bugs found in the MATLAB binding's own type-conversion
# layer -- see tests/matlab/test_ctoon.m's "MATLAB-native jsondecode()
# round trips" section, and the Python/Go/Rust/Zig siblings of this file
# for the other bindings.
#
# JSON.jl's default dicttype is Base.Dict, which (like CToon.jl's own
# comment on why it uses OrderedDict warns) does not preserve key order --
# parsing with dicttype=OrderedDict below is not a style choice, it's
# required for the exact-TOON-text comparison against the paired .toon
# file to mean anything (an unordered Dict would make field order
# effectively random, and comparisons would flake rather than fail
# consistently).
#
# Expected TOON strings are copied verbatim from toon-format/spec's own
# tests/fixtures/encode/*.json -- never from running ctoon's own CLI or
# any of ctoon's own bindings against these inputs (that would only prove
# the implementation agrees with itself). Each case names its source file.
#
# NOTE: like tests/zig/native_json_roundtrip_test.zig, this file could not
# actually be run here (no Julia toolchain in this sandbox), only checked
# by hand against CToon.jl's own source and JSON.jl's documented API.

using Test
using CToon
using JSON
using OrderedCollections: OrderedDict

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

const LOCAL_DATA_DIR = normpath(joinpath(@__DIR__, "..", "data"))

"""
Path to toon-format/spec's examples/conversions/ dir, or `nothing` if
CTOON_SPEC_EXAMPLES_DIR isn't set/doesn't exist -- only available under
CMake/ctest, which fetches toon-format/spec centrally (see
tests/CMakeLists.txt). Same env-var convention as
tests/python/conftest.py's spec_examples_dir fixture.
"""
function spec_examples_dir()
    dir = get(ENV, "CTOON_SPEC_EXAMPLES_DIR", "")
    (isempty(dir) || !isdir(dir)) ? nothing : dir
end

"Every `<name>.json` in `dir` with a matching `<name>.toon`, sorted."
function pair_files(dir::AbstractString)
    names = String[]
    for f in readdir(dir)
        endswith(f, ".json") || continue
        stem = f[1:end-length(".json")]
        isfile(joinpath(dir, stem * ".toon")) && push!(names, stem)
    end
    sort(names)
end

"""
Numeric-tolerant equality between a JSON.jl value and a CToon.jl value
(rather than converting one to the other and using `==`, which would hide
exactly the kind of int/float mismatch this test exists to catch).
"""
function values_equal(native, own)
    if native isa Union{Int64,Int32,Integer} && own isa Union{Int64,UInt64}
        return native == own
    elseif native isa Real && own isa Real
        return Float64(native) == Float64(own)
    elseif native isa AbstractDict && own isa AbstractDict
        length(native) == length(own) || return false
        for (k, v) in native
            haskey(own, k) || return false
            values_equal(v, own[k]) || return false
        end
        return true
    elseif native isa AbstractVector && own isa AbstractVector
        length(native) == length(own) || return false
        return all(values_equal(a, b) for (a, b) in zip(native, own))
    elseif native === nothing || own === nothing
        return native === own
    else
        return native == own
    end
end

trim_trailing_newlines(s::AbstractString) = rstrip(s, '\n')

function check_pair(dir::AbstractString, name::AbstractString)
    raw_json = read(joinpath(dir, name * ".json"), String)

    native = JSON.parse(raw_json; dicttype=OrderedDict)
    own = CToon.parse(raw_json)
    @test values_equal(native, own)

    produced = CToon.dumps(native)
    expected = read(joinpath(dir, name * ".toon"), String)
    @test trim_trailing_newlines(produced) == trim_trailing_newlines(expected)

    round_tripped = CToon.parse(produced)
    @test values_equal(round_tripped, native)
end

# ---------------------------------------------------------------------------
# tests/data/*.json + *.toon — local to this repo, always available.
# ---------------------------------------------------------------------------

@testset "native JSON roundtrip: local data corpus" begin
    names = pair_files(LOCAL_DATA_DIR)
    @test !isempty(names)
    for name in names
        @testset "$name" begin
            check_pair(LOCAL_DATA_DIR, name)
        end
    end
end

# ---------------------------------------------------------------------------
# toon-format/spec's examples/conversions/*.json + *.toon.
# ---------------------------------------------------------------------------

@testset "native JSON roundtrip: spec examples corpus" begin
    dir = spec_examples_dir()
    if dir === nothing
        @test_skip "CTOON_SPEC_EXAMPLES_DIR not set/found — only available under CMake/ctest."
    else
        names = pair_files(dir)
        @test !isempty(names)
        for name in names
            @testset "$name" begin
                check_pair(dir, name)
            end
        end
    end
end
