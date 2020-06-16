#!/bin/sh

dir="$(dirname "$0")"
cd "$dir" || exit
abspath="$(realpath "$dir")"

mkdir -p ./bin >/dev/null 2>&1
mkdir -p ./obj >/dev/null 2>&1

. ./cflags.sh

# /path/to/meme.c -> meme
exename() {
  basename "$1" | rev | cut -d. -f2- | rev | cut -d_ -f2-
}

compile() {
  ${cc:-gcc} $cflags "$@" $ldflags -lGL -lX11 -lm
}

# I explicitly build core separately just to make sure it's built with the absolute minimum cflags
# so that any non-portable code is more likely to generate a warning
echo ":: building core"
compile -DWEEBCORE_IMPLEMENTATION -c -o obj/WeebCore.o WeebCore.c || exit

cflags="$cflags
 -I\"$abspath\"
 -DWEEBCORE_OVERRIDE_MONOLITHIC_BUILD=1
 -D_XOPEN_SOURCE=600 obj/WeebCore.o"

case "$1" in
  *.c)
    compile "$@" -I"$abspath" -o "./bin/$(exename "$1")" || exit
    ;;
  *)
    echo ":: building examples"
    for x in Examples/*.c; do
      compile "$@" "$x" -I"$abspath" -o "./bin/$(exename "$x")"\
        || exit
    done

    echo ":: building utils"
    for x in Utils/*.c; do
      compile "$@" "$x" -I"$abspath" -o "./bin/$(exename "$x")" || exit
    done
  ;;
esac
