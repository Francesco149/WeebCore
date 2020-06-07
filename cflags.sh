#!/bin/sh

cflags="-std=c89 -pedantic -O3 -Wall -Wno-overlength-strings -Wno-int-to-pointer-cast 
  -Wno-pointer-to-int-cast -ffunction-sections -fdata-sections -fwrapv"
if [ -z "$DBGINFO" ]; then
  cflags="$cflags -g0 -fno-unwind-tables -s -fno-asynchronous-unwind-tables -fno-stack-protector
    -Wl,--build-id=none"
else
  cflags="$cflags -g -fsanitize=address,undefined"
fi
if [ "$(uname)" = "Darwin" ]; then
  cc=${CC:-clang}
  cflags="$cflags -Wl,-dead_strip"
else
  cc=${CC:-gcc}
  cflags="$cflags -Wl,--gc-sections"
fi

cflags="$cflags ${CFLAGS:-}"
ldflags="$ldflags ${LDFLAGS:-}"

{ uname -a; echo "$cc"; echo "$cflags"; echo "$ldflags"; $cc --version; $cc -dumpmachine; } \
  > flags.log

export cflags
export ldflags
export cc

