using Test
using CToon

@testset "CToon.parse basics" begin
    @test CToon.parse("null") === nothing
    @test CToon.parse("true") === true
    @test CToon.parse("false") === false
    @test CToon.parse("42") == 42
    @test CToon.parse("3.5") == 3.5
    @test CToon.parse("\"hi\"") == "hi"
    @test CToon.parse("[1, 2, 3]") == Any[1, 2, 3]

    obj = CToon.parse("{\"a\": 1, \"b\": [true, null]}")
    @test obj isa Dict
    @test obj["a"] == 1
    @test obj["b"] == Any[true, nothing]
end
