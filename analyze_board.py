#!/usr/bin/env python3
"""
Board centering/sizing analysis for R8 verification.
Analyzes captured frames to verify board positioning and sizing across resolutions.
"""

import os
import sys
import json
from pathlib import Path
from PIL import Image
import numpy as np

# Board background color (brown ~139,105,70) vs dark background (#1e1e28)
BOARD_COLOR = (139, 105, 70)
BOARD_TOLERANCE = 30
DARK_BG = (30, 30, 40)  # #1e1e28 approx

# HUD layout percentages
HUD_SIDEBAR_LEFT = 0.16   # 16% left sidebar
HUD_SIDEBAR_RIGHT = 0.04  # 4% right sidebar
HUD_TITLE_BAND = 0.055    # 5.5% top title band
HUD_FOOTER_BAND = 0.10    # 10% bottom footer band

CENTER_TOLERANCE = 0.05  # 5% tolerance for center alignment

def find_board_bbox(image_path):
    """Find board bounding box by detecting board background color."""
    img = Image.open(image_path).convert('RGB')
    arr = np.array(img)
    h, w = arr.shape[:2]
    
    # Create mask for board-colored pixels
    diff = np.abs(arr.astype(np.int16) - np.array(BOARD_COLOR).astype(np.int16))
    board_mask = np.all(diff <= BOARD_TOLERANCE, axis=2)
    
    # Also detect non-dark-background (board area vs dark UI)
    dark_diff = np.abs(arr.astype(np.int16) - np.array(DARK_BG).astype(np.int16))
    non_dark_mask = np.any(dark_diff > 40, axis=2)
    
    # Combine: board color OR (non-dark and roughly rectangular)
    combined_mask = board_mask | (non_dark_mask & ~board_mask)
    
    # Find bounding box of board-colored region
    rows = np.any(board_mask, axis=1)
    cols = np.any(board_mask, axis=0)
    
    if not np.any(rows) or not np.any(cols):
        # Fallback: use non-dark region
        rows = np.any(non_dark_mask, axis=1)
        cols = np.any(non_dark_mask, axis=0)
    
    if not np.any(rows) or not np.any(cols):
        return None
    
    y_min, y_max = np.where(rows)[0][[0, -1]]
    x_min, x_max = np.where(cols)[0][[0, -1]]
    
    return (int(x_min), int(y_min), int(x_max), int(y_max))

def compute_allotted_region(width, height):
    """Compute the allotted region for the board (window minus HUD elements)."""
    left = int(width * HUD_SIDEBAR_LEFT)
    right = int(width * (1 - HUD_SIDEBAR_RIGHT))
    top = int(height * HUD_TITLE_BAND)
    bottom = int(height * (1 - HUD_FOOTER_BAND))
    return (left, top, right, bottom)

def analyze_resolution(capture_dir, width, height):
    """Analyze a single resolution's captures."""
    frames = sorted(Path(capture_dir).glob("frame_*.png"))
    if not frames:
        return None
    
    # Use first frame for analysis (board position should be static)
    bbox = find_board_bbox(frames[0])
    if not bbox:
        return None
    
    x_min, y_min, x_max, y_max = bbox
    board_center_x = (x_min + x_max) / 2
    board_center_y = (y_min + y_max) / 2
    board_width = x_max - x_min
    board_height = y_max - y_min
    board_size = min(board_width, board_height)  # Board should be square
    
    # Allotted region
    left, top, right, bottom = compute_allotted_region(width, height)
    allotted_center_x = (left + right) / 2
    allotted_center_y = (top + bottom) / 2
    allotted_width = right - left
    allotted_height = bottom - top
    allotted_size = min(allotted_width, allotted_height)
    
    # Center offset as percentage of allotted region
    center_offset_x = abs(board_center_x - allotted_center_x) / allotted_width
    center_offset_y = abs(board_center_y - allotted_center_y) / allotted_height
    
    centered = (center_offset_x <= CENTER_TOLERANCE) and (center_offset_y <= CENTER_TOLERANCE)
    
    return {
        "resolution": f"{width}x{height}",
        "board_bbox": [x_min, y_min, x_max, y_max],
        "board_center": [board_center_x, board_center_y],
        "board_size": board_size,
        "allotted_region": [left, top, right, bottom],
        "allotted_center": [allotted_center_x, allotted_center_y],
        "allotted_size": allotted_size,
        "center_offset_pct": [center_offset_x * 100, center_offset_y * 100],
        "centered": centered,
        "size_ratio": board_size / allotted_size
    }

def main():
    resolutions = [
        ("/tmp/vis_r8_1280x720", 1280, 720),
        ("/tmp/vis_r8_1920x1080", 1920, 1080),
        ("/tmp/vis_r8_2560x1440", 2560, 1440),
        ("/tmp/vis_r8_1000x600", 1000, 600),
    ]
    
    results = []
    for capture_dir, w, h in resolutions:
        print(f"Analyzing {w}x{h}...")
        result = analyze_resolution(capture_dir, w, h)
        if result:
            results.append(result)
            print(f"  Board bbox: {result['board_bbox']}")
            print(f"  Board center: ({result['board_center'][0]:.1f}, {result['board_center'][1]:.1f})")
            print(f"  Board size: {result['board_size']:.1f}")
            print(f"  Allotted center: ({result['allotted_center'][0]:.1f}, {result['allotted_center'][1]:.1f})")
            print(f"  Center offset: ({result['center_offset_pct'][0]:.2f}%, {result['center_offset_pct'][1]:.2f}%)")
            print(f"  Centered: {result['centered']}")
            print(f"  Size ratio: {result['size_ratio']:.3f}")
        else:
            print(f"  FAILED to analyze")
    
    # Check monotonic growth
    print("\n=== MONOTONIC GROWTH CHECK ===")
    sizes = [(r['resolution'], r['board_size']) for r in results if '1280x720' in r['resolution'] or '1920x1080' in r['resolution'] or '2560x1440' in r['resolution']]
    sizes.sort(key=lambda x: int(x[0].split('x')[0]))
    
    monotonic = True
    for i in range(1, len(sizes)):
        prev_size = sizes[i-1][1]
        curr_size = sizes[i][1]
        if curr_size <= prev_size:
            monotonic = False
            print(f"FAIL: {sizes[i-1][0]} ({prev_size:.1f}) -> {sizes[i][0]} ({curr_size:.1f}) not growing")
        else:
            print(f"OK: {sizes[i-1][0]} ({prev_size:.1f}) -> {sizes[i][0]} ({curr_size:.1f}) growing")
    
    # Check 1000x600 is appropriate (smaller than 1280x720)
    r_1000 = next((r for r in results if r['resolution'] == '1000x600'), None)
    r_1280 = next((r for r in results if r['resolution'] == '1280x720'), None)
    if r_1000 and r_1280:
        appropriate = r_1000['board_size'] < r_1280['board_size']
        print(f"1000x600 appropriate: {appropriate} (size={r_1000['board_size']:.1f} < {r_1280['board_size']:.1f})")
    
    # Log to .kimi_progress.log
    log_entry = {
        "step": "board_centering_sizing",
        "results": results,
        "monotonic_growth": monotonic,
        "all_centered": all(r['centered'] for r in results)
    }
    
    with open(".kimi_progress.log", "a") as f:
        f.write(json.dumps(log_entry, indent=2) + "\n\n")
    
    # Summary
    print("\n=== SUMMARY ===")
    print(f"All centered: {all(r['centered'] for r in results)}")
    print(f"Monotonic growth (1280->1920->2560): {monotonic}")
    
    return 0 if (all(r['centered'] for r in results) and monotonic) else 1

if __name__ == "__main__":
    sys.exit(main())