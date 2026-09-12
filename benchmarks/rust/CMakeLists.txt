# Both dependencies — ctoon included — are fetched by Cargo itself from
# their real GitHub/crates.io homes (see Cargo.toml), the same way every
# other language benchmark fetches its own dependencies. Built with
# `cargo build --release`; nothing else for this project to fetch here.
add_custom_target(ctoon_benchmarks_rust
    COMMAND ${CMAKE_COMMAND} -E echo "-- Building Rust benchmark, this fetches ctoon + toon-rust from GitHub/crates.io..."
    COMMAND ${CARGO_EXECUTABLE} build --release --quiet
    COMMAND ${CMAKE_COMMAND} -E echo "-- Running Rust benchmark..."
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/target/release/ctoon-bench-rust
            ${CTOON_BENCH_MANIFEST} ${CTOON_BENCH_RESULTS_DIR}/rust.json
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    USES_TERMINAL
)
add_dependencies(ctoon_benchmarks ctoon_benchmarks_rust)
