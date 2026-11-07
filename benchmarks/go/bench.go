// CToon Go benchmark.
//
// Every implementation here - ctoon included - is one more peer entry,
// fetched from its own GitHub module path, tested the same way. No
// "ctoon vs X" framing: one shared results table, one row per
// (library, operation) pair.
//
// Methodology (same across every language benchmark in this repo):
//  1. Load every file in the corpus manifest into memory (untimed).
//  2. Untimed pre-pass: convert each JSON file to TOON once (with ctoon).
//  3. Timed "json_to_toon": repeatedly parse JSON and re-serialise to TOON.
//  4. Timed "toon_to_json": repeatedly parse the TOON text from step 2 and
//     re-serialise to JSON. Measures interop with ctoon's specific output.
//  5. Timed "roundtrip": json -> toon -> parse toon -> json, all four
//     steps chained as one operation using each library's own
//     encoder/decoder together. Measures self-consistency, not interop -
//     see the top-level README for why a library's toon_to_json and
//     roundtrip success rates can legitimately differ a lot.
//  6. Timed "toon_to_struct" / "struct_to_toon": the same two directions
//     with the text format taken out -- decode the pre-pass TOON into the
//     library's in-memory Go value and stop; encode an in-memory Go value
//     built untimed before the clock starts. ctoon is fed the value ITS OWN
//     LoadsJSON produced (its native structure); the peers are fed the
//     value encoding/json produced (theirs). gotoon has no decoder, so it
//     only has struct_to_toon.
//  7. Report throughput (MB/s of bytes actually read by successful
//     conversions only) and documents/sec.
//
// Implementations:
//   - ctoon    this project (github.com/mohammadraziei/ctoon)
//   - gotoon   github.com/alpkeskin/gotoon (community; encode-only, no decoder)
//   - toon-go  github.com/toon-format/toon-go (official; requires Go >= 1.23,
//              which is why this whole module's go.mod floor is 1.23 too -
//              no separate "vs" subdirectory to work around it)
//
// If a log file path is given (4th CLI arg), one line per failure is
// written there - only for the first repeat of each timed phase (and the
// untimed pre-pass), not all 20 repeats.
package main

import (
	"bufio"
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"
	"regexp"
	"runtime"
	"strconv"
	"strings"
	"time"

	"github.com/alpkeskin/gotoon"
	"github.com/mohammadraziei/ctoon"
	toongo "github.com/toon-format/toon-go"
)

const repeats = 20

type benchFile struct {
	path string
	json string
	toon string // ctoon's own TOON output, shared decode input for all libs

	// In-memory values for struct_to_toon, built UNTIMED in the pre-pass.
	val      interface{} // ctoon's own native value (ctoon.LoadsJSON)
	hasVal   bool
	plain    interface{} // encoding/json's value, fed to the peer libraries
	hasPlain bool
}

type result struct {
	Library       string  `json:"library"`
	Operation     string  `json:"operation"`
	ThroughputMBs float64 `json:"throughput_mb_s"`
	DocsPerSec    float64 `json:"docs_per_sec"`
	SuccessRate   float64 `json:"success_rate"`
	TotalTimeS    float64 `json:"total_time_s"`
}

var results []result
var logFile *os.File

// ------------------------------------------------------------ order check --
// Key order, one direction at a time, against the shared samples in
// benchmarks/order_check/ (sample.json / sample.toon: non-alphabetical keys,
// a nested object, a tabular array):
//   json_to_toon -- keys of the TOON output must follow the order of sample.json
//   toon_to_json -- keys of the JSON output must follow the order of sample.toon
// Both reduce to "first-occurrence order of the keys", so the tabular header
// `list[2]{z,a}:` and the expanded `- z: 1` form compare equal.

type orderResult struct {
	Library   string `json:"library"`
	Operation string `json:"operation"`
	Preserved bool   `json:"preserved"`
}

var (
	jsonKeyRE  = regexp.MustCompile(`"((?:[^"\\]|\\.)*)"\s*:`)
	toonKeyRE  = regexp.MustCompile(`^\s*(?:-\s+)*("(?:[^"\\]|\\.)*"|[^\s:\[\]{}",]+)(?:\[[^\]]*\])?(?:\{([^}]*)\})?:`)
	fieldSepRE = regexp.MustCompile(`[,|\t]`)
)

func uniqueKeys(keys []string) []string {
	seen := []string{}
	for _, k := range keys {
		k = strings.TrimSpace(k)
		if len(k) >= 2 && k[0] == '"' && k[len(k)-1] == '"' {
			k = k[1 : len(k)-1]
		}
		dup := k == ""
		for _, x := range seen {
			if x == k {
				dup = true
			}
		}
		if !dup {
			seen = append(seen, k)
		}
	}
	return seen
}

func jsonKeys(text string) []string {
	var keys []string
	for _, m := range jsonKeyRE.FindAllStringSubmatch(text, -1) {
		keys = append(keys, m[1])
	}
	return uniqueKeys(keys)
}

func toonKeys(text string) []string {
	var keys []string
	for _, line := range strings.Split(text, "\n") {
		m := toonKeyRE.FindStringSubmatch(line)
		if m == nil {
			continue
		}
		keys = append(keys, m[1])
		if m[2] != "" {
			keys = append(keys, fieldSepRE.Split(m[2], -1)...)
		}
	}
	return uniqueKeys(keys)
}

// orderDir finds benchmarks/order_check next to this source file, falling
// back to ../order_check from the working directory.
func orderDir() string {
	if _, file, _, ok := runtime.Caller(0); ok {
		d := filepath.Join(filepath.Dir(file), "..", "order_check")
		if _, err := os.Stat(d); err == nil {
			return d
		}
	}
	return filepath.Join("..", "order_check")
}

// checkOrder returns one row per direction; a nil function (the library has
// no such direction) adds no row.
func checkOrder(library string, jsonToToon, toonToJSON func(string) (string, error)) []orderResult {
	dir := orderDir()
	sj, err1 := os.ReadFile(filepath.Join(dir, "sample.json"))
	st, err2 := os.ReadFile(filepath.Join(dir, "sample.toon"))
	if err1 != nil || err2 != nil {
		fmt.Fprintf(os.Stderr, "warning: order-check samples not found in %s\n", dir)
		return nil
	}
	expected := jsonKeys(string(sj))
	var rows []orderResult
	for _, c := range []struct {
		op      string
		fn      func(string) (string, error)
		sample  string
		extract func(string) []string
	}{
		{"json_to_toon", jsonToToon, string(sj), toonKeys},
		{"toon_to_json", toonToJSON, string(st), jsonKeys},
	} {
		if c.fn == nil {
			continue
		}
		row := orderResult{Library: library, Operation: c.op}
		if out, err := c.fn(c.sample); err == nil {
			row.Preserved = strings.Join(c.extract(out), "\n") == strings.Join(expected, "\n")
		}
		rows = append(rows, row)
	}
	return rows
}

func jsonToValue(s string) (interface{}, error) {
	var v interface{}
	err := json.Unmarshal([]byte(s), &v)
	return v, err
}

func logFail(library, operation, path string, err error) {
	if logFile == nil {
		return
	}
	fmt.Fprintf(logFile, "[%s] [%s] FILE: %s ERROR: %v\n", library, operation, path, err)
}

func loadCorpus(manifestPath string) ([]*benchFile, error) {
	mf, err := os.Open(manifestPath)
	if err != nil {
		return nil, err
	}
	defer mf.Close()

	var files []*benchFile
	scanner := bufio.NewScanner(mf)
	scanner.Buffer(make([]byte, 1024*1024), 1024*1024)
	for scanner.Scan() {
		path := scanner.Text()
		if path == "" {
			continue
		}
		data, err := os.ReadFile(path)
		if err != nil {
			continue
		}
		files = append(files, &benchFile{path: path, json: string(data)})
	}
	return files, nil
}

func record(library, operation string, bytes float64, ops int, seconds float64, attempted int) {
	throughput := 0.0
	if ops > 0 {
		throughput = bytes / seconds / 1e6
	}
	successRate := 0.0
	if attempted > 0 {
		successRate = float64(ops) / float64(attempted)
	}
	results = append(results, result{
		Library: library, Operation: operation,
		ThroughputMBs: throughput, DocsPerSec: float64(ops) / seconds,
		SuccessRate: successRate, TotalTimeS: seconds,
	})
	fmt.Printf("%-8s %-14s %9.2f MB/s %14.0f %8.0f%%  %8.4f s  (x%d reps)\n",
		library, operation, throughput, float64(ops)/seconds, successRate*100, seconds, repeats)
}

// Untimed. Writes what each operation actually produced for each file to
//   <results dir>/dump/go/<library>/<operation>/<file index>.txt
// (nothing for a file the library failed on) so report/verify_outputs.mjs can
// judge whether the output is CORRECT, not merely error-free -- see that
// script's header.
var dumpRoot string

func dumpWrite(library, operation string, idx int, text string) {
	dir := filepath.Join(dumpRoot, library, operation)
	if err := os.MkdirAll(dir, 0o755); err != nil {
		return
	}
	_ = os.WriteFile(filepath.Join(dir, strconv.Itoa(idx)+".txt"), []byte(text), 0o644)
}

func dumpOutputs(files []*benchFile) {
	for i, f := range files {
		// ctoon: json_to_toon / toon_to_json / roundtrip call ctoon's
		// one-shot text conversion functions (no Go value in between).
		if toon, err := ctoon.JSONToToon(f.json, ctoon.DefaultEncodeOptions()); err == nil {
			dumpWrite("ctoon", "json_to_toon", i, toon)
			if js, err := ctoon.ToonToJSON(toon, 0); err == nil {
				dumpWrite("ctoon", "roundtrip", i, js)
			}
		}
		if f.toon != "" {
			if js, err := ctoon.ToonToJSON(f.toon, 0); err == nil {
				dumpWrite("ctoon", "toon_to_json", i, js)
			}
			// toon_to_struct: the decoded value serialised to JSON
			// (untimed) only so the verifier can compare it to the
			// original document.
			if val, err := ctoon.Loads(f.toon); err == nil {
				if js, err := ctoon.DumpsJSON(val, 2); err == nil {
					dumpWrite("ctoon", "toon_to_struct", i, string(js))
				}
			}
		}
		if f.hasVal {
			if toon, err := ctoon.Dumps(f.val); err == nil {
				dumpWrite("ctoon", "struct_to_toon", i, toon)
			}
		}

		var plain interface{}
		if err := json.Unmarshal([]byte(f.json), &plain); err != nil {
			continue
		}

		// gotoon (encode-only)
		if toon, err := gotoon.Encode(plain); err == nil {
			dumpWrite("gotoon", "json_to_toon", i, string(toon))
			dumpWrite("gotoon", "struct_to_toon", i, string(toon))
		}

		// toon-go
		if toon, err := toongo.MarshalString(plain); err == nil {
			dumpWrite("toon-go", "json_to_toon", i, toon)
			if val2, err := toongo.DecodeString(toon); err == nil {
				if js, err := json.Marshal(val2); err == nil {
					dumpWrite("toon-go", "roundtrip", i, string(js))
				}
			}
		}
		if toon, err := toongo.MarshalString(plain); err == nil {
			dumpWrite("toon-go", "struct_to_toon", i, toon)
		}
		if f.toon != "" {
			if val, err := toongo.DecodeString(f.toon); err == nil {
				if js, err := json.Marshal(val); err == nil {
					dumpWrite("toon-go", "toon_to_json", i, string(js))
					dumpWrite("toon-go", "toon_to_struct", i, string(js))
				}
			}
		}
	}
}

func main() {
	if len(os.Args) < 3 {
		fmt.Fprintln(os.Stderr, "usage: bench <manifest> <results.json> [log_file]")
		os.Exit(1)
	}
	manifestPath, resultsPath := os.Args[1], os.Args[2]
	if len(os.Args) >= 4 && os.Args[3] != "" {
		var err error
		logFile, err = os.Create(os.Args[3])
		if err != nil {
			fmt.Fprintf(os.Stderr, "warning: could not open log file %s: %v\n", os.Args[3], err)
		} else {
			defer logFile.Close()
		}
	}

	files, err := loadCorpus(manifestPath)
	if err != nil || len(files) == 0 {
		fmt.Fprintln(os.Stderr, "Corpus manifest is empty or unreadable.")
		os.Exit(1)
	}

	totalJSONBytes := 0
	for _, f := range files {
		totalJSONBytes += len(f.json)
	}

	fmt.Println("CToon Benchmarks - Go")
	fmt.Printf("Corpus: %d files, %.2f MB (JSON)\n\n", len(files), float64(totalJSONBytes)/1e6)
	fmt.Printf("%-8s %-14s %12s %14s %9s %10s %12s\n",
		"Library", "Operation", "Throughput", "Docs/sec", "Success", "", "Total time")

	// Untimed pre-pass: TOON text with ctoon, shared decode input for all.
	totalTOONBytes := 0
	for _, f := range files {
		val, err := ctoon.LoadsJSON(f.json)
		if err != nil {
			logFail("ctoon", "pre_pass", f.path, err)
			continue
		}
		f.val, f.hasVal = val, true
		var plain interface{}
		if err := json.Unmarshal([]byte(f.json), &plain); err == nil {
			f.plain, f.hasPlain = plain, true
		}
		toon, err := ctoon.Dumps(val)
		if err != nil {
			logFail("ctoon", "pre_pass", f.path, err)
			continue
		}
		f.toon = toon
		totalTOONBytes += len(toon)
	}

	// Outputs for report/verify_outputs.mjs live next to the results JSON.
	dumpRoot = filepath.Join(filepath.Dir(resultsPath), "dump", "go")
	_ = os.RemoveAll(dumpRoot)
	dumpOutputs(files)

	// ── ctoon ──  (json_to_toon / toon_to_json / roundtrip: one-shot text
	// conversion functions, no Go value built in between)
	var opsA, opsB int
	var bytesA, bytesB float64
	t0 := time.Now()
	for r := 0; r < repeats; r++ {
		for _, f := range files {
			if _, err := ctoon.JSONToToon(f.json, ctoon.DefaultEncodeOptions()); err == nil {
				opsA++
				bytesA += float64(len(f.json))
			} else if r == 0 {
				logFail("ctoon", "json_to_toon", f.path, err)
			}
		}
	}
	record("ctoon", "json_to_toon", bytesA, opsA, time.Since(t0).Seconds(), len(files)*repeats)

	t0 = time.Now()
	for r := 0; r < repeats; r++ {
		for _, f := range files {
			if f.toon == "" {
				continue
			}
			if _, err := ctoon.ToonToJSON(f.toon, 0); err == nil {
				opsB++
				bytesB += float64(len(f.toon))
			} else if r == 0 {
				logFail("ctoon", "toon_to_json", f.path, err)
			}
		}
	}
	record("ctoon", "toon_to_json", bytesB, opsB, time.Since(t0).Seconds(), len(files)*repeats)

	// ctoon roundtrip: JSONToToon then ToonToJSON on its output, chained as
	// one operation per file.
	var opsRT int
	var bytesRT float64
	t0 = time.Now()
	for r := 0; r < repeats; r++ {
		for _, f := range files {
			toon, err := ctoon.JSONToToon(f.json, ctoon.DefaultEncodeOptions())
			if err != nil {
				if r == 0 {
					logFail("ctoon", "roundtrip", f.path, err)
				}
				continue
			}
			if _, err := ctoon.ToonToJSON(toon, 0); err == nil {
				opsRT++
				bytesRT += float64(len(f.json))
			} else if r == 0 {
				logFail("ctoon", "roundtrip", f.path, err)
			}
		}
	}
	record("ctoon", "roundtrip", bytesRT, opsRT, time.Since(t0).Seconds(), len(files)*repeats)

	// ── gotoon (encode-only: no decoder) ──
	var opsC int
	var bytesC float64
	t0 = time.Now()
	for r := 0; r < repeats; r++ {
		for _, f := range files {
			var val interface{}
			if err := json.Unmarshal([]byte(f.json), &val); err != nil {
				if r == 0 {
					logFail("gotoon", "json_to_toon", f.path, err)
				}
				continue
			}
			if _, err := gotoon.Encode(val); err == nil {
				opsC++
				bytesC += float64(len(f.json))
			} else if r == 0 {
				logFail("gotoon", "json_to_toon", f.path, err)
			}
		}
	}
	record("gotoon", "json_to_toon", bytesC, opsC, time.Since(t0).Seconds(), len(files)*repeats)

	// ── toon-go ──
	var opsD, opsE int
	var bytesD, bytesE float64
	t0 = time.Now()
	for r := 0; r < repeats; r++ {
		for _, f := range files {
			var val interface{}
			if err := json.Unmarshal([]byte(f.json), &val); err != nil {
				if r == 0 {
					logFail("toon-go", "json_to_toon", f.path, err)
				}
				continue
			}
			if _, err := toongo.MarshalString(val); err == nil {
				opsD++
				bytesD += float64(len(f.json))
			} else if r == 0 {
				logFail("toon-go", "json_to_toon", f.path, err)
			}
		}
	}
	record("toon-go", "json_to_toon", bytesD, opsD, time.Since(t0).Seconds(), len(files)*repeats)

	t0 = time.Now()
	for r := 0; r < repeats; r++ {
		for _, f := range files {
			if f.toon == "" {
				continue
			}
			val, err := toongo.DecodeString(f.toon)
			if err != nil {
				if r == 0 {
					logFail("toon-go", "toon_to_json", f.path, err)
				}
				continue
			}
			if _, err := json.Marshal(val); err == nil {
				opsE++
				bytesE += float64(len(f.toon))
			} else if r == 0 {
				logFail("toon-go", "toon_to_json", f.path, err)
			}
		}
	}
	record("toon-go", "toon_to_json", bytesE, opsE, time.Since(t0).Seconds(), len(files)*repeats)

	// toon-go roundtrip: json -> toon -> (parse toon) -> json, chained as
	// one operation per file.
	var opsF int
	var bytesF float64
	t0 = time.Now()
	for r := 0; r < repeats; r++ {
		for _, f := range files {
			var val interface{}
			if err := json.Unmarshal([]byte(f.json), &val); err != nil {
				if r == 0 {
					logFail("toon-go", "roundtrip", f.path, err)
				}
				continue
			}
			toon, err := toongo.MarshalString(val)
			if err != nil {
				if r == 0 {
					logFail("toon-go", "roundtrip", f.path, err)
				}
				continue
			}
			val2, err := toongo.DecodeString(toon)
			if err != nil {
				if r == 0 {
					logFail("toon-go", "roundtrip", f.path, err)
				}
				continue
			}
			if _, err := json.Marshal(val2); err == nil {
				opsF++
				bytesF += float64(len(f.json))
			} else if r == 0 {
				logFail("toon-go", "roundtrip", f.path, err)
			}
		}
	}
	record("toon-go", "roundtrip", bytesF, opsF, time.Since(t0).Seconds(), len(files)*repeats)

	// ── toon_to_struct / struct_to_toon ──
	// ctoon
	var opsTS, opsST int
	var bytesTS, bytesST float64
	t0 = time.Now()
	for r := 0; r < repeats; r++ {
		for _, f := range files {
			if f.toon == "" {
				continue
			}
			if _, err := ctoon.Loads(f.toon); err == nil {
				opsTS++
				bytesTS += float64(len(f.toon))
			} else if r == 0 {
				logFail("ctoon", "toon_to_struct", f.path, err)
			}
		}
	}
	record("ctoon", "toon_to_struct", bytesTS, opsTS, time.Since(t0).Seconds(), len(files)*repeats)

	t0 = time.Now()
	for r := 0; r < repeats; r++ {
		for _, f := range files {
			if !f.hasVal {
				continue
			}
			if _, err := ctoon.Dumps(f.val); err == nil {
				opsST++
				bytesST += float64(len(f.json))
			} else if r == 0 {
				logFail("ctoon", "struct_to_toon", f.path, err)
			}
		}
	}
	record("ctoon", "struct_to_toon", bytesST, opsST, time.Since(t0).Seconds(), len(files)*repeats)

	// gotoon (encode-only: struct_to_toon only)
	var opsGT int
	var bytesGT float64
	t0 = time.Now()
	for r := 0; r < repeats; r++ {
		for _, f := range files {
			if !f.hasPlain {
				continue
			}
			if _, err := gotoon.Encode(f.plain); err == nil {
				opsGT++
				bytesGT += float64(len(f.json))
			} else if r == 0 {
				logFail("gotoon", "struct_to_toon", f.path, err)
			}
		}
	}
	record("gotoon", "struct_to_toon", bytesGT, opsGT, time.Since(t0).Seconds(), len(files)*repeats)

	// toon-go
	var opsTGS, opsTGT int
	var bytesTGS, bytesTGT float64
	t0 = time.Now()
	for r := 0; r < repeats; r++ {
		for _, f := range files {
			if f.toon == "" {
				continue
			}
			if _, err := toongo.DecodeString(f.toon); err == nil {
				opsTGS++
				bytesTGS += float64(len(f.toon))
			} else if r == 0 {
				logFail("toon-go", "toon_to_struct", f.path, err)
			}
		}
	}
	record("toon-go", "toon_to_struct", bytesTGS, opsTGS, time.Since(t0).Seconds(), len(files)*repeats)

	t0 = time.Now()
	for r := 0; r < repeats; r++ {
		for _, f := range files {
			if !f.hasPlain {
				continue
			}
			if _, err := toongo.MarshalString(f.plain); err == nil {
				opsTGT++
				bytesTGT += float64(len(f.json))
			} else if r == 0 {
				logFail("toon-go", "struct_to_toon", f.path, err)
			}
		}
	}
	record("toon-go", "struct_to_toon", bytesTGT, opsTGT, time.Since(t0).Seconds(), len(files)*repeats)

	// ── write results JSON ──
	out := struct {
		Language string `json:"language"`
		Corpus   struct {
			Files int `json:"files"`
			Bytes int `json:"bytes"`
		} `json:"corpus"`
		Results    []result      `json:"results"`
		OrderCheck []orderResult `json:"order_check"`
	}{Language: "go"}
	out.Corpus.Files = len(files)
	out.Corpus.Bytes = totalJSONBytes
	out.Results = results

	// Key order, per library and direction. gotoon has no TOON decoder, so it
	// only gets json_to_toon. The peers go through encoding/json's map (as in
	// the timed rows above), which is exactly where Go loses key order.
	out.OrderCheck = append(out.OrderCheck, checkOrder("ctoon",
		func(s string) (string, error) {
			t, err := ctoon.JSONToToon(s, ctoon.DefaultEncodeOptions())
			return string(t), err
		},
		func(s string) (string, error) {
			j, err := ctoon.ToonToJSON(s, 0)
			return string(j), err
		})...)
	out.OrderCheck = append(out.OrderCheck, checkOrder("gotoon",
		func(s string) (string, error) {
			v, err := jsonToValue(s)
			if err != nil {
				return "", err
			}
			t, err := gotoon.Encode(v)
			return string(t), err
		}, nil)...)
	out.OrderCheck = append(out.OrderCheck, checkOrder("toon-go",
		func(s string) (string, error) {
			v, err := jsonToValue(s)
			if err != nil {
				return "", err
			}
			return toongo.MarshalString(v)
		},
		func(s string) (string, error) {
			v, err := toongo.DecodeString(s)
			if err != nil {
				return "", err
			}
			j, err := json.Marshal(v)
			return string(j), err
		})...)

	f, err := os.Create(resultsPath)
	if err != nil {
		fmt.Fprintf(os.Stderr, "warning: could not write %s: %v\n", resultsPath, err)
		return
	}
	defer f.Close()
	enc := json.NewEncoder(f)
	enc.SetIndent("", "  ")
	if err := enc.Encode(out); err != nil {
		fmt.Fprintf(os.Stderr, "warning: could not encode results: %v\n", err)
		return
	}
	fmt.Printf("\nResults written to %s\n", resultsPath)
	if logFile != nil {
		fmt.Printf("Debug log written to %s\n", os.Args[3])
	}
}
