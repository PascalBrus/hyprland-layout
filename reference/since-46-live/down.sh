#!/bin/sh
# Stop the nested Hyprland that up.sh started in <dir>.
#
#   reference/since-46-live/down.sh <dir>
set -e
[ -n "$1" ] || { echo "usage: $0 <dir>" >&2; exit 2; }
H=$(cd "$1" && pwd)
[ -e "$H/sig.txt" ] || { echo "no sig.txt in $H; nothing to stop" >&2; exit 0; }
sig=$(cat "$H/sig.txt")

cd "$H"
python3 -I - "$sig" <<'EOF'
import os, signal, sys, time
sys.path.insert(0, os.getcwd())
import nest as n
n.pick(sys.argv[1])
pid = n.instance()["pid"]
assert pid > 1 and n.HIS != n.OUTER, "refusing to stop the outer session"
os.kill(pid, signal.SIGTERM)
for _ in range(20):
    try:
        os.kill(pid, 0)
    except ProcessLookupError:
        break
    time.sleep(0.5)
else:
    sys.exit(f"nested pid {pid} is still running")
print(f"stopped nested Hyprland {sys.argv[1]}")
EOF
rm -f "$H/sig.txt"
