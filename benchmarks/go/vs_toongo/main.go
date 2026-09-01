// CToon vs toon-go (github.com/toon-format/toon-go, official, in development).
//
// Same methodology as benchmarks/go/bench_ctoon.go (see benchmarks/README.md):
// JSON -> TOON and TOON -> JSON, x20 reps, over the shared corpus manifest.
// toon-go has no JSON codec of its own, so encoding/json is used for the
// JSON side on both sides of the comparison.
//
// A separate module on purpose: toon-go requires Go >= 1.23, which would
// force that floor onto the whole repo's go.mod if this lived in the same
// module as the root package (used by the actual ctoon Go binding/tests).
// The `replace` below points back at the repo root for the ctoon import.
package main

import (
	"bufio"
	"encoding/json"
	"fmt"
	"os"
	"time"

	"github.com/mohammadraziei/ctoon"
	toongo "github.com/toon-format/toon-go"
)

const repeats = 20

type benchFile struct {
	path string
	json string
	toon string // ctoon's own TOON output, shared decode input for both libs
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

type result struct {
	seconds float64
	ops     int
	bytes   int64
}

func report(label string, r result, filesTotal int) {
	throughput := 0.0
	if r.ops > 0 {
		throughput = float64(r.bytes) / r.seconds / 1e6
	}
	fmt.Printf("%-14s %9.2f MB/s %14.0f %8.0f%%  %8.4f s  (x%d reps)\n",
		label, throughput, float64(r.ops)/r.seconds,
		100*float64(r.ops)/(float64(filesTotal)*repeats), r.seconds, repeats)
}

func main() {
	if len(os.Args) < 2 {
		fmt.Fprintln(os.Stderr, "usage: vs_toongo <manifest>")
		os.Exit(1)
	}

	files, err := loadCorpus(os.Args[1])
	if err != nil || len(files) == 0 {
		fmt.Fprintln(os.Stderr, "Corpus manifest is empty or unreadable.")
		os.Exit(1)
	}

	// -- Untimed pre-pass: TOON text with ctoon, shared decode input --
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
	}

	fmt.Println("CToon Go Benchmark — vs. toon-go (official, requires Go >= 1.23)")
	fmt.Printf("Corpus: %d files\n\n", len(files))

	// -- ctoon: JSON -> TOON --
	var resA result
	t0 := time.Now()
	for r := 0; r < repeats; r++ {
		for _, f := range files {
			val, err := ctoon.LoadsJSON(f.json)
			if err != nil {
				continue
			}
			if _, err := ctoon.Dumps(val); err == nil {
				resA.ops++
				resA.bytes += int64(len(f.json))
			}
		}
	}
	resA.seconds = time.Since(t0).Seconds()

	// -- ctoon: TOON -> JSON --
	var resB result
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
				resB.ops++
				resB.bytes += int64(len(f.toon))
			}
		}
	}
	resB.seconds = time.Since(t0).Seconds()

	fmt.Printf("%-14s %12s %14s %9s %10s %12s\n", "Operation", "Throughput", "Docs/sec", "Success", "", "Total time")
	report("JSON -> TOON", resA, len(files))
	report("TOON -> JSON", resB, len(files))

	// -- toon-go: JSON -> TOON --
	fmt.Println("\nvs toon-go")
	var resC result
	t0 = time.Now()
	for r := 0; r < repeats; r++ {
		for _, f := range files {
			var val interface{}
			if err := json.Unmarshal([]byte(f.json), &val); err != nil {
				continue
			}
			if _, err := toongo.MarshalString(val); err == nil {
				resC.ops++
				resC.bytes += int64(len(f.json))
			}
		}
	}
	resC.seconds = time.Since(t0).Seconds()
	report("JSON -> TOON", resC, len(files))

	// -- toon-go: TOON -> JSON (parsing ctoon's TOON output, same input as resB) --
	var resD result
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
				resD.ops++
				resD.bytes += int64(len(f.toon))
			}
		}
	}
	resD.seconds = time.Since(t0).Seconds()
	report("TOON -> JSON", resD, len(files))
}
