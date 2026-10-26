#!/usr/bin/env bash
# Builds ../text_cache.dll and ../crashfix.dll (32-bit, no runtime DLLs) with mingw.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
i686-w64-mingw32-g++ -O2 -std=c++17 -shared -static -s -Wall \
    -o "$here/../text_cache.dll" "$here/text_cache.cpp"
i686-w64-mingw32-gcc -O2 -shared -static -s -Wall \
    -o "$here/../crashfix.dll" "$here/crashfix.c"
