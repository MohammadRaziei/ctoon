# toon-rust (fetched via Cargo's own git dependency, in Cargo.toml) is
# built with `cargo build --release`; no ctoon Rust binding exists yet
# (see main.rs), so there's nothing else for this project to fetch here.
add_custom_target(ctoon_benchmarks_rust
    COMMAND ${CMAKE_COMMAND} -E echo "-- Building Rust benchmark, this fetches toon-rust from GitHub..."
    COMMAND ${CARGO_EXECUTABLE} build --release --quiet
    COMMAND ${CMAKE_COMMAND} -E echo "-- Running Rust benchmark..."
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/target/release/ctoon-bench-rust
            ${CTOON_BENCH_MANIFEST} ${CTOON_BENCH_RESULTS_DIR}/rust.json
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    USES_TERMINAL
)
add_dependencies(ctoon_benchmarks ctoon_benchmarks_rust)
