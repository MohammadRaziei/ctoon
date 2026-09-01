// CToon vs gotoon (github.com/alpkeskin/gotoon, community).
//
// gotoon only implements encoding (Go value -> TOON string) — it has no
// decoder at all — so this only compares the JSON -> TOON direction.
//
// A separate module for the same reason as vs_toongo: keeps the root
// module's go.mod free of a dependency it doesn't need. The `replace`
// below points back at the repo root for the ctoon import.
package main

import (
	"bufio"
	"encoding/json"
	"fmt"
	"os"
	"time"

	"github.com/alpkeskin/gotoon"
	"github.com/mohammadraziei/ctoon"
)

const repeats = 20

type benchFile struct {
	path string
	json string
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
		fmt.Fprintln(os.Stderr, "usage: vs_gotoon <manifest>")
		os.Exit(1)
	}

	files, err := loadCorpus(os.Args[1])
	if err != nil || len(files) == 0 {
		fmt.Fprintln(os.Stderr, "Corpus manifest is empty or unreadable.")
		os.Exit(1)
	}

	fmt.Println("CToon Go Benchmark — vs. gotoon (community, encode-only: no decoder)")
	fmt.Printf("Corpus: %d files\n\n", len(files))

	fmt.Printf("%-14s %12s %14s %9s %10s %12s\n", "Operation", "Throughput", "Docs/sec", "Success", "", "Total time")

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
	report("JSON -> TOON", resA, len(files))

	// -- gotoon: JSON -> TOON --
	fmt.Println("\nvs gotoon")
	var resB result
	t0 = time.Now()
	for r := 0; r < repeats; r++ {
		for _, f := range files {
			var val interface{}
			if err := json.Unmarshal([]byte(f.json), &val); err != nil {
				continue
			}
			if _, err := gotoon.Encode(val); err == nil {
				resB.ops++
				resB.bytes += int64(len(f.json))
			}
		}
	}
	resB.seconds = time.Since(t0).Seconds()
	report("JSON -> TOON", resB, len(files))
}
