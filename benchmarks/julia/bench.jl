# CToon Julia benchmark.
#
# Unlike every other language folder here, there is no peer library to
# test alongside ctoon: a search of Julia's General registry turned up
# no TOON implementation for Julia at the time this was written. So
# this is a one-library table, same as benchmarks/matlab's.
#
# It's also a *smaller* table than the other languages': this repo's
# Julia binding (src/bindings/julia — fetched here the same way any
# real dependent would, via `Pkg.add(url=..., subdir=...)`) only wraps
# ctoon's reader (CToon.parse). There's no encode/dumps support yet, so
# unlike every other language benchmark here, this one cannot measure
# json_to_toon, toon_to_json, or roundtrip — only decode. The single
# operation reported, "json_parse", corresponds to the decode half of
# what the other benchmarks call "toon_to_json" (parsing — here, JSON
# text straight through ctoon's C reader via ctoon_read_json — rather
# than producing TOON text at all).
#
# Methodology for the parts that DO apply (same across every language
# benchmark in this repo):
#  1. Load every file in the corpus manifest into memory (untimed).
#  2. Timed "json_parse": repeatedly parse each file's JSON text into a
#     native Julia value via CToon.parse.
#  3. Report throughput (MB/s of bytes actually read by successful
#     parses only) and documents/sec.
#
# If a log file path is given (3rd CLI arg), one line per failure is
# written there — only for the first repeat, not all `repeats` reps.
using CToon

const REPEATS = 20

struct BenchFile
    path::String
    json::String
end

function load_corpus(manifest_path::AbstractString)
    files = BenchFile[]
    for line in eachline(manifest_path)
        isempty(line) && continue
        try
            push!(files, BenchFile(line, read(line, String)))
        catch
            continue
        end
    end
    return files
end

# Minimal hand-rolled JSON writer for the fixed, small results shape
# below — not worth pulling in a whole JSON package (JSON.jl) just to
# write a handful of floats and strings.
json_escape(s::AbstractString) = replace(s, "\\" => "\\\\", "\"" => "\\\"")
json_num(x::Real) = isfinite(x) ? string(Float64(x)) : "0"

function write_results_json(path, language, n_files, n_bytes, results)
    open(path, "w") do io
        println(io, "{")
        println(io, "  \"language\": \"$(json_escape(language))\",")
        println(io, "  \"corpus\": { \"files\": $n_files, \"bytes\": $n_bytes },")
        println(io, "  \"results\": [")
        for (i, r) in enumerate(results)
            comma = i == length(results) ? "" : ","
            println(io, "    {")
            println(io, "      \"library\": \"$(json_escape(r.library))\",")
            println(io, "      \"operation\": \"$(json_escape(r.operation))\",")
            println(io, "      \"throughput_mb_s\": $(json_num(r.throughput_mb_s)),")
            println(io, "      \"docs_per_sec\": $(json_num(r.docs_per_sec)),")
            println(io, "      \"success_rate\": $(json_num(r.success_rate)),")
            println(io, "      \"total_time_s\": $(json_num(r.total_time_s))")
            println(io, "    }$comma")
        end
        println(io, "  ]")
        println(io, "}")
    end
end

struct Result
    library::String
    operation::String
    throughput_mb_s::Float64
    docs_per_sec::Float64
    success_rate::Float64
    total_time_s::Float64
end

function record!(results, library, operation, seconds, ok, attempted, bytes)
    throughput = ok > 0 ? bytes / seconds / 1e6 : 0.0
    success_rate = attempted > 0 ? ok / attempted : 0.0
    push!(results, Result(library, operation, throughput, ok / seconds, success_rate, seconds))
    println(
        rpad(library, 8), " ", rpad(operation, 14), " ",
        lpad(round(throughput; digits=2), 9), " MB/s ",
        lpad(round(Int, ok / seconds), 14), " ",
        lpad(round(Int, success_rate * 100), 8), "%  ",
        lpad(round(seconds; digits=4), 8), " s  (x$REPEATS reps)",
    )
end

function main()
    if length(ARGS) < 2
        println(stderr, "usage: julia bench.jl <manifest> <results.json> [log_file]")
        exit(1)
    end
    manifest_path, results_path = ARGS[1], ARGS[2]
    log_path = length(ARGS) >= 3 ? ARGS[3] : ""
    log_io = isempty(log_path) ? nothing : open(log_path, "w")

    log_fail(op, path, err) = log_io === nothing || println(log_io, "[ctoon] [$op] FILE: $path ERROR: $err")

    files = load_corpus(manifest_path)
    if isempty(files)
        println(stderr, "Corpus manifest is empty or unreadable.")
        exit(1)
    end

    total_json_bytes = sum(sizeof(f.json) for f in files)

    println("CToon Benchmarks — Julia")
    println("Corpus: $(length(files)) files, $(round(total_json_bytes / 1e6; digits=2)) MB (JSON)\n")
    println(rpad("Library", 8), " ", rpad("Operation", 14), " ", lpad("Throughput", 12), " ",
            lpad("Docs/sec", 14), " ", lpad("Success", 9), "  ", lpad("Total time", 12))

    results = Result[]

    ok = 0
    bytes = 0.0
    t0 = time()
    for r in 1:REPEATS
        for f in files
            try
                CToon.parse(f.json)
                ok += 1
                bytes += sizeof(f.json)
            catch e
                r == 1 && log_fail("json_parse", f.path, e)
            end
        end
    end
    elapsed = time() - t0
    record!(results, "ctoon", "json_parse", elapsed, ok, length(files) * REPEATS, bytes)

    write_results_json(results_path, "julia", length(files), total_json_bytes, results)
    println("\nResults written to $results_path")
    if log_io !== nothing
        println("Debug log written to $log_path")
        close(log_io)
    end
end

main()
