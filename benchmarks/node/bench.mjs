#!/usr/bin/env node
// CToon Benchmarks — Node.js.
//
// ctoon has no Node.js binding yet, so this currently benchmarks the
// official @toon-format/toon package (npmjs.com/package/@toon-format/toon)
// alone — but it follows the exact same methodology, corpus, and JSON
// output schema as every other language benchmark here, so a future
// ctoon Node binding slots in the same way gotoon/toon-go do for Go.
//
// Methodology (same across every language benchmark in this repo):
//   1. Load every file in the corpus manifest into memory (untimed).
//   2. Untimed pre-pass: convert each JSON file to TOON once (using
//      @toon-format/toon itself, since it's the only implementation here).
//   3. Timed "json_to_toon": repeatedly parse JSON and re-serialise to TOON.
//   4. Timed "toon_to_json": repeatedly parse the TOON text from step 2 and
//      re-serialise to JSON.
//   5. Timed "roundtrip": json -> toon -> parse toon -> json, chained as
//      one operation using the library's own encoder/decoder together.
//   6. Report throughput (MB/s of bytes actually read by successful
//      conversions only) and documents/sec.
//
// If a log file path is given, one line per failure is written there —
// only for the first repeat of each timed phase, not all 20.

import { readFileSync, writeFileSync, openSync, writeSync, closeSync } from "node:fs";
import { encode, decode } from "@toon-format/toon";

const REPEATS = 20;

function loadCorpus(manifestPath) {
  const paths = readFileSync(manifestPath, "utf8")
    .split("\n")
    .map((l) => l.trim())
    .filter(Boolean);
  const files = [];
  for (const path of paths) {
    try {
      files.push({ path, json: readFileSync(path, "utf8"), toon: null });
    } catch {
      // skip unreadable entries
    }
  }
  return files;
}

function record(results, library, operation, bytes, ops, seconds, attempted) {
  const throughput = ops > 0 ? bytes / seconds / 1e6 : 0;
  const successRate = attempted > 0 ? ops / attempted : 0;
  console.log(
    `${library.padEnd(8)} ${operation.padEnd(14)} ${throughput.toFixed(2).padStart(9)} MB/s ` +
      `${(ops / seconds).toFixed(0).padStart(14)} ${(successRate * 100).toFixed(0).padStart(8)}%  ` +
      `${seconds.toFixed(4).padStart(8)} s  (x${REPEATS} reps)`
  );
  results.push({
    library,
    operation,
    throughput_mb_s: throughput,
    docs_per_sec: ops / seconds,
    success_rate: successRate,
    total_time_s: seconds,
  });
}

function main() {
  const [, , manifestPath, resultsJsonPath, logPath] = process.argv;
  if (!manifestPath || !resultsJsonPath) {
    console.error("usage: bench.mjs <manifest> <results.json> [log_file]");
    process.exit(1);
  }

  const logFd = logPath ? openSync(logPath, "w") : null;
  const logFail = (library, operation, path, err) => {
    if (logFd === null) return;
    writeSync(logFd, `[${library}] [${operation}] FILE: ${path} ERROR: ${err.message}\n`);
  };

  const files = loadCorpus(manifestPath);
  if (files.length === 0) {
    console.error("Corpus manifest is empty or unreadable - nothing to benchmark.");
    process.exit(1);
  }

  const totalJSONBytes = files.reduce((sum, f) => sum + Buffer.byteLength(f.json), 0);

  console.log("CToon Benchmarks — Node.js");
  console.log(`Corpus: ${files.length} files, ${(totalJSONBytes / 1e6).toFixed(2)} MB (JSON)\n`);
  console.log(
    `${"Library".padEnd(8)} ${"Operation".padEnd(14)} ${"Throughput".padStart(12)} ` +
      `${"Docs/sec".padStart(14)} ${"Success".padStart(9)} ${"".padStart(10)} ${"Total time".padStart(12)}`
  );

  const results = [];

  // Untimed pre-pass: TOON text with @toon-format/toon itself (the only
  // implementation available for this language).
  for (const f of files) {
    try {
      const val = JSON.parse(f.json);
      f.toon = encode(val);
    } catch (e) {
      logFail("@toon-format/toon", "pre_pass", f.path, e);
    }
  }

  // json_to_toon
  let ops = 0;
  let bytes = 0;
  let t0 = process.hrtime.bigint();
  for (let rep = 0; rep < REPEATS; rep++) {
    for (const f of files) {
      try {
        const val = JSON.parse(f.json);
        encode(val);
        ops++;
        bytes += Buffer.byteLength(f.json);
      } catch (e) {
        if (rep === 0) logFail("@toon-format/toon", "json_to_toon", f.path, e);
      }
    }
  }
  let seconds = Number(process.hrtime.bigint() - t0) / 1e9;
  record(results, "@toon-format/toon", "json_to_toon", bytes, ops, seconds, files.length * REPEATS);

  // toon_to_json
  ops = 0;
  bytes = 0;
  t0 = process.hrtime.bigint();
  for (let rep = 0; rep < REPEATS; rep++) {
    for (const f of files) {
      if (!f.toon) continue;
      try {
        const val = decode(f.toon);
        JSON.stringify(val);
        ops++;
        bytes += Buffer.byteLength(f.toon);
      } catch (e) {
        if (rep === 0) logFail("@toon-format/toon", "toon_to_json", f.path, e);
      }
    }
  }
  seconds = Number(process.hrtime.bigint() - t0) / 1e9;
  record(results, "@toon-format/toon", "toon_to_json", bytes, ops, seconds, files.length * REPEATS);

  // roundtrip: json -> toon -> (parse toon) -> json, chained as one
  // operation per file rather than the two legs above run separately.
  ops = 0;
  bytes = 0;
  t0 = process.hrtime.bigint();
  for (let rep = 0; rep < REPEATS; rep++) {
    for (const f of files) {
      try {
        const val = JSON.parse(f.json);
        const toon = encode(val);
        const val2 = decode(toon);
        JSON.stringify(val2);
        ops++;
        bytes += Buffer.byteLength(f.json);
      } catch (e) {
        if (rep === 0) logFail("@toon-format/toon", "roundtrip", f.path, e);
      }
    }
  }
  seconds = Number(process.hrtime.bigint() - t0) / 1e9;
  record(results, "@toon-format/toon", "roundtrip", bytes, ops, seconds, files.length * REPEATS);

  writeFileSync(
    resultsJsonPath,
    JSON.stringify(
      {
        language: "node",
        corpus: { files: files.length, bytes: totalJSONBytes },
        results,
      },
      null,
      2
    )
  );
  console.log(`\nResults written to ${resultsJsonPath}`);

  if (logFd !== null) {
    closeSync(logFd);
    console.log(`Debug log written to ${logPath}`);
  }
}

main();
