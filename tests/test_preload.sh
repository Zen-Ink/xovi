#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

cmake -S "$repo" -B "$work/build" \
    -DCMAKE_TOOLCHAIN_FILE="$repo/cmake/toolchains/aarch64.cmake" \
    -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build "$work/build" --parallel >/dev/null
mkdir -p "$work/root/extensions.d"
printf '%s\n' \
    '#include <stdio.h>' \
    '#include <stdlib.h>' \
    'int main(void) { const char *p = getenv("LD_PRELOAD"); puts(p ? p : "<unset>"); }' \
    | aarch64-linux-gnu-gcc -x c - -o "$work/probe"

preload="$work/build/xovi.so:libm.so.6"
sysroot=$(aarch64-linux-gnu-gcc -print-sysroot)
filtered=$(qemu-aarch64 -L "$sysroot" -U XOVI_INJECT_CHILDREN \
    -E "XOVI_ROOT=$work/root" -E "LD_PRELOAD=$preload" "$work/probe")
inherited=$(qemu-aarch64 -L "$sysroot" -E XOVI_INJECT_CHILDREN=1 \
    -E "XOVI_ROOT=$work/root" -E "LD_PRELOAD=$preload" "$work/probe")

test "$filtered" = "libm.so.6"
test "$inherited" = "$preload"
