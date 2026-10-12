# GNU Octave: Installation

GNU Octave uses the **same binding as MATLAB**: the same `ctoon_mex.c` gateway
and the same `+ctoon` wrappers, compiled with `mkoctfile` instead of MATLAB's
`mex`. This page only covers how to install it. Everything else (usage, type
mapping, file I/O, the API reference) is identical and lives in the MATLAB
documentation:

- [MATLAB overview](../matlab/index.html)
- [Usage](../matlab/usage.html)
- [API reference](../matlab/api.html)

## Requirements

- GNU Octave 8.1 or newer.
- Octave's development files, which provide `mkoctfile` (Debian/Ubuntu:
  `apt install octave octave-dev`; Homebrew: `brew install octave`).
- A C compiler.

There is **no prebuilt Octave binary**: you build the binding once, from
source. Only that build needs `mkoctfile` and a compiler. Using the result
needs nothing but Octave.

## Install with `ctoon.manager`

If you have the Python package, no checkout is needed:

```bash
python -m ctoon.manager octave detect    # Octave version, mkoctfile, compiler
python -m ctoon.manager octave install   # download, build, self-check
```

`install` downloads the MATLAB source distribution of the release
(`ctoon-matlab-<version>-src.zip`), builds it with `mkoctfile` and prints the
`addpath(...)` line to use. Options:

| Option | Meaning |
|--------|---------|
| `--prefix DIR` | Where to install (default: `<python prefix>/share/ctoon/octave`). |
| `--tag TAG` | Install a specific release, e.g. `v0.8.3`. |
| `--beta` | Use the `__beta__` development pre-release (unstable). |
| `--zip FILE` | Build from a local `ctoon-matlab-*-src.zip` instead of downloading. |

## Build from a checkout

```matlab
cd src/bindings/matlab
ctoon_build('/path/to/ctoon-octave')   % compiles with mkoctfile
addpath('/path/to/ctoon-octave')       % that one folder is all Octave needs
```

The build directory holds `ctoon_mex.mex` next to the `+ctoon` package. (Octave
does not look inside a `private/` folder of a `+package`, which is where the
MATLAB build puts the gateway.)

## Make it permanent

Put the `addpath(...)` line in `~/.octaverc`.

`ctoon_install` also works under Octave, but Octave's `savepath` stores your
*whole* current path in `~/.octaverc`, not just CToon's, so the one-line
`addpath` is tidier.

## Verify

```matlab
ctoon.encode(struct('hello', 'world'))
```

## Build and test with CMake

```bash
cmake -B build -DCTOON_BUILD_OCTAVE=ON
cmake --build build
ctest --test-dir build -R octave --output-on-failure
```

The tests use [MOxUnit](https://github.com/MOxUnit/MOxUnit), which CMake
fetches for you. They load the already-built binding and compile nothing.

## Next

Usage and the full API are the MATLAB ones: see the
[MATLAB documentation](../matlab/index.html).
