import csv

def analyze():
    try:
        with open('striker_measure.csv', 'r') as f:
            reader = csv.DictReader(f)
            data = list(reader)
    except FileNotFoundError:
        print("CSV not found")
        return

    if not data:
        print("No data")
        return

    # Group by phase
    phases = {}
    current_phase = None
    phase_groups = []

    for row in data:
        p = row['phase']
        if p != current_phase:
            current_phase = p
            phase_groups.append([])
        phase_groups[-1].append(row)

    print(f"| Phase | Wall-Sec | Dist (Board-Widths) | Velocity (BW/s) |")
    print(f"|-------|----------|---------------------|------------------|")
    
    for group in phase_groups:
        if len(group) < 2: continue
        
        p_id = group[0]['phase']
        t_start = float(group[0]['wall_time'])
        t_end = float(group[-1]['wall_time'])
        duration = t_end - t_start
        
        x_start = float(group[0]['striker_x'])
        y_start = float(group[0]['striker_y'])
        x_end = float(group[-1]['striker_x'])
        y_end = float(group[-1]['striker_y'])
        
        dist = ((x_end - x_start)**2 + (y_end - y_start)**2)**0.5
        vel = dist / duration if duration > 0 else 0
        
        phase_name = "Unknown"
        if p_id == "1": phase_name = "THINKING"
        elif p_id == "2": phase_name = "PLACEMENT"
        elif p_id == "3": phase_name = "AIM_PREVIEW"
        elif p_id == "5": phase_name = "SHOT"
        
        print(f"| {phase_name} | {duration:.3f} | {dist:.3f} | {vel:.3f} |")

analyze()
