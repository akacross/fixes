# Sourced by the fixes native tests (*_test.sh): the same script on Linux and
# on Windows (Git Bash / MSYS2).
#
#   Linux    builds with i686-w64-mingw32-gcc and runs the .exe under Wine in
#            a throwaway prefix, never the game's.
#   Windows  builds with a 32-bit MinGW gcc and runs the .exe directly - it is
#            a Windows program; there is no Wine to ask.
#
# Sets: CC (the compiler), PY (a Python that runs), ON_WINDOWS (1 or empty).
# Defines: run_exe <exe> [args...], winpath <path> (the path a Windows program
# reads: Z:\... under Wine, C:\... natively).
#
# No 32-bit MinGW gcc means the test is SKIPPED (exit 77, the automake skip
# code), not failed: on Windows that compiler is an install away
# (w64devkit's i686 build, or MSYS2's mingw-w64-i686-gcc).

case "$(uname -s)" in
    # Git Bash / MSYS2 / Cygwin, and Windows_NT: w64devkit's busybox uname
    MINGW*|MSYS*|CYGWIN*|Windows_NT) ON_WINDOWS=1 ;;
    *) ON_WINDOWS= ;;
esac

# The compiler: $CC when set, else the cross name, else a native gcc that
# targets 32-bit Windows (w64devkit-i686, MSYS2 mingw32 shell).
if [ -z "${CC:-}" ]; then
    if command -v i686-w64-mingw32-gcc >/dev/null 2>&1; then
        CC=i686-w64-mingw32-gcc
    elif command -v gcc >/dev/null 2>&1 && gcc -dumpmachine 2>/dev/null | grep -q '^i686-.*mingw32'; then
        CC=gcc
    fi
fi
if [ -z "${CC:-}" ]; then
    echo "skip: no 32-bit MinGW gcc (i686-w64-mingw32-gcc, or a gcc whose -dumpmachine is i686-*-mingw32); set CC to use another"
    exit 77
fi

# Build for the target the shipped DLLs are built for: plain i686 with x87
# maths, the default of Linux's i686-w64-mingw32-gcc. w64devkit's gcc is
# configured --with-arch=pentium4 --with-fpmath=sse, and crashfix_dll_test
# compiled that way failed 7 checks on Windows (doordamp1-4, bikestill x2,
# the ftol countdown) that pass with x87 maths - same gcc 16.2, same source.
# A test that wants SSE (fps_fixes_test) passes -msse2 -mfpmath=sse after
# these, and the later flags win. CC is a function so "$CC" keeps working.
CC_BIN=$CC
cc_i686() { "$CC_BIN" -march=i686 -mfpmath=387 "$@"; }
CC=cc_i686

# A Python that RUNS: on Windows python3 is often the Microsoft Store stub.
PY=
for py in python3 python py; do
    if command -v "$py" >/dev/null 2>&1 && "$py" -c 'import sys' >/dev/null 2>&1; then
        PY=$py
        break
    fi
done

if [ -z "$ON_WINDOWS" ]; then
    if ! command -v wine >/dev/null 2>&1; then
        echo "skip: no wine to run the 32-bit test program"
        exit 77
    fi
    # set by the caller after mktemp, but defaulted here so a test that
    # forgets cannot touch the game's prefix
    export WINEDEBUG=-all
fi

# run_exe <exe> [args...]: from the current directory.
run_exe() {
    local exe="$1"; shift
    if [ -n "$ON_WINDOWS" ]; then
        "./$exe" "$@"
    else
        wine "$exe" "$@"
    fi
}

# winpath <path>: an absolute POSIX path as the test program sees it.
winpath() {
    if [ -n "$ON_WINDOWS" ]; then
        cygpath -w "$1"
    else
        printf 'Z:%s' "${1//\//\\}"
    fi
}
