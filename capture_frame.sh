#!/bin/bash
mkdir -p captures/pocket_test
xvfb-run -a ./build/carrom_arena --mode capture --frames 1 --capture-dir captures/pocket_test &
PID=$!
while [ ! -f captures/pocket_test/frame_0000.png ] && [ $(ps -p $PID | wc -l) -gt 0 ]; do
    sleep 0.5
done
kill $PID 2>/dev/null
