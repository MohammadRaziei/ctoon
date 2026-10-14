# Installing the benchmark toolchains

Every command below was actually run, in this order, on a clean
Ubuntu 24.04.5 LTS (x86_64) container as `root`. Versions are the ones that
ended up installed.

| Tool    | Version | How                         |
|---------|---------|-----------------------------|
| gcc/g++ | 13.3.0  | preinstalled                |
| make    | 4.3     | preinstalled                |
| cmake   | 3.28.3  | preinstalled (`/usr/bin/cmake`) |
| Go      | 1.24.7  | preinstalled (`/usr/local/go`)  |
| Rust    | 1.97.0  | preinstalled (`~/.cargo/bin`)   |
| Zig     | 0.17.0  | `pip install ziglang`       |
| Julia   | 1.13.1  | release tarball (see below) |
| Octave  | 8.4.0   | `apt-get install octave octave-dev` |

## 0. Get the sources

```bash
GIT_LFS_SKIP_SMUDGE=1 git clone --depth 1 https://github.com/MohammadRaziei/ctoon ~/ctoon
```

## 1. Check what is already there

```bash
for t in cmake go rustc cargo octave gcc g++ make apt-get conda; do
  printf "%-9s" $t; (command -v $t && $t --version 2>&1 | head -1) || echo "-"
done
go version        # `go --version` is not valid, Go uses `go version`
```

Result: cmake, Go and Rust were already installed. Octave was missing.

## 2. Zig (via PyPI)

```bash
pip install --break-system-packages ziglang
python3 -m ziglang version     # 0.17.0
```

`--break-system-packages` is needed because pip is on the system Python
(PEP 668). Zig is invoked as `python3 -m ziglang ...`.

## 3. Julia (from MohammadRaziei/julialang releases)

The release tag is `__download__` (not `download`; that URL returns 404).

```bash
# Index of all builds: 33 versions, 1.10.0 .. 1.13.1, 7 platforms
curl -sSL -o julia-list.json \
  https://github.com/MohammadRaziei/julialang/releases/download/__download__/julia-list.json

# Pick linux-x86_64 of the "latest" version from the index
python3 -I - <<'EOF'
import json
d = json.load(open("julia-list.json"))
f = d["versions"][d["latest"]]["files"]["linux-x86_64"]
print(f["name"], f["url"], f["size"], f["sha256"])
EOF

# Download, verify, install
curl -sSL -o julia.tar.gz \
  https://github.com/MohammadRaziei/julialang/releases/download/__download__/julia-1.13.1-linux-x86_64.tar.gz
echo "0f2e18c8dea60a2c8711d089cd9612f7a8df394b89e5208096fe5867647e0908  julia.tar.gz" | sha256sum -c -

mkdir -p /opt/julia
tar -xzf julia.tar.gz -C /opt/julia --strip-components=1
ln -sf /opt/julia/bin/julia /usr/local/bin/julia

julia --version
julia -e 'println(Sys.ARCH, " ", VERSION)'    # x86_64 1.13.1
```

## 4. GNU Octave (apt)

```bash
apt-get update            # the download.docker.com repo may return 403 here; harmless
DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends octave octave-dev

octave --version | head -1        # GNU Octave, version 8.4.0
mkoctfile --version               # 8.4.0
octave-cli --no-gui --eval 'disp(sum(1:10))'    # 55
```

Notes:

- The development package is called `octave-dev` on Ubuntu. `liboctave-dev`
  does not exist there ("Unable to locate package"). `octave-dev` provides
  `mkoctfile`, which is what builds the MEX gateway.
- Headless runs print `octave: X11 DISPLAY environment variable not set` and
  disable GUI features. That is only a notice. Use `octave-cli` or
  `octave --no-gui` to avoid it.

## Verify everything

```bash
cmake --version | head -1
go version
rustc --version && cargo --version
python3 -m ziglang version
julia --version
octave --version | head -1
```
