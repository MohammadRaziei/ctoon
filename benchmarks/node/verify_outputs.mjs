#!/usr/bin/env node
// verify_outputs.mjs -- makes "success" mean "the output is CORRECT", not
// merely "the call didn't throw".
//
// Every language harness, in an untimed pass before its timed runs, writes
// the text each (library, operation) produced for each corpus file to
//   <results dir>/dump/<language>/<library>/<operation>/<file index>.txt
// (nothing is written for a file the library failed on). This script, run
// once after all the languages, judges all of those outputs against ONE
// shared criterion, so no language gets a private notion of "correct":
//
//   json_to_toon   the TOON text must decode -- with the OFFICIAL reference
//                  decoder (@toon-format/toon), not with the library's own,
//                  because a lenient decoder hides encoder bugs from its own
//                  round trip -- back to the original JSON value.
//   toon_to_json   the output must be JSON that parses to the original value.
//   roundtrip      likewise.
//
// toon_to_json is fed, for every library, the TOON that ctoon produced for
// that file (the shared pre-pass). If that input is itself missing or wrong,
// no decoder can be blamed for failing on it, so such a file is left out of
// EVERY library's toon_to_json row (numerator and denominator alike); the
// encoder that produced the bad input is charged for it in its own
// json_to_toon row instead.
//
// "The same value" is the spec's JSON-model equality, with numbers compared
// as IEEE-754 doubles (SPEC 2: an encoder MAY emit the host's numeric
// approximation for numbers outside its domain, so a 60-digit integer that
// comes back as the nearest double is correct): objects are unordered maps,
// arrays are ordered, -0 equals 0. Key ORDER is checked separately (the "key
// order preservation" table), not here.
//
// "Success" and "correctness" are two separate parameters, because they say
// different things: a library can run without error and still hand back the
// wrong document (and a wrong answer shouldn't read as a fast one).
// This script rewrites each <language>.json row in place:
//   success_rate     files converted WITHOUT ERROR / files attempted
//                    (the harness's own notion; recomputed here from the
//                    recorded outputs only so that the toon_to_json
//                    denominator can leave out bad shared inputs, see above)
//   correct_rate     of the outputs that WERE produced, the share that are
//                    correct -- so "98% success, 0% correct" is readable at a
//                    glance. null when nothing was produced.
//   throughput_mb_s  }  re-derived so that only bytes of CORRECT conversions
//   docs_per_sec     }  count (a wrong answer must not look like a fast one).
//                       The time is unchanged: it is the time of all attempts.
// and keeps what the harness measured under "raw", so re-running this script
// never compounds.
//
// Usage: node verify_outputs.mjs <corpus_manifest.txt> <results dir>

import { readFileSync, writeFileSync, readdirSync, existsSync, statSync } from "node:fs";
import { join, basename } from "node:path";
import { decode } from "@toon-format/toon";

const [manifestPath, resultsDir] = process.argv.slice(2);
if (!manifestPath || !resultsDir) {
  console.error("usage: verify_outputs.mjs <corpus_manifest.txt> <results dir>");
  process.exit(2);
}

// Must match each harness's directory naming (only Node has a library whose
// name isn't already filesystem-safe: "@toon-format/toon").
const safe = (name) => name.replace(/[^A-Za-z0-9._-]/g, "_");

const paths = readFileSync(manifestPath, "utf8").split("\n").map((l) => l.trim()).filter(Boolean);

// The originals, parsed once. A file the reference JSON parser can't read
// can't be judged; it is reported and counts as incorrect for everyone.
const originals = paths.map((p) => {
  try {
    const text = readFileSync(p, "utf8");
    return { path: p, bytes: Buffer.byteLength(text, "utf8"), value: JSON.parse(text), ok: true };
  } catch {
    return { path: p, bytes: 0, value: undefined, ok: false };
  }
});
const unreadable = originals.filter((o) => !o.ok).length;
if (unreadable) console.error(`verify_outputs: ${unreadable} corpus file(s) are not valid JSON to the reference parser; they count as incorrect.`);

function same(a, b) {
  if (a === b) return true;                                   // also -0 === 0
  if (typeof a === "number" && typeof b === "number") return Number.isNaN(a) && Number.isNaN(b);
  if (a === null || b === null || typeof a !== "object" || typeof b !== "object") return false;
  if (Array.isArray(a) !== Array.isArray(b)) return false;
  if (Array.isArray(a)) return a.length === b.length && a.every((x, i) => same(x, b[i]));
  const ka = Object.keys(a);
  if (ka.length !== Object.keys(b).length) return false;
  return ka.every((k) => Object.prototype.hasOwnProperty.call(b, k) && same(a[k], b[k]));
}

function judge(operation, text, original) {
  if (!original.ok) return false;
  try {
    const value = operation === "json_to_toon" ? decode(text) : JSON.parse(text);
    return same(value, original.value);
  } catch {
    return false;
  }
}

// A harness records its outputs in one of two layouts:
//   dump/<lang>/<library>/<operation>/<file index>.txt   one file per output
//   dump_<lang>_<library>_<operation>.frames             one file per operation,
//       records of "<file index>\t<byte length>\n<bytes>" (used by Zig, whose
//       file/dir API is too unstable to rely on blind)
// Either way "no record for a file" means the library failed on it.
// openDump() returns file index -> output text | null, or null if the harness
// recorded nothing for this (language, library, operation).
function parseFrames(buf) {
  const out = new Map();
  let pos = 0;
  while (pos < buf.length) {
    const nl = buf.indexOf(0x0a, pos);
    if (nl < 0) break;
    const [idx, len] = buf.toString("utf8", pos, nl).split("\t").map(Number);
    if (!Number.isInteger(idx) || !Number.isInteger(len) || nl + 1 + len > buf.length) break;
    out.set(idx, buf.toString("utf8", nl + 1, nl + 1 + len));
    pos = nl + 1 + len;
  }
  return out;
}

function openDump(lang, library, operation) {
  const dir = join(resultsDir, "dump", lang, safe(library), operation);
  if (existsSync(dir) && statSync(dir).isDirectory()) {
    return (i) => {
      const f = join(dir, `${i}.txt`);
      return existsSync(f) ? readFileSync(f, "utf8") : null;
    };
  }
  const framed = join(resultsDir, `dump_${lang}_${safe(library)}_${operation}.frames`);
  if (existsSync(framed)) {
    const m = parseFrames(readFileSync(framed));
    return (i) => (m.has(i) ? m.get(i) : null);
  }
  return null;
}

let anyRow = false, anyUnverified = false;
for (const file of readdirSync(resultsDir).filter((f) => f.endsWith(".json") && f !== "system_info.json").sort()) {
  const jsonPath = join(resultsDir, file);
  let data;
  try { data = JSON.parse(readFileSync(jsonPath, "utf8")); } catch { continue; }
  if (!data || !Array.isArray(data.results) || !data.language) continue;
  const lang = data.language;

  // The shared decode input every toon_to_json run used is the language's
  // own pre-pass TOON -- ctoon's json_to_toon output (or, for Node, the only
  // library there). Its per-file size is the byte weight of toon_to_json.
  const libs = [...new Set(data.results.map((r) => r.library))];
  const prepassLib = libs.includes("ctoon") ? "ctoon" : libs[0];
  const readPrepass = openDump(lang, prepassLib ?? "", "json_to_toon");
  const prepassText = paths.map((_, i) => (readPrepass ? readPrepass(i) : null));
  const prepassBytes = prepassText.map((t) => (t === null ? null : Buffer.byteLength(t, "utf8")));
  // A usable shared input: it exists AND is correct TOON for that file.
  const prepassUsable = prepassText.map((t, i) => t !== null && judge("json_to_toon", t, originals[i]));
  const unusableInputs = prepassUsable.filter((u) => !u).length;

  for (const row of data.results) {
    anyRow = true;
    const readOutput = openDump(lang, row.library, row.operation);
    if (!readOutput) {
      row.verified = false;                      // harness wrote no dump: can't judge it
      anyUnverified = true;
      console.error(`verify_outputs: ${lang}/${row.library}/${row.operation}: no dump -> UNVERIFIED (keeping the no-exception rate)`);
      continue;
    }
    const raw = row.raw ?? { throughput_mb_s: row.throughput_mb_s, docs_per_sec: row.docs_per_sec, success_rate: row.success_rate };

    const sharedInput = row.operation === "toon_to_json";
    let noError = 0, correct = 0, bytesNoError = 0, bytesCorrect = 0, considered = 0;
    const bad = [];
    for (let i = 0; i < originals.length; i++) {
      if (sharedInput && !prepassUsable[i]) continue;                 // bad shared input: nobody's fault but the encoder's
      considered++;
      const text = readOutput(i);
      if (text === null) continue;                                    // the library failed on this file
      const w = row.operation === "toon_to_json" ? (prepassBytes[i] ?? 0) : originals[i].bytes;
      noError++; bytesNoError += w;
      if (judge(row.operation, text, originals[i])) { correct++; bytesCorrect += w; }
      else if (bad.length < 3) bad.push(basename(originals[i].path));
    }

    row.raw = raw;
    row.verified = true;
    row.success_rate = considered ? noError / considered : 0;
    row.correct_rate = noError ? correct / noError : null;
    row.throughput_mb_s = bytesNoError ? raw.throughput_mb_s * (bytesCorrect / bytesNoError) : 0;
    row.docs_per_sec = noError ? raw.docs_per_sec * (correct / noError) : 0;
    row.verify = { files: considered, no_error: noError, correct, incorrect_examples: bad };
    if (sharedInput && unusableInputs) row.verify.excluded_bad_shared_inputs = unusableInputs;

    const wrong = noError - correct;
    if (wrong > 0) {
      console.error(`verify_outputs: ${lang}/${row.library}/${row.operation}: ${wrong} of ${noError} outputs that raised no error are WRONG (e.g. ${bad.join(", ")})`);
    }
  }
  writeFileSync(jsonPath, JSON.stringify(data, null, 2));
}

if (!anyRow) { console.error("verify_outputs: no results found in " + resultsDir); process.exit(1); }
console.error(anyUnverified
  ? "verify_outputs: done (some rows unverified, see above)"
  : "verify_outputs: done (every row verified)");
