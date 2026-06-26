#!/usr/bin/env python3
"""Plot Alta operational turbine inflow speed and power for all turbines."""

import sys
from pathlib import Path

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np

SCRIPT_DIR = Path(__file__).resolve().parent
CSV_PATH = SCRIPT_DIR / "turbine_power_output.csv"
OUTPUT_IMAGE = SCRIPT_DIR / "alta_power_output.png"


def main():
    if not CSV_PATH.exists():
        print(f"ERROR: Missing simulation results: {CSV_PATH}")
        print("Run python3 test_alta_wind_center.py first.")
        sys.exit(1)

    data = np.genfromtxt(CSV_PATH, delimiter=",", names=True, dtype=None, encoding="utf-8")
    order = np.lexsort((data["northing_m"], data["easting_m"]))
    inflows = data["inflow_speed_ms"][order]
    powers = data["power_kw"][order]
    turbine_index = np.arange(1, len(order) + 1)

    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(12, 8), sharex=True)

    ax1.plot(turbine_index, inflows, color="#1f77b4", linewidth=1.2)
    ax1.scatter(turbine_index, inflows, s=10, color="#1f77b4")
    ax1.set_ylabel("Inflow speed [m/s]")
    ax1.set_title("Alta Wind Energy Center Operational Turbine Results")
    ax1.grid(True, linestyle="--", alpha=0.35)

    ax2.plot(turbine_index, powers, color="#d62728", linewidth=1.2)
    ax2.scatter(turbine_index, powers, s=10, color="#d62728")
    ax2.set_xlabel("Turbine index sorted west-to-east")
    ax2.set_ylabel("Power [kW]")
    ax2.grid(True, linestyle="--", alpha=0.35)

    fig.tight_layout()
    fig.savefig(OUTPUT_IMAGE, dpi=300)
    plt.close(fig)
    print(f"Wrote {OUTPUT_IMAGE}")


if __name__ == "__main__":
    main()
