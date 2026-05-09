# Installation

## Option 1 — pip (recommended)

The easiest way. The Python package ships a pre-built `ctoon` binary
alongside the Python extension — no compiler or CMake needed.

```bash
pip install ctoon
```

After install, `ctoon` is immediately available in your shell:

```bash
ctoon --version
```

## Option 2 — build with CMake

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
