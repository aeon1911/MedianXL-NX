#!/bin/sh
set -e
cd "$(dirname "$0")"
CC=i686-w64-mingw32-gcc
$CC -O2 -Wall -Wextra -shared -static-libgcc -o D2NXPad.dll d2nxpad.c d2ver.c -Wl,--kill-at -lm
$CC -O2 -Wall -Wextra -shared -static-libgcc -o D2Autorun.dll d2autorun.c -Wl,--kill-at -luser32 -ladvapi32
$CC -O2 -shared -static-libgcc -o tests/xinput1_4.dll tests/xinput_file.c -Wl,--kill-at
echo built
