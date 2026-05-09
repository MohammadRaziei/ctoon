# CLI Reference

The `ctoon` command-line tool converts between JSON and TOON formats.
It auto-detects the conversion direction from the file extension —
`.json` files are encoded to TOON, `.toon` files are decoded to JSON.

## Synopsis

```bash
ctoon [OPTIONS] <input>
ctoon [OPTIONS] -e <input>   # force encode  JSON → TOON
ctoon [OPTIONS] -d <input>   # force decode  TOON → JSON
```

## Quick start

```bash
# Install
pip install ctoon

# Convert JSON → TOON
ctoon input.json

# Convert TOON → JSON
ctoon input.toon -o output.json -i 4
```
