#!/usr/bin/env bash
# Pre-fetch the pinned dependency sources from build/pins.txt into deps/ (or the directory given as $1).
# Each one is a depth-1 fetch of exactly the pinned commit; a checkout that is already at the pin is left alone,
# so a restored CI cache costs nothing. CMake uses deps/<name> automatically when it exists (see CMakeLists.txt).
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
dest="${1:-$root/deps}"
mkdir -p "$dest"
while read -r name url sha _; do
    case "$name" in ''|'#'*) continue ;; esac
    d="$dest/$name"
    if [ -d "$d/.git" ] && [ "$(git -C "$d" rev-parse HEAD 2>/dev/null)" = "$sha" ]; then
        echo "deps: $name already at ${sha:0:9}"
        continue
    fi
    rm -rf "$d"; mkdir -p "$d"
    git -C "$d" init -q
    git -C "$d" remote add origin "$url"
    git -C "$d" fetch -q --depth 1 origin "$sha"
    git -C "$d" checkout -q --detach FETCH_HEAD
    [ "$(git -C "$d" rev-parse HEAD)" = "$sha" ] || { echo "deps: $name: fetched commit does not match the pin" >&2; exit 1; }
    echo "deps: $name fetched ${sha:0:9}"
done < "$root/build/pins.txt"
