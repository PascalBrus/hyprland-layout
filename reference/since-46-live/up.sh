#!/bin/sh
# Set up the harness in <dir> and start the nested Hyprland on it.
#
#   reference/since-46-live/up.sh <dir> [threecol.so]
#
# Copies this folder into <dir>, links the bind-dispatchers-live checks there
# (so an edit to one runs without copying it again), fills in the placeholders, copies the plugin (default: build/threecol.so) to
# <dir>/build/, launches the nested Hyprland silently on the outer workspace 9,
# writes its signature to <dir>/sig.txt, and returns once threecol is loaded
# and the output size has settled. Stop it with down.sh <dir>.
set -e
[ -n "$1" ] || { echo "usage: $0 <dir> [threecol.so]" >&2; exit 2; }
[ -n "$HYPRLAND_INSTANCE_SIGNATURE" ] || { echo "run this from inside the outer Hyprland session" >&2; exit 1; }

HERE=$(cd "$(dirname "$0")" && pwd)
R=$(cd "$HERE/../.." && pwd)
mkdir -p "$1"
H=$(cd "$1" && pwd)
SO=${2:-$R/build/threecol.so}
[ -f "$SO" ] || { echo "no plugin at $SO; build it first" >&2; exit 1; }

# A reused <dir> must be a harness, so nothing else gets overwritten.
if [ -n "$(ls -A "$H")" ] && [ ! -e "$H/.harness" ]; then
    echo "$H is not empty and not a harness directory" >&2; exit 1
fi
if [ -e "$H/sig.txt" ] && hyprctl instances | grep -q "$(cat "$H/sig.txt")"; then
    echo "a nested Hyprland from $H is still running; stop it with down.sh $H" >&2; exit 1
fi

cp -r "$HERE"/. "$H"/
for f in "$R"/reference/bind-dispatchers-live/*.py; do ln -sf "$f" "$H"/; done
rm -rf "$H/__pycache__" "$H/sig.txt"
sed -i "s|<HARNESS>|$H|g; s|<REPO>|$R|g" "$H/launch.sh" "$H/nest/hyprland.lua" "$H/nest.py"
mkdir -p "$H/build"
cp "$SO" "$H/build/threecol.so"
: > "$H/.harness"

others() {
    hyprctl instances -j | python3 -I -c "import json, os, sys; [print(i['instance']) for i in json.load(sys.stdin) if i['instance'] != os.environ['HYPRLAND_INSTANCE_SIGNATURE']]" | sort
}
# The new instance is the one that wasn't running before the launch.
others > "$H/.before"
hyprctl dispatch "hl.dsp.exec_cmd('$H/launch.sh', { workspace = '9 silent' })" > /dev/null
sig=
for _ in $(seq 40); do
    sig=$(others | comm -13 "$H/.before" - | head -1)
    [ -n "$sig" ] && break
    sleep 0.5
done
rm -f "$H/.before"
[ -n "$sig" ] || { echo "the nested Hyprland didn't start; see $H/nest/log.txt" >&2; exit 1; }
echo "$sig" > "$H/sig.txt"

cd "$H"
python3 -I - "$sig" <<'EOF'
import os, sys, time
sys.path.insert(0, os.getcwd())
import nest as n, checks as c
n.pick(sys.argv[1])
for _ in range(40):
    if "threecol" in n.hc("plugin", "list"):
        break
    time.sleep(0.5)
else:
    sys.exit("threecol didn't load; see nest/log.txt")
# The first reload sets the configured size again, so take it now rather
# than in the middle of the first check.
n.reload()
c.settle_size()
m = n.monitor()
print(f"nested Hyprland up: {sys.argv[1]}, output {m['width']}x{m['height']}")
EOF
