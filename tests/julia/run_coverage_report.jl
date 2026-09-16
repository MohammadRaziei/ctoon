# Driver script for folding Julia's raw --code-coverage .cov files into
# one lcov report — run as `julia --project=<binding dir>
# run_coverage_report.jl <output.lcov>` after run_tests.jl has already
# run with --code-coverage=user. Kept as a real .jl file for the same
# reason as run_tests.jl (see that file's comment) — avoids CMake -e
# string quoting/escaping pitfalls entirely.
using Coverage

output_path = ARGS[1]
cov = process_folder("src")
LCOV.writefile(output_path, cov)
clean_folder("src")
