#!/usr/bin/env bash
# The Linux packages raylib needs to build (X11 and OpenGL headers) plus xvfb for the render tests.
# Installs without recommended packages and skips 'apt-get update' unless the install fails (the runner images
# usually have current package lists); this is much faster than update + full install on every job.
set -euo pipefail
pkgs="ninja-build libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev xvfb"
# ninja comes from the pinned setup step, so it is not installed here
pkgs="${pkgs#ninja-build }"
if ! sudo apt-get install -y --no-install-recommends $pkgs; then
    sudo rm -f /etc/apt/sources.list.d/google-chrome.list
    sudo apt-get update
    sudo apt-get install -y --no-install-recommends $pkgs
fi
