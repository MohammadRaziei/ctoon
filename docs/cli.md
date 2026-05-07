# CLI Reference

The `ctoon` command-line tool converts between JSON and TOON formats.
It auto-detects the conversion direction from the file extension —
`.json` files are encoded to TOON, `.toon` files are decoded to JSON.

## Installation

### Option 1 — pip (recommended)

The easiest way. The Python package ships a pre-built `ctoon` binary
alongside the Python extension — no compiler or CMake needed.

```bash
pip install ctoon
```

After install, `ctoon` is immediately available in your shell:

```bash
ctoon --version
```

### Option 2 — build with CMake

If you need a custom build configuration or are on a platform without
a pre-built wheel, you can build from source:

```bash
git clone https://github.com/MohammadRaziei/ctoon.git
cd ctoon
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# Install to system (adds `ctoon` to PATH)
sudo cmake --install build
```

Requires CMake 3.19+ and a C99-compatible compiler (gcc / clang / MSVC).

## Synopsis

```bash
ctoon [OPTIONS] <input>
ctoon [OPTIONS] -e <input>   # force encode  JSON → TOON
ctoon [OPTIONS] -d <input>   # force decode  TOON → JSON
```

## Options

| Option | Short | Description |
|---|---|---|
| `--output <file>` | `-o` | Write output to `<file>` instead of stdout |
| `--indent <n>` | `-i` | Indentation level for output (default: 0) |
| `--encode` | `-e` | Force encode mode: JSON → TOON |
| `--decode` | `-d` | Force decode mode: TOON → JSON |
| `--delimiter <type>` | | Field delimiter: `comma` (default), `tab`, `pipe` |
| `--length-marker` | | Emit length markers for arrays: `items[#3]: …` |
| `--stats` | | Print byte-savings summary to stderr |
| `--help` | `-h` | Show help message and exit |
| `--version` | `-v` | Print version and exit |

## Examples

### Basic conversion

```bash
# JSON → TOON (auto-detected)
ctoon input.json

# TOON → JSON (auto-detected)
ctoon input.toon

# Explicit output file
ctoon input.json -o output.toon
```

### Pretty-printed JSON output

```bash
ctoon input.toon -o output.json --indent 4
```

### Read from stdin

```bash
cat data.json | ctoon -e -
echo '{"name":"Alice"}' | ctoon -e -
```

### Custom delimiter

```bash
# Use pipe as list delimiter
ctoon input.json --delimiter pipe

# Use tab as list delimiter
ctoon input.json --delimiter tab
```

### Length markers

Useful when passing TOON to an LLM that benefits from knowing array sizes upfront:

```bash
ctoon input.json --length-marker
```

Output example:
```
items[#3]:
  - Alice
  - Bob
  - Carol
```

### Stats output

```bash
ctoon input.json --stats
```

Prints a summary to stderr:
```
Input  (JSON): 1024 bytes
Output (TOON):  612 bytes
Savings:         40.2%
```

## Exit codes

| Code | Meaning |
|---|---|
| `0` | Success |
| `1` | Input file not found or unreadable |
| `2` | Parse error in input |
| `3` | Write error on output |