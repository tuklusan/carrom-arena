import os
import re
import subprocess
from collections import defaultdict

def run_sim(speed, frames=300):
    capture_dir = f"captures/speed_test_{speed}"
    if not os.path.exists("captures"):
        os.makedirs("captures")
    cmd = [
        "./build/carrom_arena",
        "--mode", "capture",
        "--playback-speed", str(speed),
        "--frames", str(frames),
        "--capture-dir", capture_dir,
        "--headless", "true",
        "--debug-phase", "true"
    ]
    print(f"Running simulation at speed {speed}x...")
    result = subprocess.run(cmd, capture_output=True, text=True)
    return result.stderr

def parse_striker_speed(logs):
    pattern = re.compile(r"frame=(\d+) phase=(\d+) placement_timer=([\d.]+) playback_speed=([\d.]+) striker_x=([\d.-]+) striker_y=([\d.-]+)")
    
    frames = []
    for line in logs.splitlines():
        match = pattern.search(line)
        if match:
            frames.append({
                "frame": int(match.group(1)),
                "phase": int(match.group(2)),
                "timer": float(match.group(3)),
                "speed": float(match.group(4)),
                "x": float(match.group(5)),
                "y": float(match.group(6))
            })
    
    if not frames:
        return None

    results = {}
    # Phase 2: PLACEMENT, Phase 5: SHOT
    for phase_id in [2, 5]:
        phase_frames = [f for f in frames if f["phase"] == phase_id]
        if len(phase_frames) < 2:
            continue
            
        first = phase_frames[0]
        last = phase_frames[-1]
        
        dx = last["x"] - first["x"]
        dy = last["y"] - first["y"]
        total_dist = (dx*dx + dy*dy)**0.5
        
        # Use actual frame count for wall time
        wall_time = (len(phase_frames) - 1) * (1.0 / 15.0)
        speed_bw_ps = total_dist / wall_time
        results[phase_id] = speed_bw_ps
        
    return results

if __name__ == "__main__":
    speeds_to_test = [0.1, 1.0]
    final_table = []
    
    for s in speeds_to_test:
        logs = run_sim(s)
        res = parse_striker_speed(logs)
        if res:
            final_table.append((s, res))
    
    if not final_table:
        print("No data captured.")
        exit(1)

    print("\n--- Speed Measurement Table ---")
    print("Speed(x) | Phase     | Drawn Speed (BW/s)")
    print("-------------------------------------------")
    for s, res in final_table:
        for phase, val in res.items():
            phase_name = "PLACEMENT" if phase == 2 else "SHOT"
            print(f"{s:8.2f} | {phase_name:9} | {val:10.4f}")
    
    if len(final_table) >= 2:
        s1, res1 = final_table[0]
        s2, res2 = final_table[1]
        print("\nLinearity Verification:")
        for phase in [2, 5]:
            if phase in res1 and phase in res2:
                ratio = res2[phase] / res1[phase]
                expected = s2 / s1
                print(f"Phase {phase} Ratio: {ratio:.4f}, Expected: {expected:.4f}")

    with open(".kimi_progress.log", "a") as f:
        f.write("\n--- Game Speed Measurement Table ---\n")
        f.write("Speed(x) | Phase     | Drawn Speed (BW/s)\n")
        f.write("-------------------------------------------\n")
        for s, res in final_table:
            for phase, val in res.items():
                phase_name = "PLACEMENT" if phase == 2 else "SHOT"
                f.write(f"{s:8.2f} | {phase_name:9} | {val:10.4f}\n")
