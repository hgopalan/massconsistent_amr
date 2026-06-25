#!/usr/bin/env python3
"""Run the fixed Alta operational wake case and generate setup and hub-height plots."""

import csv
import os
import sys
import unittest
from pathlib import Path

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np

TEST_DIR = Path(__file__).resolve().parent
REPO_ROOT = TEST_DIR.parent.parent.parent
SRC_PYTHON_DIR = REPO_ROOT / "src" / "python"
BUILD_PYTHON_DIR = REPO_ROOT / "build" / "python"
TURBINE_FILE = TEST_DIR / "turbines.csv"
TERRAIN_FILE = TEST_DIR / "terrain.csv"
INPUTS_FILE = TEST_DIR / "inputs.i"
LAYOUT_IMAGE = TEST_DIR / "alta_turbine_layout.png"
WAKE_IMAGE = TEST_DIR / "alta_wake_80m.png"
OUTPUT_CSV = TEST_DIR / "turbine_power_output.csv"
EXPECTED_TURBINE_COUNT = 485

sys.path.insert(0, str(BUILD_PYTHON_DIR))
sys.path.insert(0, str(SRC_PYTHON_DIR))

try:
    from wind_solver import WindSolver
except ImportError as e:
    print(f"ERROR: Could not import WindSolver: {e}")
    sys.exit(1)


def load_turbine_records(path: Path):
    records = []
    with open(path, "r", encoding="utf-8") as handle:
        for line in handle:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            parts = [part.strip() for part in line.split(",")]
            records.append(
                {
                    "x": float(parts[0]),
                    "y": float(parts[1]),
                    "hub_height": float(parts[2]),
                    "rotor_diameter": float(parts[3]),
                }
            )
    return records


class TestAltaWindCenter(unittest.TestCase):
    def setUp(self):
        self.output_dir = TEST_DIR
        self.assertTrue(TURBINE_FILE.exists(), f"Missing {TURBINE_FILE}")
        self.assertTrue(TERRAIN_FILE.exists(), f"Missing {TERRAIN_FILE}")
        self.assertTrue(INPUTS_FILE.exists(), f"Missing {INPUTS_FILE}")

        self.records = load_turbine_records(TURBINE_FILE)
        self.num_turbines = len(self.records)
        self.assertEqual(
            self.num_turbines,
            EXPECTED_TURBINE_COUNT,
            "Alta operational setup must contain the committed 485 turbines.",
        )
        self.xs = np.array([record["x"] for record in self.records])
        self.ys = np.array([record["y"] for record in self.records])
        self.hub_heights = np.array([record["hub_height"] for record in self.records])
        self.rotor_diameters = np.array([record["rotor_diameter"] for record in self.records])

    def _write_layout_image(self):
        fig, ax = plt.subplots(figsize=(10, 8))
        scatter = ax.scatter(
            self.xs,
            self.ys,
            c=self.hub_heights,
            s=np.clip(self.rotor_diameters * 0.8, 30.0, 120.0),
            cmap="viridis",
            alpha=0.85,
            edgecolors="black",
            linewidths=0.2,
        )
        cbar = fig.colorbar(scatter, ax=ax)
        cbar.set_label("Hub height [m]")
        ax.set_title("Alta Wind Energy Center Operational Turbine Layout")
        ax.set_xlabel("Easting [m] (UTM Zone 11N)")
        ax.set_ylabel("Northing [m] (UTM Zone 11N)")
        ax.grid(True, linestyle="--", alpha=0.35)
        ax.set_aspect("equal", adjustable="box")
        fig.tight_layout()
        fig.savefig(LAYOUT_IMAGE, dpi=300)
        plt.close(fig)

    def test_alta_simulation(self):
        self._write_layout_image()

        old_cwd = os.getcwd()
        os.chdir(self.output_dir)
        try:
            wind = WindSolver()
            wind.initialize(str(INPUTS_FILE.name))
            solve_result = wind.solve()
            self.assertTrue(solve_result["success"])

            powers = np.asarray(wind.get_turbine_power_outputs(), dtype=float)
            inflows = np.asarray(wind.get_turbine_inflow_speeds(), dtype=float)
            self.assertEqual(len(powers), self.num_turbines)
            self.assertEqual(len(inflows), self.num_turbines)

            with open(OUTPUT_CSV, "w", encoding="utf-8", newline="") as handle:
                writer = csv.writer(handle)
                writer.writerow(
                    [
                        "wt_id",
                        "easting_m",
                        "northing_m",
                        "hub_height_m",
                        "rotor_diameter_m",
                        "inflow_speed_ms",
                        "power_kw",
                    ]
                )
                for idx, (record, inflow, power) in enumerate(zip(self.records, inflows, powers), start=1):
                    writer.writerow(
                        [
                            idx,
                            f"{record['x']:.2f}",
                            f"{record['y']:.2f}",
                            f"{record['hub_height']:.1f}",
                            f"{record['rotor_diameter']:.1f}",
                            f"{inflow:.4f}",
                            f"{power:.2f}",
                        ]
                    )

            vel_agl = wind.get_velocity_at_agl(80.0)
            u = vel_agl["u"]
            v = vel_agl["v"]
            ws_agl = np.sqrt(u ** 2 + v ** 2)

            x_grid = np.linspace(wind.xmin, wind.xmax, wind.nx)
            y_grid = np.linspace(wind.ymin, wind.ymax, wind.ny)

            fig, ax = plt.subplots(figsize=(10, 8))
            contour = ax.contourf(x_grid, y_grid, ws_agl, levels=50, cmap="viridis")
            cbar = fig.colorbar(contour, ax=ax)
            cbar.set_label("Wind speed at 80 m AGL [m/s]")
            ax.scatter(self.xs, self.ys, color="white", marker="^", s=12, alpha=0.8, label="Alta turbines")
            ax.set_title("Alta Wind Energy Center Hub-Height Wind Speed")
            ax.set_xlabel("Easting [m] (UTM Zone 11N)")
            ax.set_ylabel("Northing [m] (UTM Zone 11N)")
            ax.grid(True, linestyle="--", alpha=0.3)
            ax.legend(loc="upper right")
            fig.tight_layout()
            fig.savefig(WAKE_IMAGE, dpi=300)
            plt.close(fig)

            self.assertGreater(float(np.min(inflows)), 0.0)
            self.assertGreater(float(np.max(inflows)), float(np.min(inflows)))
            self.assertGreater(float(np.sum(powers)), 0.0)

            wind.finalize()
        finally:
            os.chdir(old_cwd)


if __name__ == "__main__":
    unittest.main()
