set(CTOON_TEST_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(CTOON_TEST_BINARY_DIR ${CMAKE_BINARY_DIR}/tests)
set(CTOON_COVERAGE_BINARY_DIR ${CMAKE_BINARY_DIR}/coverage)

set(CTOON_COVERAGE_TOTAL_BINARY_DIR ${CTOON_COVERAGE_BINARY_DIR}/total CACHE PATH "Total coverage output directory")
set(CTOON_COVERAGE_C_BINARY_DIR ${CTOON_COVERAGE_BINARY_DIR}/c CACHE PATH "C coverage output directory")
set(CTOON_COVERAGE_CPP_BINARY_DIR ${CTOON_COVERAGE_BINARY_DIR}/cpp CACHE PATH "C++ coverage output directory")
set(CTOON_COVERAGE_PYTHON_BINARY_DIR ${CTOON_COVERAGE_BINARY_DIR}/python CACHE PATH "Python coverage output directory")
set(CTOON_COVERAGE_GO_BINARY_DIR ${CTOON_COVERAGE_BINARY_DIR}/go CACHE PATH "Go coverage output directory")
set(CTOON_COVERAGE_RUST_BINARY_DIR ${CTOON_COVERAGE_BINARY_DIR}/rust CACHE PATH "Rust coverage output directory")
set(CTOON_COVERAGE_MATLAB_BINARY_DIR ${CTOON_COVERAGE_BINARY_DIR}/matlab CACHE PATH "MATLAB MEX coverage output directory")
set(CTOON_COVERAGE_JULIA_BINARY_DIR ${CTOON_COVERAGE_BINARY_DIR}/julia CACHE PATH "Julia coverage output directory")

file(REMOVE_RECURSE ${CTOON_COVERAGE_BINARY_DIR})
file(MAKE_DIRECTORY ${CTOON_COVERAGE_BINARY_DIR})

add_custom_target(ctoon_test
    COMMAND ${CMAKE_COMMAND} -E echo "-- Running All tests..."
)

add_custom_target(ctoon_coverage
    COMMAND ${CMAKE_COMMAND} -E echo "-- Running coverage for all languages ..."
)


find_package(Python3 COMPONENTS Interpreter REQUIRED)
find_program(LCOV_EXECUTABLE lcov)
find_program(GENHTML_EXECUTABLE genhtml)

# toon-format/spec isn't part of this project — its test fixtures are only
# used (read-only) to check each language binding's encode/decode output
# against the spec. Fetched ONCE here, centrally, so every language under
# tests/ (not just C++ and Python, which is all that consumed it before)
# gets the same CTOON_SPEC_FIXTURES_DIR. Pinned to a specific release tag
# (auto-resolved to the latest one, see CTOON_SPEC_TAG below) for
# reproducible test runs; shallow clone since only the fixture JSON files
# are needed, not history.
#
# CTOON_SPEC_FIXTURES_DIR is intentionally allowed to end up unset (e.g. no
# network at configure time): every consumer below must check it and skip
# its spec-conformance tests, not fail the whole suite, when it's empty.
#
# CTOON_SPEC_TAG comes from the root CMakeLists.txt (cmake/SpecVersion.cmake,
# resolved off main's package.json/CHANGELOG.md) and is inherited here via
# add_subdirectory(). It's a real published git tag (e.g. v4.1.1), so this
# is still a fully reproducible clone - no manually-maintained commit hash
# to keep in sync anymore, and it auto-tracks whatever was last released.
include(FetchContent)
FetchContent_Declare(
    toon_format_spec
    GIT_REPOSITORY https://github.com/toon-format/spec.git
    GIT_TAG        ${CTOON_SPEC_TAG}
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)
FetchContent_MakeAvailable(toon_format_spec)
# toon_format_spec has no CMakeLists.txt of its own, so MakeAvailable only
# populates toon_format_spec_SOURCE_DIR and does not add_subdirectory() it.
set(CTOON_SPEC_FIXTURES_DIR "${toon_format_spec_SOURCE_DIR}/tests/fixtures"
    CACHE PATH "Absolute path to toon-format/spec's tests/fixtures/ (empty if the fetch failed/was skipped)")

# Separate from tests/fixtures/{encode,decode}/*.json above (those are test
# *manifests* — {"tests":[{"input","expected",...}]}, not real documents).
# examples/conversions/ instead holds genuine paired <name>.json / <name>.toon
# files — the same shape as this project's own tests/data/*.json+*.toon
# pairs, just spec-sourced. Good for plain "does language X's native JSON
# parser + ctoon's TOON writer agree with the spec" round-trip corpus tests.
set(CTOON_SPEC_EXAMPLES_DIR "${toon_format_spec_SOURCE_DIR}/examples/conversions"
    CACHE PATH "Absolute path to toon-format/spec's examples/conversions/ paired .json/.toon files (empty if the fetch failed/was skipped)")

add_subdirectory(${CTOON_TEST_SOURCE_DIR}/c)
add_subdirectory(${CTOON_TEST_SOURCE_DIR}/cpp)
add_subdirectory(${CTOON_TEST_SOURCE_DIR}/python)
add_subdirectory(${CTOON_TEST_SOURCE_DIR}/go)
add_subdirectory(${CTOON_TEST_SOURCE_DIR}/rust)
add_subdirectory(${CTOON_TEST_SOURCE_DIR}/zig)
add_subdirectory(${CTOON_TEST_SOURCE_DIR}/matlab)
add_subdirectory(${CTOON_TEST_SOURCE_DIR}/julia)

if(TARGET ctoon_coverage_c AND TARGET ctoon_coverage_cpp AND TARGET ctoon_coverage_python)
    if(LCOV_EXECUTABLE AND GENHTML_EXECUTABLE)

        # Build the list of --add-tracefile arguments dynamically so Go and
        # MATLAB are included when their coverage targets are available.
        set(LCOV_MERGE_ARGS
            --add-tracefile ${CTOON_COVERAGE_C_BINARY_DIR}/coverage.lcov
            --add-tracefile ${CTOON_COVERAGE_CPP_BINARY_DIR}/coverage.lcov
            --add-tracefile ${CTOON_COVERAGE_PYTHON_BINARY_DIR}/coverage.lcov
        )
        set(LCOV_MERGE_DEPS ctoon_coverage_c ctoon_coverage_cpp ctoon_coverage_python)
        set(LCOV_MERGE_ECHO "-- Merging C, C++ and Python coverage...")

        if(TARGET ctoon_coverage_go)
            list(APPEND LCOV_MERGE_ARGS
                --add-tracefile ${CTOON_COVERAGE_GO_BINARY_DIR}/coverage.lcov
            )
            list(APPEND LCOV_MERGE_DEPS ctoon_coverage_go)
            set(LCOV_MERGE_ECHO "-- Merging C, C++, Python and Go coverage...")
        endif()

        if(TARGET ctoon_coverage_rust)
            list(APPEND LCOV_MERGE_ARGS
                --add-tracefile ${CTOON_COVERAGE_RUST_BINARY_DIR}/coverage.lcov
            )
            list(APPEND LCOV_MERGE_DEPS ctoon_coverage_rust)
            set(LCOV_MERGE_ECHO "-- Merging C, C++, Python, Go and Rust coverage...")
        endif()

        if(TARGET ctoon_coverage_julia)
            list(APPEND LCOV_MERGE_ARGS
                --add-tracefile ${CTOON_COVERAGE_JULIA_BINARY_DIR}/coverage.lcov
            )
            list(APPEND LCOV_MERGE_DEPS ctoon_coverage_julia)
            set(LCOV_MERGE_ECHO "-- Merging C, C++, Python, Go, Rust and Julia coverage...")
        endif()

        # NOTE: Zig coverage isn't produced at all yet — Zig's own native
        # coverage tooling is still too immature/unstable to depend on
        # (see tests/zig/CMakeLists.txt's comment where ctoon_coverage_zig
        # would go). MATLAB coverage is produced by TestTask (Cobertura XML) via
        # buildtool, not by lcov. It is therefore not merged into the lcov
        # total but is still shown on the dashboard (see below).

        add_custom_target(ctoon_coverage_total
            # Clean previous coverage data
            COMMAND ${CMAKE_COMMAND} -E remove_directory ${CTOON_COVERAGE_TOTAL_BINARY_DIR}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CTOON_COVERAGE_TOTAL_BINARY_DIR}/html
            # Merge all coverage info files
            COMMAND ${CMAKE_COMMAND} -E echo ${LCOV_MERGE_ECHO}
            COMMAND ${LCOV_EXECUTABLE}
                    ${LCOV_MERGE_ARGS}
                    --output-file ${CTOON_COVERAGE_TOTAL_BINARY_DIR}/coverage.lcov
            COMMAND ${CMAKE_COMMAND} -E echo "-- Total coverage info created at: ${CTOON_COVERAGE_TOTAL_BINARY_DIR}/coverage.lcov"

            # Generate HTML report
            COMMAND ${CMAKE_COMMAND}
                -DCOV_FILE=${CTOON_COVERAGE_TOTAL_BINARY_DIR}/coverage.lcov
                -DPREFIX_PATH=${PROJECT_SOURCE_DIR}/
                -P ${PROJECT_SOURCE_DIR}/cmake/FixLCovPaths.cmake
            COMMAND ${CMAKE_COMMAND} -E echo "-- Generating HTML report..."
            COMMAND ${GENHTML_EXECUTABLE} --quiet
                    ${CTOON_COVERAGE_TOTAL_BINARY_DIR}/coverage.lcov
                    --output-directory ${CTOON_COVERAGE_TOTAL_BINARY_DIR}/html
                    --prefix ${PROJECT_SOURCE_DIR}
                    --title "CToon Total Coverage"
                    --css-file ${CTOON_TEST_SOURCE_DIR}/genhtml.css
                    --dark-mode
            COMMAND ${CMAKE_COMMAND}
                -DCOV_FILE=${CTOON_COVERAGE_TOTAL_BINARY_DIR}/coverage.lcov
                -DSOURCE_PATH=${PROJECT_SOURCE_DIR}/
                -P ${PROJECT_SOURCE_DIR}/cmake/FixLCovPaths.cmake
            COMMAND ${CMAKE_COMMAND} -E echo "-- HTML entry point created here: ${CTOON_COVERAGE_TOTAL_BINARY_DIR}/html/index.html"

            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
            USES_TERMINAL
        )

        foreach(_dep IN LISTS LCOV_MERGE_DEPS)
            add_dependencies(ctoon_coverage_total ${_dep})
        endforeach()

        add_dependencies(ctoon_coverage ctoon_coverage_total)

        add_custom_command(TARGET ctoon_coverage_total POST_BUILD
            COMMAND ${CMAKE_COMMAND}
                -DIFRAME_SRC="html/index.html"
                -DSECTION_TITLE="Total%20Coverage"
                -DLOGO_SVG="${PROJECT_SOURCE_DIR}/docs/images/ctoon-sq.svg"
                -DPROJECT_NAME="${PROJECT_NAME}"
                -DOUTPUT_DIR="${CTOON_COVERAGE_TOTAL_BINARY_DIR}"
                -P "${PROJECT_SOURCE_DIR}/cmake/GenerateIframe.cmake"
            COMMENT "Generating iframe wrapper for total coverage report"
            WORKING_DIRECTORY ${CTOON_TEST_SOURCE_DIR}
        )
    endif()
endif()

# --- Generate Coverage Dashboard HTML ---
set(COVERAGE_DATA_JSON "[")

if(TARGET ctoon_coverage_total)
    set(COVERAGE_DATA_JSON "${COVERAGE_DATA_JSON}
        {
            \"id\": \"total\",
            \"title\": \"Total Coverage\",
            \"badge\": \"Merged\",
            \"badgeClass\": \"total\",
            \"color\": \"#a371f7\",
            \"desc\": \"Combined coverage report for C, C++, Python, Go and Rust codebases. Useful for SonarQube analysis.\",
            \"htmlLink\": \"total/index.html\",
            \"lcovLink\": \"total/coverage.lcov\"
        },")
endif()

if(TARGET ctoon_coverage_c)
    set(COVERAGE_DATA_JSON "${COVERAGE_DATA_JSON}
        {
            \"id\": \"c\",
            \"title\": \"C Core\",
            \"badge\": \"Native\",
            \"color\": \"#2ecc71\",
            \"desc\": \"Detailed coverage report for the core C library, including headers and implementation files generated by LCOV.\",
            \"htmlLink\": \"c/index.html\",
            \"lcovLink\": \"c/coverage.lcov\"
        },")
endif()

if(TARGET ctoon_coverage_cpp)
    set(COVERAGE_DATA_JSON "${COVERAGE_DATA_JSON}
        {
            \"id\": \"cpp\",
            \"title\": \"C++ Core\",
            \"badge\": \"Native\",
            \"color\": \"#f0883e\",
            \"desc\": \"Detailed coverage report for the core C++ library, including headers and implementation files generated by LCOV.\",
            \"htmlLink\": \"cpp/index.html\",
            \"lcovLink\": \"cpp/coverage.lcov\"
        },")
endif()

if(TARGET ctoon_coverage_python)
    set(COVERAGE_DATA_JSON "${COVERAGE_DATA_JSON}
        {
            \"id\": \"py\",
            \"title\": \"Python Bindings\",
            \"badge\": \"Interpreter\",
            \"color\": \"#3572A5\",
            \"desc\": \"Coverage analysis for Python bindings and high-level logic, generated by coverage.py.\",
            \"htmlLink\": \"python/index.html\",
            \"lcovLink\": \"python/coverage.lcov\"
        },")
endif()

if(TARGET ctoon_coverage_go)
    set(COVERAGE_DATA_JSON "${COVERAGE_DATA_JSON}
        {
            \"id\": \"go\",
            \"title\": \"Go Bindings\",
            \"badge\": \"Interpreter\",
            \"color\": \"#00ADD8\",
            \"desc\": \"Coverage analysis for Go bindings generated by go tool cover, converted to lcov for unified reporting.\",
            \"htmlLink\": \"go/index.html\",
            \"lcovLink\": \"go/coverage.lcov\"
        },")
endif()

if(TARGET ctoon_coverage_rust)
    set(COVERAGE_DATA_JSON "${COVERAGE_DATA_JSON}
        {
            \"id\": \"rust\",
            \"title\": \"Rust Bindings\",
            \"badge\": \"LLVM\",
            \"color\": \"#dea584\",
            \"desc\": \"Coverage analysis for the Rust binding generated by cargo-llvm-cov, natively in lcov format.\",
            \"htmlLink\": \"rust/index.html\",
            \"lcovLink\": \"rust/coverage.lcov\"
        },")
endif()

if(TARGET ctoon_coverage_julia)
    set(COVERAGE_DATA_JSON "${COVERAGE_DATA_JSON}
        {
            \"id\": \"julia\",
            \"title\": \"Julia Bindings\",
            \"badge\": \"Coverage.jl\",
            \"color\": \"#9558B2\",
            \"desc\": \"Coverage analysis for the Julia binding generated by Coverage.jl, natively in lcov format.\",
            \"htmlLink\": \"julia/index.html\",
            \"lcovLink\": \"julia/coverage.lcov\"
        },")
endif()

if(TARGET ctoon_coverage_matlab)
    set(COVERAGE_DATA_JSON "${COVERAGE_DATA_JSON}
        {
            \"id\": \"matlab\",
            \"title\": \"MATLAB MEX\",
            \"badge\": \"MEX\",
            \"color\": \"#e16737\",
            \"desc\": \"Coverage for the MATLAB MEX binding, collected by the MATLAB Build Tool (TestTask) and reported in Cobertura XML format.\",
            \"htmlLink\": \"../src/bindings/matlab/coverage/html/index.html\",
            \"coberturaLink\": \"../src/bindings/matlab/coverage/cobertura.xml\"
        },")
endif()

set(COVERAGE_DATA_JSON "${COVERAGE_DATA_JSON}]")

set(CTOON_LOGO_SVG "${PROJECT_SOURCE_DIR}/docs/images/ctoon-sq.svg")
if(EXISTS "${CTOON_LOGO_SVG}")
    file(READ "${CTOON_LOGO_SVG}" LOGO_CONTENT)
else()
    set(LOGO_CONTENT "")
endif()

configure_file(
    ${CTOON_TEST_SOURCE_DIR}/coverage_dashboard.html.in
    ${CTOON_COVERAGE_BINARY_DIR}/index.html
    @ONLY
)