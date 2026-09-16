# Tiny driver script so CreateDocs.cmake never has to pass a `-e`
# string containing a semicolon to julia through CMake's Unix Makefiles
# generator — see tests/julia/run_tests.jl's comment for the exact
# quoting pitfall this sidesteps.
import Pkg
Pkg.instantiate()
