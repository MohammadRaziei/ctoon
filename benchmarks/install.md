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
| Zig     | 0.16.0 + 0.15.2 | installed by the build from PyPI (`ziglang`) with `uv`, one venv each |
| Julia   | 1.13.2  | `pip install julialang` + `julialang install` |
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

## 2. Zig (via PyPI, installed by the build)

Nothing to install by hand any more: the CMake build creates the venvs and
runs `uv pip install ziglang==<version>` in each. It needs `uv` and network
access to PyPI (or a mirror, see `CTOON_PYPI_INDEX`). Two versions are used
because the two Zig benchmarks need different compilers, and pip cannot put
two versions of one package into one environment:

| Benchmark   | Zig    | venv (under the build dir) |
|-------------|--------|----------------------------|
| `zig/ctoon/`         | 0.16.0 | `venv_zig_0.16.0` |
| `zig/zig-toon/`      | 0.15.2 | `venv_zig_0.15.2` |

Each venv has its own Zig cache (`zig_cache_<version>/`) and is separate from
the Python benchmark's venv. A Zig on `PATH` is not used. The compiler
inside is `<build-bench>/venv_zig_<version>/bin/python-zig`. (This section
describes how the build is meant to work; it has not been run end to end.)

### Running the Zig benchmark

Normally `cmake --build build-bench --target ctoon_benchmarks_zig` is all
there is to it: it runs `zig fetch --save` to pull ctoon from GitHub, then
`zig build ... run`. On a machine whose network goes through an HTTPS
proxy, Zig's own HTTP client may not get through it: `zig fetch` then
fails with `unable to discover remote git server capabilities: ReadFailed`
(or hangs) even though `git` and `pip` work. Nothing in the project can fix
that. What was run instead, from a scratch copy so the repository stays
untouched:

```bash
S=$(mktemp -d)
cp -r benchmarks/zig/ctoon/. $S/ && ln -s "$PWD" $S/ctoon-src
# in $S/build.zig.zon, replace the whole `.ctoon = .{ .url = ..., .hash = ... }`
# entry with a local path (Zig only accepts a path relative to the build root):
#     .ctoon = .{ .path = "ctoon-src" },
cd $S
LD_LIBRARY_PATH=/usr/lib/x86_64-linux-gnu \
python-zig build --libc <build-bench>/zig/ctoon/zig-libc.txt \
  --search-prefix /usr/lib/x86_64-linux-gnu --release=fast run -- \
  <build-bench>/corpus_manifest.txt <build-bench>/results/zig.json
```

`<build-bench>/zig/ctoon/zig-libc.txt` is the libc paths file CMake wrote during
configure. The benchmark then runs on the local checkout instead of the
GitHub master, which is the one difference from the normal path.

## 3. Julia (via the `julialang` PyPI package)

[`julialang`](https://github.com/MohammadRaziei/julialang) is a
pure-Python CLI (no dependencies) that downloads the official Julia build
from the CI mirror in `MohammadRaziei/julialang` and verifies its
sha256/md5.

```bash
pip install --break-system-packages julialang
julialang install                 # latest Julia, becomes the default
julia --version                   # julia version 1.13.2
julia -e 'println(Sys.ARCH, " ", VERSION)'    # x86_64 1.13.2
```

Julia lives in `~/.julialang/versions/<version>/`; `/usr/local/bin/julia` is
a shim that picks the default version. Other versions:

```bash
julialang install 1.12            # newest 1.12.x
julialang install 1.10.5          # exactly this version
julialang install 1.10 --no-default
JULIALANG_VERSION=1.10 julia -e 'print(VERSION)'
```

`julialang list` / `julialang use <version>` / `julialang uninstall <version>`
manage installed versions. If `julia` is not on `PATH`, use
`python3 -m julialang run -e '...'`. Set `JULIALANG_REPO` to use a fork's
mirror.

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
python-zig version
julia --version
octave --version | head -1
```
