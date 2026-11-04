#!/bin/sh
# Builds the README GIFs: docs/threecol.gif (tour.py) and
# docs/threecol-settings.gif (settings.py). Needs Python 3 with Pillow and
# NumPy, the JetBrains Mono Nerd and Noto Sans fonts, and ffmpeg.
set -eu
here=$(cd "$(dirname "$0")" && pwd)
docs="$here/../../docs"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
mkdir -p "$docs"
cd "$here"

build() {
    script=$1 out=$2
    python3 "$script.py" 1600
    python3 render.py "$script.pkl" "$work/$script"
    ffmpeg -loglevel error -y -framerate 25 -i "$work/$script/%05d.png" -vf palettegen=stats_mode=diff "$work/$script-pal.png"
    ffmpeg -loglevel error -y -framerate 25 -i "$work/$script/%05d.png" -i "$work/$script-pal.png" \
        -lavfi "paletteuse=dither=bayer:bayer_scale=5:diff_mode=rectangle" -loop 0 "$docs/$out"
    rm "$script.pkl"
    ls -l "$docs/$out"
}

build tour threecol.gif
build settings threecol-settings.gif
