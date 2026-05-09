# Options

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

## Exit codes

| Code | Meaning |
|---|---|
| `0` | Success |
| `1` | Input file not found or unreadable |
| `2` | Parse error in input |
| `3` | Write error on output |
