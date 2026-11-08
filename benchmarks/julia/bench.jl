# CToon Julia benchmark: ctoon next to its one Julia peer, ToonFormat.jl
# (github.com/toon-format/ToonFormat.jl, the official implementation).
#
# ctoon works on JSON/TOON text directly (one call into the C core). ToonFormat.jl
# only converts native Julia values, so - exactly like toon_format in bench.py -
# its text operations go through JSON.jl: json_to_toon = encode(JSON.parse(text)),
# toon_to_json = JSON.json(decode(text)). Its two struct operations are the bare
# decode / encode, same as ctoon's parse_toon / dumps.
#
# Same operations as every other language benchmark here (ctoon's calls shown):
#  - json_to_toon: CToon.json_to_toon (JSON text -> TOON text, one call into
#    the C core; no Julia value is built).
#  - toon_to_json: CToon.toon_to_json (TOON text -> JSON text, same).
#  - roundtrip: json_to_toon's output fed through toon_to_json, as one
#    timed operation per file (matching how every other language here
#    defines "roundtrip" -- see e.g. bench.py's own comment on this).
#
# Plus the two text-free halves, measured against the language's own
# in-memory value (whatever CToon.parse / CToon.parse_toon return):
#  - toon_to_struct: CToon.parse_toon (TOON text -> native Julia value),
#    nothing else.
#  - struct_to_toon: CToon.dumps of a native value built UNTIMED in a
#    pre-pass (by CToon.parse -- there is no other JSON parser in this
#    benchmark's environment, and it is never inside the timed region).
#
# Methodology (same across every language benchmark in this repo):
#  1. Load every file in the corpus manifest into memory (untimed).
#  2. Time each operation over `REPEATS` passes over the whole corpus.
#  3. Report throughput (MB/s of bytes actually processed by successful
#     calls only) and documents/sec.
#
# If a log file path is given (3rd CLI arg), one line per failure is
# written there - only for the first repeat, not all `REPEATS` reps.
import CToon
import ToonFormat
import JSON
using OrderedCollections: OrderedDict

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

# One entry per library: the six calls every operation below is built from.
#   json_to_toon / toon_to_json   text -> text
#   parse / dumps                 JSON text -> native value / native value -> TOON text
#   parse_toon / to_json          TOON text -> native value / native value -> JSON text
# ToonFormat.jl has no text API, so JSON.jl fills the JSON side (timed, like toon_format in bench.py).
struct Lib
    name::String
    json_to_toon::Function
    toon_to_json::Function
    parse::Function
    dumps::Function
    parse_toon::Function
    to_json::Function
end

const LIBS = [
    Lib("ctoon", CToon.json_to_toon, CToon.toon_to_json,
        CToon.parse, CToon.dumps, CToon.parse_toon, CToon.to_json),
    # JSON.jl 0.21 (the version CToon.jl's own compat pins): `parse` builds an unordered Dict
    # unless told otherwise, and the indent is a positional argument of `json`.
    Lib("ToonFormat.jl",
        text -> ToonFormat.encode(JSON.parse(text; dicttype = OrderedDict{String,Any})),
        text -> JSON.json(ToonFormat.decode(text), 2),
        text -> JSON.parse(text; dicttype = OrderedDict{String,Any}),
        ToonFormat.encode, ToonFormat.decode, JSON.json),
]

# Minimal hand-rolled JSON writer for the fixed, small results shape
# below - not worth pulling in a whole JSON package (JSON.jl) just to
# write a handful of floats and strings.
json_escape(s::AbstractString) = replace(s, "\\" => "\\\\", "\"" => "\\\"")
json_num(x::Real) = isfinite(x) ? string(Float64(x)) : "0"

# ------------------------------------------------------------ order check --
# Key order, one direction at a time, against the shared samples in
# benchmarks/order_check/ (sample.json / sample.toon: non-alphabetical keys,
# a nested object, a tabular array):
#   json_to_toon -- keys of the TOON output must follow the order of sample.json
#   toon_to_json -- keys of the JSON output must follow the order of sample.toon
# Both reduce to "first-occurrence order of the keys", so the tabular header
# `list[2]{z,a}:` and the expanded `- z: 1` form compare equal.
# (\x22 is the double quote, so the regexes need no escaping.)
const ORDER_DIR = joinpath(@__DIR__, "..", "order_check")
const JSON_KEY_RE = r"\x22((?:[^\x22\\]|\\.)*)\x22\s*:"
const TOON_KEY_RE = r"^\s*(?:-\s+)*(\x22(?:[^\x22\\]|\\.)*\x22|[^\s:\[\]{}\x22,]+)(?:\[[^\]]*\])?(?:\{([^}]*)\})?:"

struct OrderRow
    library::String
    operation::String
    preserved::Bool
end

function unique_keys(keys)
    seen = String[]
    for k in keys
        k = String(strip(k))
        if length(k) >= 2 && startswith(k, '"') && endswith(k, '"')
            k = chop(k; head = 1, tail = 1)
        end
        if !isempty(k) && !(k in seen)
            push!(seen, k)
        end
    end
    return seen
end

json_keys(text) = unique_keys([String(m.captures[1]) for m in eachmatch(JSON_KEY_RE, text)])

function toon_keys(text)
    keys = String[]
    for line in split(text, '\n')
        m = match(TOON_KEY_RE, line)
        m === nothing && continue
        push!(keys, String(m.captures[1]))
        m.captures[2] === nothing || append!(keys, String.(split(m.captures[2], r"[,|\t]")))
    end
    return unique_keys(keys)
end

# One row per direction; `nothing` (the library has no such direction) adds no row.
function check_order(library, json_to_toon, toon_to_json)
    sample_json = read(joinpath(ORDER_DIR, "sample.json"), String)
    sample_toon = read(joinpath(ORDER_DIR, "sample.toon"), String)
    expected = json_keys(sample_json)
    rows = OrderRow[]
    for (op, fn, sample, extract) in (("json_to_toon", json_to_toon, sample_json, toon_keys),
                                      ("toon_to_json", toon_to_json, sample_toon, json_keys))
        fn === nothing && continue
        preserved = try
            extract(String(fn(sample))) == expected
        catch
            false   # any failure is a failed check
        end
        push!(rows, OrderRow(library, op, preserved))
    end
    return rows
end

function write_results_json(path, language, n_files, n_bytes, results, order_rows)
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
        println(io, "  ],")
        println(io, "  \"order_check\": [")
        for (i, r) in enumerate(order_rows)
            comma = i == length(order_rows) ? "" : ","
            println(io, "    {\"library\": \"$(json_escape(r.library))\", \"operation\": \"$(json_escape(r.operation))\", " *
                        "\"preserved\": $(r.preserved)}$comma")
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
        rpad(library, 14), " ", rpad(operation, 14), " ",
        lpad(round(throughput; digits=2), 9), " MB/s ",
        lpad(round(Int, ok / seconds), 14), " ",
        lpad(round(Int, success_rate * 100), 8), "%  ",
        lpad(round(seconds; digits=4), 8), " s  (x$REPEATS reps)",
    )
end

function bench_json_to_toon(lib, files, log_fail)
    ok = 0
    bytes = 0.0
    t0 = time()
    for r in 1:REPEATS
        for f in files
            try
                lib.json_to_toon(f.json)
                ok += 1
                bytes += sizeof(f.json)
            catch e
                r == 1 && log_fail(lib, "json_to_toon", f.path, e)
            end
        end
    end
    return time() - t0, ok, bytes
end

# `toons` is the shared decode input (see shared_toons): every library decodes
# the SAME ctoon-made TOON, like every other language benchmark here.
function bench_toon_to_json(lib, files, log_fail, toons)
    ok = 0
    bytes = 0.0
    t0 = time()
    for r in 1:REPEATS
        for (i, f) in enumerate(files)
            t = toons[i]
            t === nothing && continue
            try
                lib.toon_to_json(t)
                ok += 1
                bytes += sizeof(t)
            catch e
                r == 1 && log_fail(lib, "toon_to_json", f.path, e)
            end
        end
    end
    return time() - t0, ok, bytes
end

# Untimed. Each file's TOON text as ctoon writes it; `nothing` when ctoon could
# not handle the file (that file is then skipped by the decode operations).
function shared_toons(files)
    toons = Vector{Union{String,Nothing}}(undef, length(files))
    for (i, f) in enumerate(files)
        toons[i] = try
            CToon.dumps(CToon.parse(f.json))
        catch
            nothing
        end
    end
    return toons
end

# Untimed pre-pass for struct_to_toon: each file's native value as THIS
# library's own parser builds it; `has_native` is false where it failed.
function prepass(lib, files)
    natives = Vector{Any}(undef, length(files))
    has_native = falses(length(files))
    for (i, f) in enumerate(files)
        try
            natives[i] = lib.parse(f.json)
            has_native[i] = true
        catch
        end
    end
    return natives, has_native
end

function bench_toon_to_struct(lib, files, log_fail, toons)
    ok = 0
    bytes = 0.0
    t0 = time()
    for r in 1:REPEATS
        for (i, f) in enumerate(files)
            t = toons[i]
            t === nothing && continue
            try
                lib.parse_toon(t)
                ok += 1
                bytes += sizeof(t)
            catch e
                r == 1 && log_fail(lib, "toon_to_struct", f.path, e)
            end
        end
    end
    return time() - t0, ok, bytes
end

function bench_struct_to_toon(lib, files, log_fail, natives, has_native)
    ok = 0
    bytes = 0.0
    t0 = time()
    for r in 1:REPEATS
        for (i, f) in enumerate(files)
            has_native[i] || continue
            try
                lib.dumps(natives[i])
                ok += 1
                bytes += sizeof(f.json)
            catch e
                r == 1 && log_fail(lib, "struct_to_toon", f.path, e)
            end
        end
    end
    return time() - t0, ok, bytes
end

function bench_roundtrip(lib, files, log_fail)
    ok = 0
    bytes = 0.0
    t0 = time()
    for r in 1:REPEATS
        for f in files
            try
                toon = lib.json_to_toon(f.json)
                lib.toon_to_json(toon)
                ok += 1
                bytes += sizeof(f.json)
            catch e
                r == 1 && log_fail(lib, "roundtrip", f.path, e)
            end
        end
    end
    return time() - t0, ok, bytes
end

# Untimed. Writes what each operation actually produced for each file to
#   <results dir>/dump/julia/<library>/<operation>/<file index>.txt
# (0-based index; nothing for a file the library failed on) so
# report/verify_outputs.mjs can judge whether the output is CORRECT, not merely
# error-free -- see that script's header.
function dump_outputs(results_path, files, toons)
    lang_root = joinpath(dirname(abspath(results_path)), "dump", "julia")
    rm(lang_root; force=true, recursive=true)
    for lib in LIBS, op in ("json_to_toon", "toon_to_json", "roundtrip", "toon_to_struct", "struct_to_toon")
        mkpath(joinpath(lang_root, lib.name, op))
    end

    for lib in LIBS
        write_out(op, i, text) = write(joinpath(lang_root, lib.name, op, "$(i - 1).txt"), string(text))
        for (i, f) in enumerate(files)
            # json_to_toon, and the roundtrip decoding that very output
            try
                toon = lib.json_to_toon(f.json)
                write_out("json_to_toon", i, toon)
                write_out("roundtrip", i, lib.toon_to_json(toon))
            catch
            end
            # struct_to_toon: the same encode of the value the timed loop serialises
            try
                write_out("struct_to_toon", i, lib.dumps(lib.parse(f.json)))
            catch
            end
            toons[i] === nothing && continue
            # toon_to_json decodes the shared ctoon-made TOON, like the timed loop
            try
                write_out("toon_to_json", i, lib.toon_to_json(toons[i]))
            catch
            end
            # toon_to_struct: the decoded value serialised to JSON (untimed),
            # only so the verifier can compare it to the original document.
            try
                write_out("toon_to_struct", i, lib.to_json(lib.parse_toon(toons[i])))
            catch
            end
        end
    end
end

function main()
    if length(ARGS) < 2
        println(stderr, "usage: julia bench.jl <manifest> <results.json> [log_file]")
        exit(1)
    end
    manifest_path, results_path = ARGS[1], ARGS[2]
    log_path = length(ARGS) >= 3 ? ARGS[3] : ""
    log_io = isempty(log_path) ? nothing : open(log_path, "w")

    log_fail = (lib, op, path, err) -> (log_io === nothing || println(log_io, "[$(lib.name)] [$op] FILE: $path ERROR: $err"))

    files = load_corpus(manifest_path)
    if isempty(files)
        println(stderr, "Corpus manifest is empty or unreadable.")
        exit(1)
    end

    total_json_bytes = sum(sizeof(f.json) for f in files)

    println("CToon Benchmarks - Julia")
    println("Corpus: $(length(files)) files, $(round(total_json_bytes / 1e6; digits=2)) MB (JSON)\n")
    println(rpad("Library", 14), " ", rpad("Operation", 14), " ", lpad("Throughput", 12), " ",
            lpad("Docs/sec", 14), " ", lpad("Success", 9), "  ", lpad("Total time", 12))

    toons = shared_toons(files)

    # Outputs for report/verify_outputs.mjs live next to the results JSON.
    try
        dump_outputs(results_path, files, toons)
    catch e
        println(stderr, "warning: could not record outputs for verification: $e")
    end

    results = Result[]
    order_rows = OrderRow[]
    n = length(files) * REPEATS

    for lib in LIBS
        seconds, ok, bytes = bench_json_to_toon(lib, files, log_fail)
        record!(results, lib.name, "json_to_toon", seconds, ok, n, bytes)
        seconds, ok, bytes = bench_toon_to_json(lib, files, log_fail, toons)
        record!(results, lib.name, "toon_to_json", seconds, ok, n, bytes)
        seconds, ok, bytes = bench_roundtrip(lib, files, log_fail)
        record!(results, lib.name, "roundtrip", seconds, ok, n, bytes)

        seconds, ok, bytes = bench_toon_to_struct(lib, files, log_fail, toons)
        record!(results, lib.name, "toon_to_struct", seconds, ok, n, bytes)
        natives, has_native = prepass(lib, files)
        seconds, ok, bytes = bench_struct_to_toon(lib, files, log_fail, natives, has_native)
        record!(results, lib.name, "struct_to_toon", seconds, ok, n, bytes)

        try
            append!(order_rows, check_order(lib.name, lib.json_to_toon, lib.toon_to_json))
        catch e
            println(stderr, "warning: key-order check skipped for $(lib.name): $e")
        end
    end

    write_results_json(results_path, "julia", length(files), total_json_bytes, results, order_rows)
    println("\nResults written to $results_path")
    if log_io !== nothing
        println("Debug log written to $log_path")
        close(log_io)
    end
end

main()
