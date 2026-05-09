# Examples

## Basic conversion

```bash
# JSON → TOON (auto-detected)
ctoon input.json

# TOON → JSON (auto-detected)
ctoon input.toon

# Explicit output file
ctoon input.json -o output.toon
```

## Pretty-printed JSON output

```bash
ctoon input.toon -o output.json --indent 4
```

## Read from stdin

```bash
cat data.json | ctoon -e -
echo '{"name":"Alice"}' | ctoon -e -
```

## Custom delimiter

```bash
# Use pipe as list delimiter
ctoon input.json --delimiter pipe

# Use tab as list delimiter
ctoon input.json --delimiter tab
```

## Length markers

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

## Stats output

```bash
ctoon input.json --stats
```

Prints a summary to stderr:

```
Input  (JSON): 1024 bytes
Output (TOON):  612 bytes
Savings:         40.2%
```
