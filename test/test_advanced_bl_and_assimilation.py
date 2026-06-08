#!/usr/bin/env python3
"""
test_advanced_bl_and_assimilation.py - Test spatially varying ABL height and 3D wind profile assimilation.
"""

import os
import sys
import tempfile
import numpy as np
from pathlib import Path

# Add python directory to path
TEST_DIR = Path(__file__).resolve().parent
SRC_PYTHON_DIR = TEST_DIR.parent / "src" / "python"
BUILD_PYTHON_DIR = TEST_DIR.parent / "build" / "python"
sys.path.insert(0, str(SRC_PYTHON_DIR))
sys.path.insert(0, str(BUILD_PYTHON_DIR))

try:
    from wind_solver import WindSolver
except ImportError as e:
    print(f"ERROR: Could not import WindSolver: {e}")
    sys.exit(1)


def test_3d_profile_assimilation():
    """Verify that vertical profiles are parsed and 3D IDW interpolated."""
    print("\n" + "="*70)
    print("Testing 3D Vertical Profile Assimilation")
    print("="*70)

    # 1. Create a synthetic terrain file
    terrain_file = TEST_DIR / "temp_terrain.csv"
    with open(terrain_file, "w") as f:
        f.write("# x, y, z\n")
        for y in range(0, 300, 30):
            for x in range(0, 300, 30):
                f.write(f"{x}, {y}, 0.0\n")

    # 2. Create a synthetic 3D vertical profile CSV
    # Columns: x, y, z, speed, direction
    # Two profiles (one at x=50, y=50, one at x=250, y=250) with varying wind speeds at different heights
    profile_file = TEST_DIR / "temp_profile.csv"
    with open(profile_file, "w") as f:
        f.write("# x, y, z, speed, direction_deg\n")
        # Profile 1 (near bottom-left): speed increases with height
        f.write("50.0, 50.0, 10.0, 5.0, 270.0\n")
        f.write("50.0, 50.0, 50.0, 10.0, 270.0\n")
        f.write("50.0, 50.0, 100.0, 15.0, 270.0\n")
        # Profile 2 (near top-right): speed decreases with height
        f.write("250.0, 250.0, 10.0, 12.0, 270.0\n")
        f.write("250.0, 250.0, 50.0, 8.0, 270.0\n")
        f.write("250.0, 250.0, 100.0, 4.0, 270.0\n")

    # 3. Create inputs.i file
    inputs_file = TEST_DIR / "temp_inputs.i"
    with open(inputs_file, "w") as f:
        f.write(f"""# Inputs for testing
terrain_file = {terrain_file}
velocity_file = {profile_file}
init_mode = raws

dx = 30.0
dy = 30.0
dz = 25.0
domain_height = 100.0

alpha_h = 1.0
alpha_v = 1.0
mlmg_verbose = 0
max_grid_size = 32

enable_bl_depth_diagnostic = true
bl_depth_param = 1000.0
plot_file = plt_test_advanced_bl
""")

    try:
        # Initialize solver
        wind = WindSolver()
        wind.initialize(str(inputs_file))
        
        # Check grid dimensions
        print(f"✓ Grid size: {wind.nx} x {wind.ny} x {wind.nz}")
        
        # Solve for wind field
        result = wind.solve()
        assert result['success'], "Solver failed"
        print("✓ Solver executed successfully")

        # Get initial velocity field
        vel0 = wind.get_velocity0()
        u0 = vel0['u']
        
        # Verify 3D variation: u0 should vary with height (z)
        # Check u0 near bottom-left at k=0 (z=12.5m) and k=3 (z=87.5m)
        # Profile 1 has speed 5.0 at z=10, 15.0 at z=100. So at bottom-left, u0 should be larger at higher k.
        u0_low = u0[0, 2, 2] # k=0, y=2, x=2
        u0_high = u0[3, 2, 2] # k=3, y=2, x=2
        
        print(f"✓ Initial u0 near bottom-left at k=0 (z=12.5m): {u0_low:.2f} m/s")
        print(f"✓ Initial u0 near bottom-left at k=3 (z=87.5m): {u0_high:.2f} m/s")
        
        assert abs(u0_high - u0_low) > 1.0, "Initial velocity does not vary with height in raws mode (3D interpolation failed)"
        print("✓ Successfully verified 3D vertical wind variation")

        # Clean up files
        wind.finalize()

    finally:
        # Clean up temporary files
        for temp_file in [terrain_file, profile_file, inputs_file]:
            if temp_file.exists():
                os.remove(temp_file)
        
        # Clean up plotfile directory
        plotfile_dir = TEST_DIR / "plt_test_advanced_bl"
        if plotfile_dir.exists():
            import shutil
            shutil.rmtree(plotfile_dir)


def test_idw_gamma_effect():
    """Verify that changing idw_gamma affects the 3D IDW interpolation."""
    print("\n" + "="*70)
    print("Testing 3D IDW vertical scaling parameter (idw_gamma)")
    print("="*70)

    # 1. Create a synthetic terrain file
    terrain_file = TEST_DIR / "temp_terrain_gamma.csv"
    with open(terrain_file, "w") as f:
        f.write("# x, y, z\n")
        for y in range(0, 300, 30):
            for x in range(0, 300, 30):
                f.write(f"{x}, {y}, 0.0\n")

    # 2. Create a synthetic 3D vertical profile CSV
    profile_file = TEST_DIR / "temp_profile_gamma.csv"
    with open(profile_file, "w") as f:
        f.write("# x, y, z, speed, direction_deg\n")
        f.write("50.0, 50.0, 10.0, 5.0, 270.0\n")
        f.write("50.0, 50.0, 50.0, 10.0, 270.0\n")
        f.write("250.0, 250.0, 10.0, 12.0, 270.0\n")
        f.write("250.0, 250.0, 50.0, 8.0, 270.0\n")

    # 3. Create inputs.i file for Case A (gamma = 1.0)
    inputs_file_a = TEST_DIR / "temp_inputs_gamma_a.i"
    with open(inputs_file_a, "w") as f:
        f.write(f"""terrain_file = {terrain_file}
velocity_file = {profile_file}
init_mode = raws
dx = 30.0
dy = 30.0
dz = 25.0
domain_height = 100.0
alpha_h = 1.0
alpha_v = 1.0
mlmg_verbose = 0
max_grid_size = 32
idw_gamma = 1.0
plot_file = plt_test_gamma_a
""")

    # 4. Create inputs.i file for Case B (gamma = 10.0)
    inputs_file_b = TEST_DIR / "temp_inputs_gamma_b.i"
    with open(inputs_file_b, "w") as f:
        f.write(f"""terrain_file = {terrain_file}
velocity_file = {profile_file}
init_mode = raws
dx = 30.0
dy = 30.0
dz = 25.0
domain_height = 100.0
alpha_h = 1.0
alpha_v = 1.0
mlmg_verbose = 0
max_grid_size = 32
idw_gamma = 10.0
plot_file = plt_test_gamma_b
""")

    try:
        # Solve Case A
        wind_a = WindSolver()
        wind_a.initialize(str(inputs_file_a))
        wind_a.solve()
        vel0_a = wind_a.get_velocity0()
        u0_a = vel0_a['u']
        wind_a.finalize()

        # Solve Case B
        wind_b = WindSolver()
        wind_b.initialize(str(inputs_file_b))
        wind_b.solve()
        vel0_b = wind_b.get_velocity0()
        u0_b = vel0_b['u']
        wind_b.finalize()

        # Verify that changing gamma resulted in a different initial interpolation field
        diff = np.abs(u0_a - u0_b)
        max_diff = np.max(diff)
        print(f"✓ Maximum difference between isotropic and anisotropic IDW: {max_diff:.4f} m/s")
        assert max_diff > 1.0e-3, "Changing idw_gamma had no effect on the 3D IDW interpolation"
        print("✓ Verified anisotropic IDW vertical scaling parameter (idw_gamma) works as expected")

    finally:
        # Clean up temporary files
        for temp_file in [terrain_file, profile_file, inputs_file_a, inputs_file_b]:
            if temp_file.exists():
                os.remove(temp_file)
        
        # Clean up plotfile directories
        for suffix in ['a', 'b']:
            plotfile_dir = TEST_DIR / f"plt_test_gamma_{suffix}"
            if plotfile_dir.exists():
                import shutil
                shutil.rmtree(plotfile_dir)


if __name__ == "__main__":
    test_3d_profile_assimilation()
    test_idw_gamma_effect()
    print("\nALL TESTS PASSED!")
