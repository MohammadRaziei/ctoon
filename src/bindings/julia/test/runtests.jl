using Test
using CToon
using OrderedCollections: OrderedDict

@testset "CToon.parse basics" begin
    @test CToon.parse("null") === nothing
    @test CToon.parse("true") === true
    @test CToon.parse("false") === false
    @test CToon.parse("42") == 42
    @test CToon.parse("3.5") == 3.5
    @test CToon.parse("\"hi\"") == "hi"
    @test CToon.parse("[1, 2, 3]") == Any[1, 2, 3]

    obj = CToon.parse("{\"a\": 1, \"b\": [true, null]}")
    @test obj isa OrderedDict # not Base.Dict -- see CToon.jl's own comment on why
    @test obj["a"] == 1
    @test obj["b"] == Any[true, nothing]
end

@testset "CToon.parse preserves key order" begin
    # Deliberately non-alphabetical, so a Base.Dict (which does not
    # preserve insertion order) would fail this -- same fixed-sample
    # idea benchmarks/c/bench.c's run_order_checks() uses.
    obj = CToon.parse("{\"zebra\": 1, \"apple\": 2, \"mango\": 3}")
    @test collect(keys(obj)) == ["zebra", "apple", "mango"]
end

@testset "CToon.dumps / CToon.to_json basics" begin
    @test CToon.dumps(nothing) == "null"
    @test CToon.dumps(true) == "true"
    @test CToon.dumps(42) == "42"
    # dumps() emits TOON, not JSON -- a bare scalar string needs no
    # quoting in TOON (same as tests/matlab/test_ctoon.m's testDecodeString
    # comment: "Bare string (no quotes needed in TOON)"). Confirmed against
    # the C CLI directly: `"hi"` (JSON) -> `hi` (TOON), no quote chars.
    @test CToon.dumps("hi") == "hi"

    @test CToon.to_json(nothing) == "null"
    @test CToon.to_json(true) == "true"
    @test CToon.to_json(42) == "42"
end

@testset "CToon.parse -> CToon.dumps -> CToon.parse_toon round trip" begin
    original = "{\"zebra\": 1, \"apple\": 2, \"nested\": {\"a\": true, \"b\": null}}"
    value = CToon.parse(original)
    toon = CToon.dumps(value)
    back = CToon.parse_toon(toon)
    @test collect(keys(back)) == collect(keys(value)) # order survives the round trip
    @test back["zebra"] == 1
    @test back["apple"] == 2
    @test back["nested"]["a"] == true
    @test back["nested"]["b"] === nothing
end

@testset "CToon.parse_toon -> CToon.to_json round trip" begin
    value = CToon.parse("{\"a\": [1, 2, 3], \"b\": \"text\"}")
    toon = CToon.dumps(value)
    json = CToon.to_json(CToon.parse_toon(toon))
    @test CToon.parse(json) == value
end
