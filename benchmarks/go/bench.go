// CToon Go benchmark.
//
// Every implementation here — ctoon included — is one more peer entry,
// fetched from its own GitHub module path, tested the same way. No
// "ctoon vs X" framing: one shared results table, one row per
// (library, operation) pair.
//
// Methodology (same across every language benchmark in this repo):
//  1. Load every file in the corpus manifest into memory (untimed).
//  2. Untimed pre-pass: convert each JSON file to TOON once (with ctoon).
//  3. Timed "json_to_toon": repeatedly parse JSON and re-serialise to TOON.
//  4. Timed "toon_to_json": repeatedly parse the TOON text from step 2 and
//     re-serialise to JSON.
//  5. Report throughput (MB/s of bytes actually read by successful
//     conversions only) and documents/sec.
//
// Implementations:
//   - ctoon    this project (github.com/mohammadraziei/ctoon)
//   - gotoon   github.com/alpkeskin/gotoon (community; encode-only, no decoder)
//   - toon-go  github.com/toon-format/toon-go (official; requires Go >= 1.23,
//              which is why this whole module's go.mod floor is 1.23 too —
//              no separate "vs" subdirectory to work around it)
package main

import (
	"bufio"
	"encoding/json"
	"fmt"
	"os"
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
}

type result struct {
	Library        string  `json:"library"`
	Operation      string  `json:"operation"`
	ThroughputMBs  float64 `json:"throughput_mb_s"`
	DocsPerSec     float64 `json:"docs_per_sec"`
	SuccessRate    float64 `json:"success_rate"`
	TotalTimeS     float64 `json:"total_time_s"`
}

var results []result

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

func main() {
	if len(os.Args) < 3 {
		fmt.Fprintln(os.Stderr, "usage: bench <manifest> <results.json>")
		os.Exit(1)
	}
	manifestPath, resultsPath := os.Args[1], os.Args[2]

	files, err := loadCorpus(manifestPath)
	if err != nil || len(files) == 0 {
		fmt.Fprintln(os.Stderr, "Corpus manifest is empty or unreadable.")
		os.Exit(1)
	}

	totalJSONBytes := 0
	for _, f := range files {
		totalJSONBytes += len(f.json)
	}

	fmt.Println("CToon Benchmarks — Go")
	fmt.Printf("Corpus: %d files, %.2f MB (JSON)\n\n", len(files), float64(totalJSONBytes)/1e6)
	fmt.Printf("%-8s %-14s %12s %14s %9s %10s %12s\n",
		"Library", "Operation", "Throughput", "Docs/sec", "Success", "", "Total time")

	// Untimed pre-pass: TOON text with ctoon, shared decode input for all.
	totalTOONBytes := 0
	for _, f := range files {
		val, err := ctoon.LoadsJSON(f.json)
		if err != nil {
			continue
		}
		toon, err := ctoon.Dumps(val)
		if err != nil {
			continue
		}
		f.toon = toon
		totalTOONBytes += len(toon)
	}

	// ── ctoon ──
	var opsA, opsB int
	var bytesA, bytesB float64
	t0 := time.Now()
	for r := 0; r < repeats; r++ {
		for _, f := range files {
			val, err := ctoon.LoadsJSON(f.json)
			if err != nil {
				continue
			}
			if _, err := ctoon.Dumps(val); err == nil {
				opsA++
				bytesA += float64(len(f.json))
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
			val, err := ctoon.Loads(f.toon)
			if err != nil {
				continue
			}
			if _, err := ctoon.DumpsJSON(val, 2); err == nil {
				opsB++
				bytesB += float64(len(f.toon))
			}
		}
	}
	record("ctoon", "toon_to_json", bytesB, opsB, time.Since(t0).Seconds(), len(files)*repeats)

	// ── gotoon (encode-only: no decoder) ──
	var opsC int
	var bytesC float64
	t0 = time.Now()
	for r := 0; r < repeats; r++ {
		for _, f := range files {
			var val interface{}
			if err := json.Unmarshal([]byte(f.json), &val); err != nil {
				continue
			}
			if _, err := gotoon.Encode(val); err == nil {
				opsC++
				bytesC += float64(len(f.json))
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
				continue
			}
			if _, err := toongo.MarshalString(val); err == nil {
				opsD++
				bytesD += float64(len(f.json))
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
				continue
			}
			if _, err := json.Marshal(val); err == nil {
				opsE++
				bytesE += float64(len(f.toon))
			}
		}
	}
	record("toon-go", "toon_to_json", bytesE, opsE, time.Since(t0).Seconds(), len(files)*repeats)

	// ── write results JSON ──
	out := struct {
		Language string `json:"language"`
		Corpus   struct {
			Files int `json:"files"`
			Bytes int `json:"bytes"`
		} `json:"corpus"`
		Results []result `json:"results"`
	}{Language: "go"}
	out.Corpus.Files = len(files)
	out.Corpus.Bytes = totalJSONBytes
	out.Results = results

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
}
