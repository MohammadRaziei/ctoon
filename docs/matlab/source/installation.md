# Installation

## Requirements

- MATLAB R2014b or newer (for MEX compilation)
- A C compiler configured for MATLAB (`mex -setup C`)
- MATLAB R2022b or newer for `buildtool` support

## Quick install

Clone or download the CToon repository, then from MATLAB:

```matlab
cd src/bindings/matlab
ctoon_install
```

`ctoon_install` compiles the MEX gateway and permanently adds the binding
to your MATLAB path via `savepath`.

## Manual build

```matlab
cd src/bindings/matlab
ctoon_build
addpath(pwd)
```

## Build with CMake

```bash
cmake -B build -DCTOON_BUILD_MATLAB=ON -DMatlab_ROOT_DIR=/path/to/matlab
cmake --build build --target ctoon_mex
```

## Self-contained export

To distribute the binding without requiring a full CToon build environment:

```bash
cmake \
  -DEXPORT_DIR=/tmp/ctoon-matlab \
  -DMEX_SOURCES_PATH=/path/to/ctoon/src \
  -DMEX_INCLUDE_DIR=/path/to/ctoon/include \
  -P src/bindings/matlab/CToonMatlabExport.cmake
```

The export directory contains everything needed — `ctoon.c`, `ctoon.h`,
`ctoon_mex.c`, and all `.m` wrappers. No CMake or git required on the
target machine.

## GNU Octave

The same MEX gateway and `+ctoon` wrappers also run under GNU Octave 8.1 or
newer. There is **no prebuilt Octave binary**: you build once from source with
`mkoctfile`, and only that build needs a C compiler. Using the result (and
running the tests) needs nothing but Octave.

- Octave 8.1+ with its development files (Debian/Ubuntu: `apt install octave octave-dev`;
  Homebrew: `brew install octave`)
- A C compiler (only to build)

```octave
cd src/bindings/matlab
ctoon_build('/path/to/ctoon-octave')   % compiles with mkoctfile
addpath('/path/to/ctoon-octave')       % that one folder is all Octave needs
```

`ctoon_install` works too, but Octave's `savepath` stores your *whole*
current path in `~/.octaverc`, not just CToon's. To make the binding permanent
more tidily, put the `addpath(...)` line above into `~/.octaverc` yourself.

Without a checkout, `ctoon.manager` builds it from a release's MATLAB source
distribution (`ctoon-matlab-<version>-src.zip`):

```bash
python -m ctoon.manager octave detect    # Octave version, mkoctfile, compiler
python -m ctoon.manager octave install   # download + build; prints the addpath line
```

With CMake: `cmake -B build -DCTOON_BUILD_OCTAVE=ON`, then
`ctest --test-dir build -R octave` (tests use [MOxUnit](https://github.com/MOxUnit/MOxUnit)).

## Verify

```matlab
ctoon.encode(struct('hello', 'world'))
```
