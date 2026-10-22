#!/usr/bin/env bash
# Builds ../text_cache.dll (32-bit, no runtime DLLs) with i686-w64-mingw32-g++.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
i686-w64-mingw32-g++ -O2 -std=c++17 -shared -static -s -Wall \
    -o "$here/../text_cache.dll" "$here/text_cache.cpp"
