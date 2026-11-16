import csv
import math
from pathlib import Path

path = Path("data/sample_tracking.csv")
path.parent.mkdir(exist_ok=True)

with path.open("w", newline="") as file:
    writer = csv.writer(file)

    writer.writerow([
        "frame", "time", "id",
        "x", "y", "z",
        "qx", "qy", "qz", "qw",
        "mean_error", "valid"
    ])

    for frame in range(720):
        t = frame / 120.0

        for body_id in range(1, 5):
            phase = 2 * math.pi * 0.25 * t + body_id * 0.6
            angle = 0.4 * math.sin(phase)

            writer.writerow([
                frame,
                f"{t:.6f}",
                body_id,
                f"{(body_id - 1) * 0.6 - 0.9 + 0.12 * math.sin(phase):.6f}",
                f"{0.25 + 0.04 * math.sin(phase):.6f}",
                f"{0.08 * math.cos(phase):.6f}",
                0,
                f"{math.sin(angle / 2):.8f}",
                0,
                f"{math.cos(angle / 2):.8f}",
                0.0002,
                1
            ])

print(f"Generated {path}")
