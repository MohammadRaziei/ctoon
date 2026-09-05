// CToon Benchmarks — Rust.
//
// ctoon has no Rust binding yet, so this currently benchmarks toon-rust
// (github.com/toon-format/toon-rust) alone — but it follows the exact
// same methodology, corpus, and JSON output schema as every other
// language benchmark here, so a future ctoon Rust binding slots in the
// same way gotoon/toon-go do for Go.
//
// Methodology (same across every language benchmark in this repo):
//   1. Load every file in the corpus manifest into memory (untimed).
//   2. Untimed pre-pass: convert each JSON file to TOON once (using
//      toon-rust itself, since it's the only implementation here — there's
//      no ctoon output to use as a shared reference for this language).
//   3. Timed "json_to_toon": repeatedly parse JSON and re-serialise to TOON.
//   4. Timed "toon_to_json": repeatedly parse the TOON text from step 2 and
//      re-serialise to JSON.
//   5. Report throughput (MB/s of bytes actually read by successful
//      conversions only) and documents/sec.

use serde_json::Value;
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

    // Untimed pre-pass: TOON text with toon-rust itself (the only
    // implementation available for this language).
    for f in files.iter_mut() {
        if let Ok(val) = serde_json::from_str::<Value>(&f.json) {
            if let Ok(toon) = encode_default(&val) {
                f.toon = Some(toon);
            }
        }
    }

    let mut results: Vec<ResultRow> = Vec::new();

    // json_to_toon
    let mut ops = 0u64;
    let mut bytes = 0f64;
    let t0 = Instant::now();
    for _ in 0..REPEATS {
        for f in &files {
            if let Ok(val) = serde_json::from_str::<Value>(&f.json) {
                if encode_default(&val).is_ok() {
                    ops += 1;
                    bytes += f.json.len() as f64;
                }
            }
        }
    }
    results.push(record("toon-rust", "json_to_toon", bytes, ops, t0.elapsed().as_secs_f64(), files.len() as u64 * REPEATS as u64));

    // toon_to_json
    let mut ops2 = 0u64;
    let mut bytes2 = 0f64;
    let t0 = Instant::now();
    for _ in 0..REPEATS {
        for f in &files {
            if let Some(toon) = &f.toon {
                if let Ok(val) = decode_default::<Value>(toon) {
                    if serde_json::to_string(&val).is_ok() {
                        ops2 += 1;
                        bytes2 += toon.len() as f64;
                    }
                }
            }
        }
    }
    results.push(record("toon-rust", "toon_to_json", bytes2, ops2, t0.elapsed().as_secs_f64(), files.len() as u64 * REPEATS as u64));

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
