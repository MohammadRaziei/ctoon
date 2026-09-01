// CToon Go benchmark.
//
// Same methodology as every other language benchmark in this repo (see
// benchmarks/README.md), so the numbers line up with the C, C++, and
// Python results for the same corpus:
//
//  1. Load every file in the corpus manifest into memory (untimed).
//  2. Untimed pre-pass: convert each JSON file to TOON once.
//  3. Timed "JSON -> TOON": repeatedly parse JSON and re-serialise to TOON.
//  4. Timed "TOON -> JSON": repeatedly parse TOON and re-serialise to JSON.
//  5. Report throughput (MB/s of the bytes actually read by that operation)
//     and documents/sec.
//
// This file has no CMakeLists-driven build step of its own — it's a plain
// Go program at benchmarks/go, resolved against the module root's go.mod
// (github.com/mohammadraziei/ctoon) exactly like tests/go does. Run it with:
//
//	go run ./benchmarks/go <path-to-manifest>
package main

import (
	"bufio"
	"fmt"
	"os"
	"time"

	"github.com/mohammadraziei/ctoon"
)

const repeats = 20

type benchFile struct {
	path string
	json string
	toon string
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
			continue // skip unreadable entries
		}
		files = append(files, &benchFile{path: path, json: string(data)})
	}
	return files, nil
}

func main() {
	if len(os.Args) < 2 {
		fmt.Fprintln(os.Stderr, "usage: bench_ctoon <manifest>")
		os.Exit(1)
	}

	files, err := loadCorpus(os.Args[1])
	if err != nil || len(files) == 0 {
		fmt.Fprintln(os.Stderr, "Corpus manifest is empty or unreadable - nothing to benchmark.")
		os.Exit(1)
	}

	totalJSONBytes := 0
	for _, f := range files {
		totalJSONBytes += len(f.json)
	}

	fmt.Println("CToon Go Benchmark")
	fmt.Printf("Corpus: %d files, %.2f MB (JSON)\n\n", len(files), float64(totalJSONBytes)/1e6)

	// -- Untimed pre-pass --
	totalTOONBytes := 0
	for _, f := range files {
		val, err := ctoon.LoadsJSON(f.json)
		if err != nil {
			continue // skip files that don't round-trip (e.g. scalar-only JSON)
		}
		toon, err := ctoon.Dumps(val)
		if err != nil {
			continue
		}
		f.toon = toon
		totalTOONBytes += len(toon)
	}

	// -- Phase A: JSON -> TOON --
	t0 := time.Now()
	opsA := 0
	for r := 0; r < repeats; r++ {
		for _, f := range files {
			val, err := ctoon.LoadsJSON(f.json)
			if err != nil {
				continue
			}
			if _, err := ctoon.Dumps(val); err == nil {
				opsA++
			}
		}
	}
	tJSONToTOON := time.Since(t0).Seconds()
	bytesA := float64(totalJSONBytes) * repeats

	// -- Phase B: TOON -> JSON --
	t0 = time.Now()
	opsB := 0
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
			}
		}
	}
	tTOONToJSON := time.Since(t0).Seconds()
	bytesB := float64(totalTOONBytes) * repeats

	// -- Report --
	fmt.Printf("%-14s %12s %14s %12s\n", "Operation", "Throughput", "Docs/sec", "Total time")
	fmt.Printf("%-14s %9.2f MB/s %14.0f %9.4f s  (x%d reps)\n",
		"JSON -> TOON", bytesA/tJSONToTOON/1e6, float64(opsA)/tJSONToTOON, tJSONToTOON, repeats)
	fmt.Printf("%-14s %9.2f MB/s %14.0f %9.4f s  (x%d reps)\n",
		"TOON -> JSON", bytesB/tTOONToJSON/1e6, float64(opsB)/tTOONToJSON, tTOONToJSON, repeats)
}
