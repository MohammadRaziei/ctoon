# Driver script for folding Julia's raw --code-coverage .cov files into
# one lcov report — run as `julia --project=<binding dir>
# run_coverage_report.jl <output.lcov> <binding dir>` after run_tests.jl
# has already run with --code-coverage=user. Kept as a real .jl file for
# the same reason as run_tests.jl (see that file's comment) — avoids
# CMake -e string quoting/escaping pitfalls entirely.
#
# process_folder() is given an *absolute* path deliberately: every other
# language's coverage output here (gcov, cargo-llvm-cov, go tool cover)
# records absolute source paths, which FixLCovPaths.cmake then strips
# down to repo-relative by matching on PROJECT_SOURCE_DIR (see that
# script). A relative "src" folder argument would instead record paths
# relative to whatever the working directory happened to be (the binding
# dir) — missing the "bindings/julia/" prefix FixLCovPaths.cmake expects
# — which is exactly what broke genhtml here before this fix.
using Coverage

output_path = ARGS[1]
binding_dir = ARGS[2]

cov = process_folder(joinpath(binding_dir, "src"))
LCOV.writefile(output_path, cov)
clean_folder(joinpath(binding_dir, "src"))
