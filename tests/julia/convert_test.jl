# CToon.json_to_toon / toon_to_json (+ _file): direct text -> text conversion.
# Expected values come only from independent sources: hand-written strings,
# paired .toon fixtures, and JSON.jl (never the ctoon CLI). Run as an extra
# include() after test/runtests.jl, same as native_json_roundtrip_test.jl.

using Test
using CToon
using JSON

const _LOCAL = normpath(joinpath(@__DIR__, "..", "data"))

function _pairs(dir)
    names = String[]
    for f in readdir(dir)
        endswith(f, ".json") || continue
        stem = f[1:end-5]
        isfile(joinpath(dir, stem * ".toon")) && push!(names, stem)
    end
    sort(names)
end

# Numbers compared by value: TOON canonicalises 15.0 -> 15.
_same(a::Number, b::Number) = a == b
_same(a::AbstractDict, b::AbstractDict) =
    length(a) == length(b) && all(haskey(b, k) && _same(v, b[k]) for (k, v) in a)
_same(a::AbstractVector, b::AbstractVector) =
    length(a) == length(b) && all(_same(x, y) for (x, y) in zip(a, b))
_same(a, b) = a == b

@testset "CToon.json_to_toon / toon_to_json basics" begin
    @test CToon.json_to_toon("{\"name\":\"Alice\",\"age\":30,\"tags\":[\"a\",\"b\"]}") ==
          "name: Alice\nage: 30\ntags[2]: a,b"
    @test CToon.json_to_toon("{\"u\":[{\"id\":1,\"n\":\"x\"},{\"id\":2,\"n\":\"y\"}]}") ==
          "u[2]{id,n}:\n  1,x\n  2,y"
    @test CToon.json_to_toon("{\"t\":[1,2,3]}"; delimiter = CToon.DELIMITERS.pipe) == "t[3|]: 1|2|3"
    @test CToon.json_to_toon(SubString("xx{\"a\":1}yy", 3, 9)) == "a: 1"

    @test CToon.toon_to_json("name: Alice\nage: 30\ntags[2]: a,b") ==
          "{\"name\":\"Alice\",\"age\":30,\"tags\":[\"a\",\"b\"]}"
    @test CToon.toon_to_json("a: 1"; indent = 2) == "{\n  \"a\": 1\n}"
end

@testset "CToon conversion errors" begin
    @test_throws ErrorException CToon.json_to_toon("{\"a\":")
    @test_throws ErrorException CToon.json_to_toon("")
    @test_throws ErrorException CToon.toon_to_json("a[2]: 1")
end

@testset "CToon conversion files" begin
    mktempdir() do dir
        j = joinpath(dir, "in.json"); t = joinpath(dir, "out.toon"); b = joinpath(dir, "back.json")
        write(j, "{\"k\":[1,2],\"s\":\"hi\"}")
        CToon.json_to_toon_file(j, t)
        @test read(t, String) == "k[2]: 1,2\ns: hi"
        CToon.toon_to_json_file(t, b)
        @test read(b, String) == "{\"k\":[1,2],\"s\":\"hi\"}"

        bad = joinpath(dir, "bad.json"); never = joinpath(dir, "never.toon")
        write(bad, "{\"a\":")
        @test_throws ErrorException CToon.json_to_toon_file(bad, never)
        @test !isfile(never)
    end
end

function _check_convert_dir(dir)
    for name in _pairs(dir)
        raw = read(joinpath(dir, name * ".json"), String)
        expected = rstrip(read(joinpath(dir, name * ".toon"), String), '\n')
        @test rstrip(CToon.json_to_toon(raw), '\n') == expected
        @test _same(JSON.parse(raw), JSON.parse(CToon.toon_to_json(expected)))
    end
end

@testset "CToon conversion: local corpus" begin
    @test !isempty(_pairs(_LOCAL))
    _check_convert_dir(_LOCAL)
end

@testset "CToon conversion: spec examples corpus" begin
    dir = get(ENV, "CTOON_SPEC_EXAMPLES_DIR", "")
    if isempty(dir) || !isdir(dir)
        @info "CTOON_SPEC_EXAMPLES_DIR not set -- skipping spec examples"
    else
        @test !isempty(_pairs(dir))
        _check_convert_dir(dir)
    end
end
