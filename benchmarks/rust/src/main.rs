// CToon Benchmarks — Rust.
//
// Every implementation here — ctoon included — is one more peer entry,
// fetched from its own GitHub/crates.io home the same way, tested the
// same way. No "ctoon vs X" framing: one shared results table, one row
// per (library, operation) pair. Same methodology, corpus, and JSON
// output schema as every other language benchmark here — see
// benchmarks/go/bench.go for the reference version of this comment.
//
// Implementations:
//   - ctoon      this project (github.com/mohammadraziei/ctoon), fetched
//                as a git dependency in Cargo.toml — not a local path,
//                even though this repo's root is right above us; see
//                this directory's CMakeLists.txt for why.
//   - toon-rust  github.com/toon-format/toon-rust (official)
//
// Methodology (same across every language benchmark in this repo):
//   1. Load every file in the corpus manifest into memory (untimed).
//   2. Untimed pre-pass: convert each JSON file to TOON once (with
//      ctoon) — the shared decode input every library's toon_to_json
//      leg reads from.
//   3. Timed "json_to_toon": repeatedly parse JSON and re-serialise to
//      TOON, with each library's own encoder.
//   4. Timed "toon_to_json": repeatedly parse the TOON text from step 2
//      and re-serialise to JSON. Measures interop with ctoon's specific
//      output.
//   5. Timed "roundtrip": json -> toon -> parse toon -> json, all
//      chained as one operation using each library's own encoder/decoder
//      together. Measures self-consistency, not interop — see the
//      top-level README for why a library's toon_to_json and roundtrip
//      success rates can legitimately differ a lot.
//   6. Report throughput (MB/s of bytes actually read by successful
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
            files.push(BenchFile { json, toon: None });
        }
    }
    if files.is_empty() {
        eprintln!("Corpus manifest is empty or unreadable - nothing to benchmark.");
        std::process::exit(1);
    }

    let total_json_bytes: usize = files.iter().map(|f| f.json.len()).sum();

    println!("CToon Benchmarks — Rust");
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
        }
    }

    let mut results: Vec<ResultRow> = Vec::new();

    // ── ctoon ──
    let mut ops = 0u64;
    let mut bytes = 0f64;
    let t0 = Instant::now();
    for _ in 0..REPEATS {
        for f in &files {
            if let Ok(val) = ctoon::loads_json(&f.json) {
                if ctoon::dumps(&val).is_ok() {
                    ops += 1;
                    bytes += f.json.len() as f64;
                }
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
                if let Ok(val) = ctoon::loads(toon) {
                    if ctoon::dumps_json(&val, 2).is_ok() {
                        ops2 += 1;
                        bytes2 += toon.len() as f64;
                    }
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
            if let Ok(val) = ctoon::loads_json(&f.json) {
                if let Ok(toon) = ctoon::dumps(&val) {
                    if let Ok(val2) = ctoon::loads(&toon) {
                        if ctoon::dumps_json(&val2, 2).is_ok() {
                            ops3 += 1;
                            bytes3 += f.json.len() as f64;
                        }
                    }
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
    out.push_str("  ]\n}\n");

    match fs::File::create(results_path).and_then(|mut f| f.write_all(out.as_bytes())) {
        Ok(_) => println!("\nResults written to {}", results_path),
        Err(e) => eprintln!("warning: could not write {}: {}", results_path, e),
    }
}
