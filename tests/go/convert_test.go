package ctoon_test

import (
	"encoding/json"
	"os"
	"path/filepath"
	"reflect"
	"strings"
	"testing"

	ctoon "github.com/mohammadraziei/ctoon"
)

// Expected values come only from independent sources: hand-written strings,
// paired .toon fixtures, and encoding/json (never the ctoon CLI).

func TestJSONToToonBasic(t *testing.T) {
	got, err := ctoon.JSONToToon(`{"name":"Alice","age":30,"tags":["a","b"]}`, ctoon.DefaultEncodeOptions())
	if err != nil {
		t.Fatal(err)
	}
	if want := "name: Alice\nage: 30\ntags[2]: a,b"; got != want {
		t.Fatalf("got %q want %q", got, want)
	}
	tab, _ := ctoon.JSONToToon(`{"u":[{"id":1,"n":"x"},{"id":2,"n":"y"}]}`, ctoon.DefaultEncodeOptions())
	if want := "u[2]{id,n}:\n  1,x\n  2,y"; tab != want {
		t.Fatalf("got %q want %q", tab, want)
	}
	o := ctoon.DefaultEncodeOptions()
	o.Delimiter = ctoon.DelimiterPipe
	pipe, _ := ctoon.JSONToToon(`{"t":[1,2,3]}`, o)
	if pipe != "t[3|]: 1|2|3" {
		t.Fatalf("pipe: %q", pipe)
	}
}

func TestToonToJSONBasic(t *testing.T) {
	got, err := ctoon.ToonToJSON("name: Alice\nage: 30\ntags[2]: a,b", 0)
	if err != nil {
		t.Fatal(err)
	}
	if want := `{"name":"Alice","age":30,"tags":["a","b"]}`; got != want {
		t.Fatalf("got %q want %q", got, want)
	}
	pretty, _ := ctoon.ToonToJSON("a: 1", 2)
	if pretty != "{\n  \"a\": 1\n}" {
		t.Fatalf("pretty: %q", pretty)
	}
}

func TestConvertErrors(t *testing.T) {
	if _, err := ctoon.JSONToToon(`{"a":`, ctoon.DefaultEncodeOptions()); err == nil {
		t.Fatal("expected parse error")
	}
	if _, err := ctoon.JSONToToon("", ctoon.DefaultEncodeOptions()); err == nil {
		t.Fatal("expected error on empty input")
	}
	if _, err := ctoon.ToonToJSON("a[2]: 1", 0); err == nil {
		t.Fatal("expected strict length-mismatch error")
	}
}

func TestConvertFiles(t *testing.T) {
	dir := t.TempDir()
	jp := filepath.Join(dir, "in.json")
	tp := filepath.Join(dir, "out.toon")
	bp := filepath.Join(dir, "back.json")
	os.WriteFile(jp, []byte(`{"k":[1,2],"s":"hi"}`), 0o644)
	if err := ctoon.JSONToToonFile(jp, tp, ctoon.DefaultEncodeOptions()); err != nil {
		t.Fatal(err)
	}
	if b, _ := os.ReadFile(tp); string(b) != "k[2]: 1,2\ns: hi" {
		t.Fatalf("toon file: %q", b)
	}
	if err := ctoon.ToonToJSONFile(tp, bp, 0); err != nil {
		t.Fatal(err)
	}
	if b, _ := os.ReadFile(bp); string(b) != `{"k":[1,2],"s":"hi"}` {
		t.Fatalf("json file: %q", b)
	}
	bad := filepath.Join(dir, "bad.json")
	os.WriteFile(bad, []byte(`{"a":`), 0o644)
	never := filepath.Join(dir, "never.toon")
	if err := ctoon.JSONToToonFile(bad, never, ctoon.DefaultEncodeOptions()); err == nil {
		t.Fatal("expected error")
	}
	if _, err := os.Stat(never); err == nil {
		t.Fatal("failed conversion must not create the output file")
	}
}

func checkConvertPair(t *testing.T, jsonPath string) {
	t.Helper()
	toonPath := strings.TrimSuffix(jsonPath, ".json") + ".toon"
	rawJSON, err1 := os.ReadFile(jsonPath)
	rawToon, err2 := os.ReadFile(toonPath)
	if err1 != nil || err2 != nil {
		return
	}
	want := strings.TrimRight(string(rawToon), "\n")
	got, err := ctoon.JSONToToon(string(rawJSON), ctoon.DefaultEncodeOptions())
	if err != nil {
		t.Errorf("%s: %v", jsonPath, err)
		return
	}
	if strings.TrimRight(got, "\n") != want {
		t.Errorf("%s: JSONToToon != paired .toon", filepath.Base(jsonPath))
	}
	back, err := ctoon.ToonToJSON(want, 0)
	if err != nil {
		t.Errorf("%s: %v", jsonPath, err)
		return
	}
	var a, b interface{}
	if json.Unmarshal(rawJSON, &a) != nil || json.Unmarshal([]byte(back), &b) != nil {
		t.Errorf("%s: unmarshal failed", jsonPath)
		return
	}
	if !reflect.DeepEqual(a, b) {
		t.Errorf("%s: ToonToJSON value differs from source JSON", filepath.Base(jsonPath))
	}
}

func TestConvertLocalCorpus(t *testing.T) {
	files, _ := filepath.Glob(filepath.Join(testDataDir(t), "*.json"))
	for _, f := range files {
		checkConvertPair(t, f)
	}
}

func TestConvertSpecExamples(t *testing.T) {
	dir := specExamplesDir(t)
	files, _ := filepath.Glob(filepath.Join(dir, "*.json"))
	if len(files) == 0 {
		t.Fatal("no spec examples")
	}
	for _, f := range files {
		checkConvertPair(t, f)
	}
}
