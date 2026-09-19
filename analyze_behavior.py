#!/usr/bin/env python3
"""
Behavioral verification for R8 - Fixed version.
Uses board region to exclude pieces from figure detection.
"""

import os
import sys
import json
import math
import numpy as np
from pathlib import Path
from PIL import Image

CAPTURE_DIR = "/tmp/vis_r8_behavior"
FRAMES = 180

# Colors (from board_view.c)
COLOR_STRIKER = (255, 215, 0)      # Gold
COLOR_WHITE_PIECE = (240, 240, 240)
COLOR_BLACK_PIECE = (30, 30, 30)
COLOR_QUEEN = (220, 30, 30)        # Red
COLOR_AIM_LINE = (255, 255, 0)     # Yellow
COLOR_AIM_OUTLINE = (0, 0, 0)      # Black
COLOR_TEAM_WHITE_FILL = (240, 240, 220)
COLOR_TEAM_WHITE_OUTLINE = (60, 60, 60)
COLOR_TEAM_BLACK_FILL = (40, 40, 40)
COLOR_TEAM_BLACK_OUTLINE = (200, 200, 200)
COLOR_BOARD = (139, 105, 70)
COLOR_CUSHION = (100, 70, 40)
COLOR_TURN_HIGHLIGHT = (255, 215, 0)  # Gold

TOL = 40  # Color tolerance

# Expected board region for 1920x1080 (from layout_compute)
BOARD_X, BOARD_Y, BOARD_SIZE = 727, 206, 706
BOARD_BBOX = (BOARD_X, BOARD_Y, BOARD_X + BOARD_SIZE, BOARD_Y + BOARD_SIZE)

# Figure bands (from layout_compute)
# N figure band: title_band_h + figure_band_h/2 = 59 + 88/2 = 103
# S figure band: sh - footer_band_h - figure_band_h/2 = 1080 - 108 - 44 = 928
# Figure bands are horizontal strips above and below board
N_FIGURE_BAND_Y = 103
S_FIGURE_BAND_Y = 928
FIGURE_BAND_H = 88

def pearsonr(x, y):
    """Compute Pearson correlation coefficient."""
    n = len(x)
    if n < 2:
        return 0.0, 1.0
    mx, my = np.mean(x), np.mean(y)
    dx, dy = x - mx, y - my
    num = np.sum(dx * dy)
    den = math.sqrt(np.sum(dx*dx) * np.sum(dy*dy))
    if den == 0:
        return 0.0, 1.0
    return num / den, 1.0

def load_frame(n):
    """Load frame n as numpy array."""
    path = Path(CAPTURE_DIR) / f"frame_{n:06d}.png"
    return np.array(Image.open(path).convert('RGB'))

def find_color_regions(arr, target_color, tol=TOL):
    """Find all pixels matching target color within tolerance."""
    diff = np.abs(arr.astype(np.int16) - np.array(target_color).astype(np.int16))
    mask = np.all(diff <= tol, axis=2)
    return mask

def get_centroid(mask):
    """Get centroid of mask."""
    coords = np.argwhere(mask)
    if len(coords) == 0:
        return None
    return coords[:, 1].mean(), coords[:, 0].mean()  # x, y

def find_striker(arr):
    """Find striker position (gold color) - only in board region."""
    bx1, by1, bx2, by2 = BOARD_BBOX
    board_region = arr[by1:by2, bx1:bx2]
    mask = find_color_regions(board_region, COLOR_STRIKER, 50)
    centroid = get_centroid(mask)
    if centroid:
        return (centroid[0] + bx1, centroid[1] + by1)
    return None

def find_figure(arr):
    """Find figure position - ONLY in figure bands (outside board)."""
    bx1, by1, bx2, by2 = BOARD_BBOX
    
    # Search in North figure band (above board)
    n_band = arr[max(0, N_FIGURE_BAND_Y - FIGURE_BAND_H//2):N_FIGURE_BAND_Y + FIGURE_BAND_H//2, :]
    # Search in South figure band (below board)
    s_band = arr[max(0, S_FIGURE_BAND_Y - FIGURE_BAND_H//2):min(arr.shape[0], S_FIGURE_BAND_Y + FIGURE_BAND_H//2), :]
    
    # Try white team figure
    for band, band_y_offset in [(n_band, N_FIGURE_BAND_Y - FIGURE_BAND_H//2), (s_band, S_FIGURE_BAND_Y - FIGURE_BAND_H//2)]:
        mask_w = find_color_regions(band, COLOR_TEAM_WHITE_FILL, 50)
        mask_w |= find_color_regions(band, COLOR_TEAM_WHITE_OUTLINE, 50)
        centroid = get_centroid(mask_w)
        if centroid:
            return (centroid[0], centroid[1] + band_y_offset), 'white'
    
    # Try black team figure
    for band, band_y_offset in [(n_band, N_FIGURE_BAND_Y - FIGURE_BAND_H//2), (s_band, S_FIGURE_BAND_Y - FIGURE_BAND_H//2)]:
        mask_b = find_color_regions(band, COLOR_TEAM_BLACK_FILL, 50)
        mask_b |= find_color_regions(band, COLOR_TEAM_BLACK_OUTLINE, 50)
        centroid = get_centroid(mask_b)
        if centroid:
            return (centroid[0], centroid[1] + band_y_offset), 'black'
    
    return None, None

def find_aim_line(arr):
    """Detect aim line (yellow line with black outline) - ONLY in board region."""
    bx1, by1, bx2, by2 = BOARD_BBOX
    board_region = arr[by1:by2, bx1:bx2]
    
    mask = find_color_regions(board_region, COLOR_AIM_LINE, 50)
    if np.sum(mask) < 10:
        return None
    
    coords = np.argwhere(mask)
    y_min, x_min = coords.min(axis=0)
    y_max, x_max = coords.max(axis=0)
    
    # Convert back to full image coordinates
    start = (x_min + bx1, y_min + by1)
    end = (x_max + bx1, y_max + by1)
    
    thickness = estimate_line_thickness(board_region, (x_min, y_min), (x_max, y_max))
    arrowhead = detect_arrowhead(board_region, (x_max, y_max))
    
    return {
        'start': start,
        'end': end,
        'thickness': thickness,
        'arrowhead': arrowhead,
        'visible': True
    }

def estimate_line_thickness(arr, start, end):
    """Estimate line thickness perpendicular to line direction."""
    x1, y1 = start
    x2, y2 = end
    dx, dy = x2 - x1, y2 - y1
    length = math.hypot(dx, dy)
    if length < 10:
        return 0
    
    px, py = -dy / length, dx / length
    mx, my = (x1 + x2) / 2, (y1 + y2) / 2
    
    count = 0
    for offset in range(-10, 11):
        sx, sy = int(mx + px * offset), int(my + py * offset)
        if 0 <= sx < arr.shape[1] and 0 <= sy < arr.shape[0]:
            diff = np.abs(arr[sy, sx].astype(np.int16) - np.array(COLOR_AIM_LINE).astype(np.int16))
            if np.all(diff <= 50):
                count += 1
    return count

def detect_arrowhead(arr, tip):
    """Detect arrowhead (triangle) at line end."""
    x, y = int(tip[0]), int(tip[1])
    count = 0
    for dy in range(-8, 9):
        for dx in range(-8, 9):
            sx, sy = x + dx, y + dy
            if 0 <= sx < arr.shape[1] and 0 <= sy < arr.shape[0]:
                diff = np.abs(arr[sy, sx].astype(np.int16) - np.array(COLOR_AIM_LINE).astype(np.int16))
                if np.all(diff <= 50):
                    count += 1
    return count > 20

def find_board_bbox(arr):
    """Find board bounding box from brown color."""
    bx1, by1, bx2, by2 = BOARD_BBOX
    board_region = arr[by1:by2, bx1:bx2]
    mask = find_color_regions(board_region, COLOR_BOARD, 30)
    coords = np.argwhere(mask)
    if len(coords) == 0:
        return BOARD_BBOX
    y_min, x_min = coords.min(axis=0)
    y_max, x_max = coords.max(axis=0)
    return (x_min + bx1, y_min + by1, x_max + bx1, y_max + by1)

def get_game_phase(arr):
    """Determine game phase from visual cues."""
    # Check for aim line in board region
    if find_aim_line(arr):
        return 'AIM_PREVIEW'
    
    # Check for thinking striker on baseline (South baseline at bottom of board)
    striker = find_striker(arr)
    if striker:
        _, _, _, by2 = BOARD_BBOX
        baseline_y = by2 + 10  # Just below board
        if abs(striker[1] - baseline_y) < 60:
            return 'THINKING'
    
    return 'UNKNOWN'

def main():
    print("Loading frames...")
    frames_data = []
    
    for i in range(FRAMES):
        if i % 30 == 0:
            print(f"  Processing frame {i}/{FRAMES}")
        arr = load_frame(i)
        
        striker = find_striker(arr)
        figure_pos, figure_team = find_figure(arr)
        aim_line = find_aim_line(arr)
        board = find_board_bbox(arr)
        phase = get_game_phase(arr)
        
        frames_data.append({
            'frame': i,
            'striker': striker,
            'figure': figure_pos,
            'figure_team': figure_team,
            'aim_line': aim_line,
            'board': board,
            'phase': phase
        })
    
    print("Analyzing...")
    
    # 1. Figure mirrors striker during THINKING (frames 10-60)
    print("\n1. Figure-Striker correlation (THINKING frames 10-60):")
    thinking_frames = [f for f in frames_data[10:61] if f['phase'] == 'THINKING' and f['striker'] and f['figure']]
    if len(thinking_frames) >= 5:
        striker_coords = np.array([f['striker'] for f in thinking_frames])
        figure_coords = np.array([f['figure'] for f in thinking_frames])
        
        r_x, _ = pearsonr(striker_coords[:, 0], figure_coords[:, 0])
        r_y, _ = pearsonr(striker_coords[:, 1], figure_coords[:, 1])
        
        print(f"  Frames analyzed: {len(thinking_frames)}")
        print(f"  Pearson r (x-axis): {r_x:.4f}")
        print(f"  Pearson r (y-axis): {r_y:.4f}")
        
        r = max(abs(r_x), abs(r_y))
        mirror_ok = r >= 0.99
        print(f"  Max correlation: {r:.4f} {'PASS' if mirror_ok else 'FAIL'} (need ≥0.99)")
    else:
        print(f"  Insufficient THINKING frames with both visible: {len(thinking_frames)}")
        mirror_ok = False
        r = 0
    
    # 2. Figure never enters board bbox
    print("\n2. Figure outside board bbox (all frames):")
    figure_inside = 0
    for f in frames_data:
        if f['figure'] and f['board']:
            fx, fy = f['figure']
            bx1, by1, bx2, by2 = f['board']
            if bx1 <= fx <= bx2 and by1 <= fy <= by2:
                figure_inside += 1
                if figure_inside <= 5:
                    print(f"  Frame {f['frame']}: Figure at ({fx:.1f}, {fy:.1f}) INSIDE board [{bx1},{by1},{bx2},{by2}]")
    
    outside_ok = figure_inside == 0
    print(f"  Frames with figure inside board: {figure_inside} {'PASS' if outside_ok else 'FAIL'}")
    
    # 3. Aim line verification (AIM_PREVIEW frames)
    print("\n3. Aim line verification (AIM_PREVIEW frames):")
    aim_frames = [f for f in frames_data if f['aim_line'] and f['aim_line']['visible']]
    aim_ok = True
    if aim_frames:
        for f in aim_frames[:5]:
            al = f['aim_line']
            striker_pos = f['striker']
            board = f['board']
            
            print(f"  Frame {f['frame']}:")
            print(f"    Line: {al['start']} -> {al['end']}")
            print(f"    Thickness: {al['thickness']}px")
            print(f"    Arrowhead: {al['arrowhead']}")
            
            if striker_pos:
                dist = math.hypot(al['start'][0] - striker_pos[0], al['start'][1] - striker_pos[1])
                print(f"    Start-striker distance: {dist:.1f}px")
                if dist > 3:
                    print(f"    FAIL: Start-striker distance > 3px")
                    aim_ok = False
            
            if board:
                bx1, by1, bx2, by2 = board
                ex, ey = al['end']
                dx = max(bx1 - ex, 0, ex - bx2)
                dy = max(by1 - ey, 0, ey - by2)
                boundary_dist = math.hypot(dx, dy) if (dx > 0 or dy > 0) else 0
                print(f"    End-boundary distance: {boundary_dist:.1f}px")
                if boundary_dist > 1:
                    print(f"    FAIL: End-boundary distance > 1px")
                    aim_ok = False
            
            if al['thickness'] < 3:
                print(f"    FAIL: Thickness < 3px")
                aim_ok = False
            
            if not al['arrowhead']:
                print(f"    FAIL: No arrowhead detected")
                aim_ok = False
    else:
        print("  No AIM_PREVIEW frames detected")
        aim_ok = False
    
    print(f"  Overall: {'PASS' if aim_ok else 'FAIL'}")
    
    # 4. No frame with (aim line visible AND striker velocity > 0)
    print("\n4. No aim line during striker movement:")
    violation = False
    for i in range(1, len(frames_data)):
        f = frames_data[i]
        f_prev = frames_data[i-1]
        if f['aim_line'] and f['aim_line']['visible'] and f['striker'] and f_prev['striker']:
            dist = math.hypot(f['striker'][0] - f_prev['striker'][0], f['striker'][1] - f_prev['striker'][1])
            if dist > 2.0:
                if not violation:
                    print(f"  First violation: Frame {f['frame']}: Aim line visible but striker moved {dist:.1f}px")
                violation = True
    
    vel_ok = not violation
    print(f"  Violations: {0 if vel_ok else 'FOUND'} {'PASS' if vel_ok else 'FAIL'}")
    
    # Footer URL check
    print("\n5. Footer URL check:")
    url_found = False
    for i in [0, 50, 100, 179]:
        arr = load_frame(i)
        h = arr.shape[0]
        footer_region = arr[int(h*0.9):, :]
        text_mask = find_color_regions(footer_region, (192, 192, 192), 50)
        if np.sum(text_mask) > 100:
            url_found = True
            print(f"  Frame {i}: Footer text detected")
            break
    
    if not url_found:
        for i in [0, 50, 100, 179]:
            arr = load_frame(i)
            h = arr.shape[0]
            footer = arr[int(h*0.9):, int(arr.shape[1]*0.3):int(arr.shape[1]*0.7)]
            mask = find_color_regions(footer, (200, 200, 200), 40)
            if np.sum(mask) > 50:
                url_found = True
                print(f"  Frame {i}: Footer link color detected")
                break
    
    print(f"  URL found: {'PASS' if url_found else 'FAIL'}")
    
    # Summary
    all_pass = mirror_ok and outside_ok and aim_ok and vel_ok and url_found
    print(f"\n=== SUMMARY ===")
    print(f"Figure mirrors striker: {'PASS' if mirror_ok else 'FAIL'} (r={r:.4f})")
    print(f"Figure outside board:   {'PASS' if outside_ok else 'FAIL'}")
    print(f"Aim line correct:       {'PASS' if aim_ok else 'FAIL'}")
    print(f"No aim during movement: {'PASS' if vel_ok else 'FAIL'}")
    print(f"Footer URL present:     {'PASS' if url_found else 'FAIL'}")
    print(f"ALL CHECKS:             {'PASS' if all_pass else 'FAIL'}")
    
    log_entry = {
        "step": "behavioral_verification",
        "figure_striker_correlation": float(r),
        "mirror_ok": bool(mirror_ok),
        "figure_inside_board_count": int(figure_inside),
        "outside_ok": bool(outside_ok),
        "aim_line_ok": bool(aim_ok),
        "velocity_violation": bool(violation),
        "vel_ok": bool(vel_ok),
        "footer_url_ok": bool(url_found),
        "all_pass": bool(all_pass)
    }
    
    with open(".kimi_progress.log", "a") as f:
        f.write(json.dumps(log_entry, indent=2) + "\n\n")
    
    return 0 if all_pass else 1

if __name__ == "__main__":
    sys.exit(main())