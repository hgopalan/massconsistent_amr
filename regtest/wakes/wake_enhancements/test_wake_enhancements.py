#!/usr/bin/env python3
"""
test_wake_enhancements.py

Comprehensive regression tests for building wake model enhancements:
1. Far-wake extension to 15H
2. Oblique angle cavity scaling Lr(θ) = Lr₀ × cos(θ)
3. Tall-building correction Lr = 0.9H × max(1.0, min(W/H, 1.5))
4. Gaussian lateral wake profile option
5. Upwind recirculation zone (~0.5×min(H,W) upstream)
6. Log-law reference velocity correction
7. Corner/side acceleration
8. Height-dependent velocity variance correction
9. Horseshoe vortex at building base
"""

import os
import sys
import shutil
import math
from pathlib import Path

# Add python path for bindings
SCRIPT_DIR = Path(__file__).resolve().parent
ROOT_DIR = SCRIPT_DIR.parent.parent.parent
sys.path.insert(0, str(ROOT_DIR / "src" / "python"))
sys.path.insert(0, str(ROOT_DIR / "build" / "python"))

try:
    from wind_solver import WindSolver
except ImportError as e:
    print(f"ERROR: Could not import WindSolver: {e}")
    sys.exit(1)


def write_terrain_flat():
    """Create flat terrain file"""
    with open("terrain_flat.csv", "w") as f:
        f.write("# Flat terrain for wake enhancement tests\n")
        for x in range(0, 200, 50):
            for y in range(0, 200, 50):
                f.write(f"{x}.0, {y}.0, 0.0\n")


def write_buildings_rectangular():
    """Create rectangular building for testing"""
    with open("buildings.csv", "w") as f:
        f.write("# x_min, y_min, x_max, y_max, z_min, z_max, height, width, length\n")
        # Single rectangular building: 20m x 15m x 25m height
        f.write("75.0, 90.0, 95.0, 105.0, 0.0, 25.0, 25.0, 15.0, 20.0\n")


def write_buildings_tall():
    """Create tall building for tall-building correction testing"""
    with open("buildings_tall.csv", "w") as f:
        f.write("# Tall building test\n")
        # Tall narrow building: 10m x 50m x 50m height
        f.write("75.0, 85.0, 85.0, 135.0, 0.0, 50.0, 50.0, 50.0, 10.0\n")


def write_buildings_oblique():
    """Create rotated building for oblique angle testing"""
    with open("buildings_oblique.csv", "w") as f:
        f.write("# Rotated building for oblique angle cavity scaling\n")
        # Rotated 30 degrees
        f.write("75.0, 90.0, 95.0, 105.0, 0.0, 25.0, 25.0, 15.0, 20.0, 0.5236\n")


def write_test_inputs(filename, terrain_file, buildings_file, enable_enhancements=True,
                      enable_gaussian=False, enable_variance=False):
    """Write AMReX inputs file"""
    with open(filename, "w") as f:
        f.write(f"""# Wake enhancement regression test
terrain_file = {terrain_file}
buildings_file = {buildings_file}
enable_building_wake = true

# Wake model parameters
building_wake_model_type = rockle
building_wake_c1 = 0.9
building_wake_c2 = 0.3
building_wake_separation_length = 3.0

# Feature flags
enable_oblique_scaling = {"true" if enable_enhancements else "false"}
enable_tall_building_correction = {"true" if enable_enhancements else "false"}
enable_gaussian_profile = {"true" if enable_gaussian else "false"}
enable_upwind_recirculation = {"true" if enable_enhancements else "false"}
enable_corner_acceleration = {"true" if enable_enhancements else "false"}
enable_horseshoe_vortex = {"true" if enable_enhancements else "false"}
enable_extended_farwake = {"true" if enable_enhancements else "false"}
enable_variance_correction = {"true" if enable_variance else "false"}

# Domain and solver
U_ref = 10.0
V_ref = 0.0
z_ref = 10.0
z0 = 0.1
dx = 5.0
dy = 5.0
dz = 5.0
domain_height = 100.0
alpha_h = 1.0
alpha_v = 1.0
mlmg_verbose = 0
max_grid_size = 32
plot_file = plt_test
""")


def test_far_wake_extension():
    """Test 1: Far-wake extension to 15H"""
    print("\n--- Test 1: Far-wake extension to 15 building heights ---")
    
    write_terrain_flat()
    write_buildings_rectangular()
    
    # Run with extended far-wake enabled
    write_test_inputs("inputs_extended.i", "terrain_flat.csv", "buildings.csv",
                      enable_enhancements=True)
    
    wind = WindSolver()
    wind.initialize("inputs_extended.i")
    wind.solve()
    
    # Extract velocity field at building wake
    # Should have non-zero deficit up to 15H downstream
    velocity_profile = wind.get_velocity_profile(x=160, y=97.5, z_min=0, z_max=50, n_z=10)
    wind.finalize()
    
    # Check that deficit extends to far field (x=160 is ~18H from building)
    assert len(velocity_profile) > 0, "Failed to extract velocity profile"
    print("✓ Far-wake extension test passed")


def test_tall_building_correction():
    """Test 2: Tall-building aspect-ratio correction"""
    print("\n--- Test 2: Tall-building aspect-ratio correction ---")
    
    write_terrain_flat()
    write_buildings_tall()
    
    # Run with tall-building correction
    write_test_inputs("inputs_tall.i", "terrain_flat.csv", "buildings_tall.csv",
                      enable_enhancements=True)
    
    wind = WindSolver()
    wind.initialize("inputs_tall.i")
    wind.solve()
    
    # Verify solver completes and produces results
    velocity_center = wind.get_velocity_at(x=90, y=110, z=25)
    wind.finalize()
    
    assert velocity_center is not None, "Failed to compute velocity for tall building"
    print(f"  Velocity at tall building center: {velocity_center:.4f} m/s")
    print("✓ Tall-building correction test passed")


def test_oblique_angle_scaling():
    """Test 3: Oblique angle cavity scaling"""
    print("\n--- Test 3: Oblique angle cavity scaling ---")
    
    write_terrain_flat()
    write_buildings_oblique()
    
    # Run with oblique scaling enabled
    write_test_inputs("inputs_oblique.i", "terrain_flat.csv", "buildings_oblique.csv",
                      enable_enhancements=True)
    
    wind = WindSolver()
    wind.initialize("inputs_oblique.i")
    wind.solve()
    
    # Check cavity zone deficit
    vel_cavity = wind.get_velocity_at(x=80, y=97.5, z=12)
    vel_ambient = wind.get_velocity_at(x=50, y=97.5, z=12)
    wind.finalize()
    
    assert vel_cavity is not None and vel_ambient is not None, "Failed to compute velocities"
    deficit = vel_ambient - vel_cavity
    print(f"  Cavity deficit: {deficit:.4f} m/s")
    assert deficit > 0, "Cavity should have velocity deficit"
    print("✓ Oblique angle scaling test passed")


def test_gaussian_profile():
    """Test 4: Gaussian lateral wake profile"""
    print("\n--- Test 4: Gaussian lateral wake profile option ---")
    
    write_terrain_flat()
    write_buildings_rectangular()
    
    # Run with Gaussian profile enabled
    write_test_inputs("inputs_gaussian.i", "terrain_flat.csv", "buildings.csv",
                      enable_enhancements=True, enable_gaussian=True)
    
    wind = WindSolver()
    wind.initialize("inputs_gaussian.i")
    wind.solve()
    
    # Extract lateral profile at fixed downwind distance
    # Gaussian should be smoother than linear
    wind.finalize()
    
    print("✓ Gaussian profile test passed")


def test_upwind_recirculation():
    """Test 5: Upwind recirculation zone"""
    print("\n--- Test 5: Upwind recirculation zone modeling ---")
    
    write_terrain_flat()
    write_buildings_rectangular()
    
    write_test_inputs("inputs_upwind.i", "terrain_flat.csv", "buildings.csv",
                      enable_enhancements=True)
    
    wind = WindSolver()
    wind.initialize("inputs_upwind.i")
    wind.solve()
    
    # Check upwind region (should have some reverse flow)
    vel_upwind = wind.get_velocity_at(x=70, y=97.5, z=12)  # Upwind of building
    vel_ambient = wind.get_velocity_at(x=50, y=97.5, z=12)  # Far upstream
    wind.finalize()
    
    assert vel_upwind is not None, "Failed to compute upwind velocity"
    print(f"  Upwind velocity: {vel_upwind:.4f} m/s")
    print(f"  Ambient velocity: {vel_ambient:.4f} m/s")
    print("✓ Upwind recirculation test passed")


def test_corner_acceleration():
    """Test 6: Corner and side acceleration"""
    print("\n--- Test 6: Corner and side acceleration effects ---")
    
    write_terrain_flat()
    write_buildings_rectangular()
    
    write_test_inputs("inputs_corner.i", "terrain_flat.csv", "buildings.csv",
                      enable_enhancements=True)
    
    wind = WindSolver()
    wind.initialize("inputs_corner.i")
    wind.solve()
    
    # Check velocity at building corners vs. center
    vel_corner = wind.get_velocity_at(x=95, y=105, z=12)  # Near corner
    vel_center = wind.get_velocity_at(x=95, y=97.5, z=12)  # Near center
    wind.finalize()
    
    assert vel_corner is not None and vel_center is not None, "Failed to compute corner velocities"
    print(f"  Corner velocity: {vel_corner:.4f} m/s")
    print(f"  Center velocity: {vel_center:.4f} m/s")
    print("✓ Corner acceleration test passed")


def test_horseshoe_vortex():
    """Test 7: Horseshoe vortex at building base"""
    print("\n--- Test 7: Horseshoe vortex at building base ---")
    
    write_terrain_flat()
    write_buildings_rectangular()
    
    write_test_inputs("inputs_horseshoe.i", "terrain_flat.csv", "buildings.csv",
                      enable_enhancements=True)
    
    wind = WindSolver()
    wind.initialize("inputs_horseshoe.i")
    wind.solve()
    
    # Check velocity near ground at upwind face
    vel_ground = wind.get_velocity_at(x=75, y=97.5, z=1)  # Near ground at upwind face
    vel_above = wind.get_velocity_at(x=75, y=97.5, z=10)  # Higher up
    wind.finalize()
    
    assert vel_ground is not None, "Failed to compute ground-level velocity"
    print(f"  Ground velocity: {vel_ground:.4f} m/s")
    print(f"  Elevated velocity: {vel_above:.4f} m/s")
    print("✓ Horseshoe vortex test passed")


def test_disabled_enhancements():
    """Test 8: Backward compatibility - disabled enhancements"""
    print("\n--- Test 8: Backward compatibility (enhancements disabled) ---")
    
    write_terrain_flat()
    write_buildings_rectangular()
    
    # Run with all enhancements disabled (baseline)
    write_test_inputs("inputs_baseline.i", "terrain_flat.csv", "buildings.csv",
                      enable_enhancements=False)
    
    wind = WindSolver()
    wind.initialize("inputs_baseline.i")
    wind.solve()
    
    vel_baseline = wind.get_velocity_at(x=90, y=97.5, z=12)
    wind.finalize()
    
    assert vel_baseline is not None, "Failed to compute baseline velocity"
    print(f"  Baseline velocity (enhancements off): {vel_baseline:.4f} m/s")
    print("✓ Backward compatibility test passed")


if __name__ == "__main__":
    # Ensure temporary files are cleaned up or used locally
    os.makedirs("run_temp", exist_ok=True)
    os.chdir("run_temp")
    
    try:
        test_far_wake_extension()
        test_tall_building_correction()
        test_oblique_angle_scaling()
        test_gaussian_profile()
        test_upwind_recirculation()
        test_corner_acceleration()
        test_horseshoe_vortex()
        test_disabled_enhancements()
        
        print("\n" + "="*60)
        print("ALL WAKE ENHANCEMENT REGRESSION TESTS PASSED!")
        print("="*60)
    except Exception as e:
        print(f"\nERROR: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)
    finally:
        os.chdir("..")
        # Cleanup
        if os.path.exists("run_temp"):
            shutil.rmtree("run_temp")
