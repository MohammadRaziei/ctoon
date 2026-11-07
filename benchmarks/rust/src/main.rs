// CToon Benchmarks - Rust.
//
// Every implementation here - ctoon included - is one more peer entry,
// fetched from its own GitHub/crates.io home the same way, tested the
// same way. No "ctoon vs X" framing: one shared results table, one row
// per (library, operation) pair. Same methodology, corpus, and JSON
// output schema as every other language benchmark here - see
// benchmarks/go/bench.go for the reference version of this comment.
//
// Implementations:
//   - ctoon      this project (github.com/mohammadraziei/ctoon), fetched
//                as a git dependency in Cargo.toml - not a local path,
//                even though this repo's root is right above us; see
//                this directory's CMakeLists.txt for why.
//   - toon-rust  github.com/toon-format/toon-rust (official)
//
// Methodology (same across every language benchmark in this repo):
//   1. Load every file in the corpus manifest into memory (untimed).
//   2. Untimed pre-pass: convert each JSON file to TOON once (with
//      ctoon) - the shared decode input every library's toon_to_json
//      leg reads from.
//   3. Timed "json_to_toon": repeatedly parse JSON and re-serialise to
//      TOON, with each library's own encoder.
//   4. Timed "toon_to_json": repeatedly parse the TOON text from step 2
//      and re-serialise to JSON. Measures interop with ctoon's specific
//      output.
//   5. Timed "roundtrip": json -> toon -> parse toon -> json, all
//      chained as one operation using each library's own encoder/decoder
//      together. Measures self-consistency, not interop - see the
//      top-level README for why a library's toon_to_json and roundtrip
//      success rates can legitimately differ a lot.
//   6. Timed "toon_to_struct" / "struct_to_toon": the same two directions
//      with the text format taken out -- decode the pre-pass TOON into the
//      library's in-memory value and stop; encode an in-memory value built
//      untimed before the clock starts. ctoon is fed the Value ITS OWN
//      loads_json produced (its native structure); toon-rust is fed the
//      serde_json::Value serde_json produced (its native structure).
//   7. Report throughput (MB/s of bytes actually read by successful
//      conversions only) and documents/sec.

use serde_json::Value as JsonValue;
use std::fs;
use std::io::{BufRead, Write};
use std::time::Instant;
use toon_format::{decode_default, encode_default};

const REPEATS: u32 = 20;

struct BenchFile {
    json: String,
    toon: Option<String>,
    // In-memory values for struct_to_toon, built UNTIMED in the pre-pass.
    value: Option<ctoon::Value>,   // ctoon's own native value (ctoon::loads_json)
    plain: Option<JsonValue>,      // serde_json's value, fed to toon-rust
}

struct ResultRow {
    library: &'static str,
    operation: &'static str,
    throughput_mb_s: f64,
    docs_per_sec: f64,
    success_rate: f64,
    total_time_s: f64,
}

fn record(library: &'static str, operation: &'static str, bytes: f64, ops: u64, seconds: f64, attempted: u64) -> ResultRow {
    let throughput = if ops > 0 { bytes / seconds / 1e6 } else { 0.0 };
    let success_rate = if attempted > 0 { ops as f64 / attempted as f64 } else { 0.0 };
    println!(
        "{:<8} {:<14} {:>9.2} MB/s {:>14.0} {:>8.0}%  {:>8.4} s  (x{} reps)",
        library, operation, throughput, ops as f64 / seconds, success_rate * 100.0, seconds, REPEATS
    );
    ResultRow { library, operation, throughput_mb_s: throughput, docs_per_sec: ops as f64 / seconds, success_rate, total_time_s: seconds }
}

/// Untimed. Writes what each operation actually produced for each file to
///   <results dir>/dump/rust/<library>/<operation>/<file index>.txt
/// (nothing for a file the library failed on) so report/verify_outputs.mjs can
/// judge whether the output is CORRECT, not merely error-free -- see that
/// script's header.
fn dump_outputs(results_path: &str, files: &[BenchFile]) {
    let root = std::path::Path::new(results_path)
        .parent()
        .unwrap_or_else(|| std::path::Path::new("."))
        .join("dump")
        .join("rust");
    let _ = fs::remove_dir_all(&root);
    let write = |library: &str, operation: &str, idx: usize, text: &str| {
        let dir = root.join(library).join(operation);
        if fs::create_dir_all(&dir).is_ok() {
            let _ = fs::write(dir.join(format!("{}.txt", idx)), text);
        }
    };

    for (i, f) in files.iter().enumerate() {
        // ctoon: json_to_toon / toon_to_json / roundtrip call ctoon's
        // one-shot text conversion functions (no ctoon::Value in between).
        if let Ok(toon) = ctoon::json_to_toon(&f.json) {
            write("ctoon", "json_to_toon", i, &toon);
            if let Ok(js) = ctoon::toon_to_json(&toon) {
                write("ctoon", "roundtrip", i, &js);
            }
        }

        // toon-rust
        if let Ok(val) = serde_json::from_str::<JsonValue>(&f.json) {
            if let Ok(toon) = encode_default(&val) {
                write("toon-rust", "json_to_toon", i, &toon);
                if let Ok(val2) = decode_default::<JsonValue>(&toon) {
                    if let Ok(js) = serde_json::to_string(&val2) {
                        write("toon-rust", "roundtrip", i, &js);
                    }
                }
            }
        }

        // toon_to_json: both libraries decode the shared (ctoon-made) TOON.
        // toon_to_struct: the same decoded value, serialised to JSON
        // (untimed) only so the verifier can compare it to the original.
        if let Some(toon) = &f.toon {
            if let Ok(js) = ctoon::toon_to_json(toon) {
                write("ctoon", "toon_to_json", i, &js);
            }
            // toon_to_struct: the decoded value serialised to JSON (untimed).
            if let Ok(val) = ctoon::loads(toon) {
                if let Ok(js) = ctoon::dumps_json(&val, 2) {
                    write("ctoon", "toon_to_struct", i, &js.to_string());
                }
            }
            if let Ok(val) = decode_default::<JsonValue>(toon) {
                if let Ok(js) = serde_json::to_string(&val) {
                    write("toon-rust", "toon_to_json", i, &js);
                    write("toon-rust", "toon_to_struct", i, &js);
                }
            }
        }

        // struct_to_toon: encode the pre-built in-memory values.
        if let Some(v) = &f.value {
            if let Ok(toon) = ctoon::dumps(v) {
                write("ctoon", "struct_to_toon", i, &toon);
            }
        }
        if let Some(v) = &f.plain {
            if let Ok(toon) = encode_default(v) {
                write("toon-rust", "struct_to_toon", i, &toon);
            }
        }
    }
}

// ------------------------------------------------------------ order check --
// Key order, one direction at a time, against the shared samples in
// benchmarks/order_check/ (sample.json / sample.toon: non-alphabetical keys,
// a nested object, a tabular array):
//   json_to_toon -- keys of the TOON output must follow the order of sample.json
//   toon_to_json -- keys of the JSON output must follow the order of sample.toon
// Both reduce to "first-occurrence order of the keys", so the tabular header
// `list[2]{z,a}:` and the expanded `- z: 1` form compare equal. Hand-written
// scanners (no regex crate: this benchmark keeps its dependency list short).
const ORDER_DIR: &str = concat!(env!("CARGO_MANIFEST_DIR"), "/../order_check");

fn add_key(keys: &mut Vec<String>, raw: &str) {
    let mut k = raw.trim();
    if k.len() >= 2 && k.starts_with('"') && k.ends_with('"') {
        k = &k[1..k.len() - 1];
    }
    if !k.is_empty() && !keys.iter().any(|x| x == k) {
        keys.push(k.to_string());
    }
}

/// Every `"key":` in a JSON text, first occurrence only.
fn json_keys(text: &str) -> Vec<String> {
    let b = text.as_bytes();
    let mut keys = Vec::new();
    let mut i = 0;
    while i < b.len() {
        if b[i] != b'"' {
            i += 1;
            continue;
        }
        let start = i + 1;
        let mut q = start;
        while q < b.len() && b[q] != b'"' {
            q += if b[q] == b'\\' && q + 1 < b.len() { 2 } else { 1 };
        }
        if q >= b.len() {
            break;
        }
        let mut r = q + 1;
        while r < b.len() && b[r].is_ascii_whitespace() {
            r += 1;
        }
        if r < b.len() && b[r] == b':' {
            add_key(&mut keys, &text[start..q]);
        }
        i = q + 1;
    }
    keys
}

/// Line by line: strip indent and `- ` list markers, then `key:`, `key[N]:`
/// or `key[N]{f1,f2}:` -- the key and, for the tabular form, the fields.
fn toon_keys(text: &str) -> Vec<String> {
    let mut keys = Vec::new();
    for line in text.lines() {
        let b = line.as_bytes();
        let end = b.len();
        let mut s = 0;
        while s < end && b[s] == b' ' {
            s += 1;
        }
        while s + 1 < end && b[s] == b'-' && b[s + 1] == b' ' {
            s += 2;
            while s < end && b[s] == b' ' {
                s += 1;
            }
        }
        let mut q = s;
        if q < end && b[q] == b'"' {
            q += 1;
            while q < end && b[q] != b'"' {
                q += if b[q] == b'\\' && q + 1 < end { 2 } else { 1 };
            }
            if q < end {
                q += 1;
            }
        } else {
            while q < end && !b" \t:[{,\"".contains(&b[q]) {
                q += 1;
            }
        }
        let key_end = q;
        let mut fields: Option<&str> = None;
        if q < end && b[q] == b'[' {
            while q < end && b[q] != b']' {
                q += 1;
            }
            if q < end {
                q += 1;
            }
        }
        if q < end && b[q] == b'{' {
            let f = q + 1;
            while q < end && b[q] != b'}' {
                q += 1;
            }
            fields = Some(&line[f..q]);
            if q < end {
                q += 1;
            }
        }
        if q < end && b[q] == b':' && key_end > s {
            add_key(&mut keys, &line[s..key_end]);
            if let Some(f) = fields {
                for part in f.split(|c| c == ',' || c == '|' || c == '\t') {
                    add_key(&mut keys, part);
                }
            }
        }
    }
    keys
}

struct OrderRow {
    library: &'static str,
    operation: &'static str,
    preserved: bool,
    detail: String,
}

type Convert<'a> = Option<&'a dyn Fn(&str) -> Result<String, String>>;

/// One row per direction; a `None` function (the library has no such
/// direction) adds no row.
fn check_order(library: &'static str, json_to_toon: Convert, toon_to_json: Convert) -> Vec<OrderRow> {
    let sample_json = fs::read_to_string(format!("{}/sample.json", ORDER_DIR));
    let sample_toon = fs::read_to_string(format!("{}/sample.toon", ORDER_DIR));
    let (sample_json, sample_toon) = match (sample_json, sample_toon) {
        (Ok(j), Ok(t)) => (j, t),
        _ => {
            eprintln!("warning: order-check samples not found in {}", ORDER_DIR);
            return Vec::new();
        }
    };
    let expected = json_keys(&sample_json);
    let mut rows = Vec::new();
    let cases: [(&'static str, Convert, &str, fn(&str) -> Vec<String>); 2] = [
        ("json_to_toon", json_to_toon, &sample_json, toon_keys),
        ("toon_to_json", toon_to_json, &sample_toon, json_keys),
    ];
    for (operation, convert, sample, extract) in cases {
        let Some(convert) = convert else { continue };
        let (preserved, detail) = match convert(sample) {
            Ok(out) => {
                let got = extract(&out);
                if got == expected { (true, String::new()) } else { (false, format!("got {}", got.join(","))) }
            }
            Err(e) => (false, format!("exception: {}", e)),
        };
        // keep the hand-built results JSON valid: no quotes/backslashes, short
        let detail: String = detail
            .chars()
            .map(|c| if c == '"' || c == '\\' || c.is_control() { '\'' } else { c })
            .take(127)
            .collect();
        rows.push(OrderRow { library, operation, preserved, detail });
    }
    rows
}

fn main() {
    let args: Vec<String> = std::env::args().collect();
    if args.len() < 3 {
        eprintln!("usage: bench <manifest> <results.json>");
        std::process::exit(1);
    }
    let manifest_path = &args[1];
    let results_path = &args[2];

    let manifest = fs::File::open(manifest_path).unwrap_or_else(|e| {
        eprintln!("Cannot open manifest: {} ({})", manifest_path, e);
        std::process::exit(1);
    });

    let mut files: Vec<BenchFile> = Vec::new();
    for line in std::io::BufReader::new(manifest).lines().flatten() {
        let path = line.trim();
        if path.is_empty() {
            continue;
        }
        if let Ok(json) = fs::read_to_string(path) {
            files.push(BenchFile { json, toon: None, value: None, plain: None });
        }
    }
    if files.is_empty() {
        eprintln!("Corpus manifest is empty or unreadable - nothing to benchmark.");
        std::process::exit(1);
    }

    let total_json_bytes: usize = files.iter().map(|f| f.json.len()).sum();

    println!("CToon Benchmarks - Rust");
    println!("Corpus: {} files, {:.2} MB (JSON)\n", files.len(), total_json_bytes as f64 / 1e6);
    println!(
        "{:<8} {:<14} {:>12} {:>14} {:>9} {:>10} {:>12}",
        "Library", "Operation", "Throughput", "Docs/sec", "Success", "", "Total time"
    );

    // Untimed pre-pass: TOON text with ctoon, shared decode input for all.
    for f in files.iter_mut() {
        if let Ok(val) = ctoon::loads_json(&f.json) {
            if let Ok(toon) = ctoon::dumps(&val) {
                f.toon = Some(toon);
            }
            f.value = Some(val);
        }
        f.plain = serde_json::from_str::<JsonValue>(&f.json).ok();
    }

    // Outputs for report/verify_outputs.mjs live next to the results JSON.
    dump_outputs(results_path, &files);

    let mut results: Vec<ResultRow> = Vec::new();

    // ── ctoon ──
    let mut ops = 0u64;
    let mut bytes = 0f64;
    let t0 = Instant::now();
    for _ in 0..REPEATS {
        for f in &files {
            if let Ok(toon) = ctoon::json_to_toon(&f.json) {
                std::hint::black_box(&toon);
                ops += 1;
                bytes += f.json.len() as f64;
            }
        }
    }
    results.push(record("ctoon", "json_to_toon", bytes, ops, t0.elapsed().as_secs_f64(), files.len() as u64 * REPEATS as u64));

    let mut ops2 = 0u64;
    let mut bytes2 = 0f64;
    let t0 = Instant::now();
    for _ in 0..REPEATS {
        for f in &files {
            if let Some(toon) = &f.toon {
                if let Ok(js) = ctoon::toon_to_json(toon) {
                    std::hint::black_box(&js);
                    ops2 += 1;
                    bytes2 += toon.len() as f64;
                }
            }
        }
    }
    results.push(record("ctoon", "toon_to_json", bytes2, ops2, t0.elapsed().as_secs_f64(), files.len() as u64 * REPEATS as u64));

    let mut ops3 = 0u64;
    let mut bytes3 = 0f64;
    let t0 = Instant::now();
    for _ in 0..REPEATS {
        for f in &files {
            if let Ok(toon) = ctoon::json_to_toon(&f.json) {
                if let Ok(js) = ctoon::toon_to_json(&toon) {
                    std::hint::black_box(&js);
                    ops3 += 1;
                    bytes3 += f.json.len() as f64;
                }
            }
        }
    }
    results.push(record("ctoon", "roundtrip", bytes3, ops3, t0.elapsed().as_secs_f64(), files.len() as u64 * REPEATS as u64));

    // ── toon-rust ──
    let mut ops = 0u64;
    let mut bytes = 0f64;
    let t0 = Instant::now();
    for _ in 0..REPEATS {
        for f in &files {
            if let Ok(val) = serde_json::from_str::<JsonValue>(&f.json) {
                if encode_default(&val).is_ok() {
                    ops += 1;
                    bytes += f.json.len() as f64;
                }
            }
        }
    }
    results.push(record("toon-rust", "json_to_toon", bytes, ops, t0.elapsed().as_secs_f64(), files.len() as u64 * REPEATS as u64));

    let mut ops2 = 0u64;
    let mut bytes2 = 0f64;
    let t0 = Instant::now();
    for _ in 0..REPEATS {
        for f in &files {
            if let Some(toon) = &f.toon {
                if let Ok(val) = decode_default::<JsonValue>(toon) {
                    if serde_json::to_string(&val).is_ok() {
                        ops2 += 1;
                        bytes2 += toon.len() as f64;
                    }
                }
            }
        }
    }
    results.push(record("toon-rust", "toon_to_json", bytes2, ops2, t0.elapsed().as_secs_f64(), files.len() as u64 * REPEATS as u64));

    let mut ops3 = 0u64;
    let mut bytes3 = 0f64;
    let t0 = Instant::now();
    for _ in 0..REPEATS {
        for f in &files {
            if let Ok(val) = serde_json::from_str::<JsonValue>(&f.json) {
                if let Ok(toon) = encode_default(&val) {
                    if let Ok(val2) = decode_default::<JsonValue>(&toon) {
                        if serde_json::to_string(&val2).is_ok() {
                            ops3 += 1;
                            bytes3 += f.json.len() as f64;
                        }
                    }
                }
            }
        }
    }
    results.push(record("toon-rust", "roundtrip", bytes3, ops3, t0.elapsed().as_secs_f64(), files.len() as u64 * REPEATS as u64));

    // ── toon_to_struct / struct_to_toon ──
    let attempted = files.len() as u64 * REPEATS as u64;

    let mut ops = 0u64;
    let mut bytes = 0f64;
    let t0 = Instant::now();
    for _ in 0..REPEATS {
        for f in &files {
            if let Some(toon) = &f.toon {
                if let Ok(val) = ctoon::loads(toon) {
                    std::hint::black_box(&val);
                    ops += 1;
                    bytes += toon.len() as f64;
                }
            }
        }
    }
    results.push(record("ctoon", "toon_to_struct", bytes, ops, t0.elapsed().as_secs_f64(), attempted));

    let mut ops = 0u64;
    let mut bytes = 0f64;
    let t0 = Instant::now();
    for _ in 0..REPEATS {
        for f in &files {
            if let Some(v) = &f.value {
                if let Ok(toon) = ctoon::dumps(v) {
                    std::hint::black_box(&toon);
                    ops += 1;
                    bytes += f.json.len() as f64;
                }
            }
        }
    }
    results.push(record("ctoon", "struct_to_toon", bytes, ops, t0.elapsed().as_secs_f64(), attempted));

    let mut ops = 0u64;
    let mut bytes = 0f64;
    let t0 = Instant::now();
    for _ in 0..REPEATS {
        for f in &files {
            if let Some(toon) = &f.toon {
                if let Ok(val) = decode_default::<JsonValue>(toon) {
                    std::hint::black_box(&val);
                    ops += 1;
                    bytes += toon.len() as f64;
                }
            }
        }
    }
    results.push(record("toon-rust", "toon_to_struct", bytes, ops, t0.elapsed().as_secs_f64(), attempted));

    let mut ops = 0u64;
    let mut bytes = 0f64;
    let t0 = Instant::now();
    for _ in 0..REPEATS {
        for f in &files {
            if let Some(v) = &f.plain {
                if let Ok(toon) = encode_default(v) {
                    std::hint::black_box(&toon);
                    ops += 1;
                    bytes += f.json.len() as f64;
                }
            }
        }
    }
    results.push(record("toon-rust", "struct_to_toon", bytes, ops, t0.elapsed().as_secs_f64(), attempted));

    // Write results JSON
    let mut out = String::new();
    out.push_str("{\n  \"language\": \"rust\",\n");
    out.push_str(&format!("  \"corpus\": {{\"files\": {}, \"bytes\": {}}},\n", files.len(), total_json_bytes));
    out.push_str("  \"results\": [\n");
    for (i, r) in results.iter().enumerate() {
        out.push_str(&format!(
            "    {{\"library\": \"{}\", \"operation\": \"{}\", \"throughput_mb_s\": {:.4}, \"docs_per_sec\": {:.2}, \"success_rate\": {:.4}, \"total_time_s\": {:.6}}}{}\n",
            r.library, r.operation, r.throughput_mb_s, r.docs_per_sec, r.success_rate, r.total_time_s,
            if i + 1 < results.len() { "," } else { "" }
        ));
    }
    out.push_str("  ],\n  \"order_check\": [\n");

    // Key order per library and direction. toon-rust goes through
    // serde_json::Value (what it takes), exactly as in the timed rows above.
    let mut order_rows = check_order(
        "ctoon",
        Some(&|s| ctoon::json_to_toon(s).map_err(|e| format!("{:?}", e))),
        Some(&|s| ctoon::toon_to_json(s).map_err(|e| format!("{:?}", e))),
    );
    order_rows.extend(check_order(
        "toon-rust",
        Some(&|s| {
            let val = serde_json::from_str::<JsonValue>(s).map_err(|e| e.to_string())?;
            encode_default(&val).map_err(|e| format!("{:?}", e))
        }),
        Some(&|s| {
            let val = decode_default::<JsonValue>(s).map_err(|e| format!("{:?}", e))?;
            serde_json::to_string(&val).map_err(|e| e.to_string())
        }),
    ));
    for (i, r) in order_rows.iter().enumerate() {
        out.push_str(&format!(
            "    {{\"library\": \"{}\", \"operation\": \"{}\", \"preserved\": {}, \"detail\": \"{}\"}}{}\n",
            r.library, r.operation, r.preserved, r.detail,
            if i + 1 < order_rows.len() { "," } else { "" }
        ));
    }
    out.push_str("  ]\n}\n");

    match fs::File::create(results_path).and_then(|mut f| f.write_all(out.as_bytes())) {
        Ok(_) => println!("\nResults written to {}", results_path),
        Err(e) => eprintln!("warning: could not write {}: {}", results_path, e),
    }
}
