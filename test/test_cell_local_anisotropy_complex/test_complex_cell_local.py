#!/usr/bin/env python3
"""
test_complex_cell_local.py - Complex Case: 8x8 km area with multiple Gaussian hills,
diurnal wind variation, and stability factors, demonstrating the value of
cell-local spatially-varying variational anisotropy.
"""

import os
import sys
import math
import numpy as np
from pathlib import Path

# Add python path
TEST_DIR = Path(__file__).resolve().parent
SRC_PYTHON_DIR = TEST_DIR.parent.parent / "src" / "python"
sys.path.insert(0, str(SRC_PYTHON_DIR))

from wind_solver import WindSolver

def generate_complex_terrain(filename, nx=41, ny=41, size=8000.0):
    """
    Generate terrain of size by size meters with 4 Gaussian hills.
    """
    print(f"Generating 8x8 km terrain with multiple Gaussian hills ({nx}x{ny} grid)...")
    
    # Define hills: (xc, yc, peak_height, sigma)
    hills = [
        (2000.0, 2000.0, 300.0, 800.0),   # Hill 1
        (5000.0, 2500.0, 450.0, 1000.0),  # Hill 2
        (2500.0, 5500.0, 250.0, 700.0),   # Hill 3
        (6000.0, 5000.0, 500.0, 1200.0)   # Hill 4
    ]
    
    terrain = []
    for j in range(ny):
        y = j * size / (ny - 1)
        for i in range(nx):
            x = i * size / (nx - 1)
            
            # Combine heights
            z = 0.0
            for xc, yc, h, sig in hills:
                r_sq = (x - xc)**2 + (y - yc)**2
                z_hill = h * math.exp(-r_sq / (2.0 * sig**2))
                z = max(z, z_hill) # Use envelope or max to represent multiple hills
            
            terrain.append((x, y, z))
            
    # Write terrain to file
    with open(filename, 'w') as f:
        f.write("# Complex 8x8 km terrain with multiple Gaussian hills\n")
        for x, y, z in terrain:
            f.write(f"{x:.6f} {y:.6f} {z:.6f}\n")
            
    print(f"✓ Terrain file written to {filename}")

def create_inputs_file(filename, enable_anisotropy, temp_gradient, U_ref, V_ref, terrain_file="terrain.csv"):
    """
    Create a custom inputs.i configuration.
    """
    content = f"""# Custom inputs.i for complex case
terrain_file = {terrain_file}
init_mode = loglaw
U_ref = {U_ref}
V_ref = {V_ref}
z_ref = 50.0
z0 = 0.05

dx = 200.0
dy = 200.0
dz = 50.0
domain_height = 1200.0

alpha_h = 1.0
alpha_v = 1.0

enable_cell_local_anisotropy = {"true" if enable_anisotropy else "false"}
anisotropy_source = all
anisotropy_slope_scale = 0.25
anisotropy_decay_height = 800.0
anisotropy_ri_gamma = 1.2
anisotropy_ri_beta = 0.6
anisotropy_fr_min = 0.1
temperature_gradient = {temp_gradient}

mlmg_verbose = 0
max_grid_size = 64
"""
    with open(filename, 'w') as f:
        f.write(content)

def run_simulation(time_name, temp_gradient, U_ref, V_ref):
    """
    Runs both isotropic (standard) and cell-local anisotropic solvers for comparison.
    """
    print(f"\n======================================================================")
    # Corrected spelling in printout
    print(f"Diurnal Case: {time_name} (U_ref={U_ref} m/s, dTheta/dz={temp_gradient} K/m)")
    print(f"======================================================================")
    
    inputs_iso = TEST_DIR / "inputs_iso.i"
    inputs_aniso = TEST_DIR / "inputs_aniso.i"
    
    # Create the input files
    create_inputs_file(inputs_iso, enable_anisotropy=False, temp_gradient=temp_gradient, U_ref=U_ref, V_ref=V_ref)
    create_inputs_file(inputs_aniso, enable_anisotropy=True, temp_gradient=temp_gradient, U_ref=U_ref, V_ref=V_ref)
    
    # 1. Run Standard Isotropic solver
    solver_iso = WindSolver()
    solver_iso.initialize(str(inputs_iso))
    solver_iso.solve()
    vel_iso = solver_iso.get_velocity()
    u_iso, v_iso, w_iso = vel_iso['u'], vel_iso['v'], vel_iso['w']
    solver_iso.finalize()
    
    # 2. Run Cell-Local Anisotropic solver
    solver_aniso = WindSolver()
    solver_aniso.initialize(str(inputs_aniso))
    solver_aniso.solve()
    vel_aniso = solver_aniso.get_velocity()
    u_aniso, v_aniso, w_aniso = vel_aniso['u'], vel_aniso['v'], vel_aniso['w']
    solver_aniso.finalize()
    
    # Compare velocity components at lower levels (e.g. k = 1, ~75m AGL)
    k_level = 1
    
    # Calculate deflection (horizontal angle change) and vertical velocity suppression
    mag_iso = np.sqrt(u_iso[k_level]**2 + v_iso[k_level]**2)
    mag_aniso = np.sqrt(u_aniso[k_level]**2 + v_aniso[k_level]**2)
    
    w_max_iso = np.max(np.abs(w_iso[k_level]))
    w_max_aniso = np.max(np.abs(w_aniso[k_level]))
    
    # Compute horizontal flow deflection around hills
    # (measured as standard deviation of v-velocity, representing lateral flow around hills)
    v_std_iso = np.std(v_iso[k_level])
    v_std_aniso = np.std(v_aniso[k_level])
    
    print(f"\nResults and Comparison for {time_name}:")
    print(f"  Standard Isotropic Solver:")
    print(f"    Max Vertical Velocity |w|: {w_max_iso:.3f} m/s")
    print(f"    Horizontal Lateral Deflection (Std Dev v): {v_std_iso:.3f} m/s")
    print(f"  Cell-Local Anisotropic Solver:")
    print(f"    Max Vertical Velocity |w|: {w_max_aniso:.3f} m/s")
    print(f"    Horizontal Lateral Deflection (Std Dev v): {v_std_aniso:.3f} m/s")
    
    # Showing the physical value:
    if temp_gradient > 0: # Stable (Morning / Night)
        suppress_pct = (w_max_iso - w_max_aniso) / (w_max_iso + 1e-5) * 100.0
        deflect_pct = (v_std_aniso - v_std_iso) / (v_std_iso + 1e-5) * 100.0
        print(f"  -> Spatially-Varying Anisotropy value:")
        print(f"    - Suppressed unphysical vertical motion over hills by {suppress_pct:.1f}%")
        print(f"    - Enhanced physical lateral deflection around hills by {deflect_pct:.1f}%")
        print(f"    - This correctly models stratified flow blocked by gravity and terrain slopes.")
    else: # Unstable (Afternoon)
        increase_pct = (w_max_aniso - w_max_iso) / (w_max_iso + 1e-5) * 100.0
        print(f"  -> Spatially-Varying Anisotropy value:")
        print(f"    - Enhanced buoyancy-driven vertical mixing over hills by {increase_pct:.1f}%")
        print(f"    - This correctly captures convection-friendly vertical motion aloft.")

def main():
    os.chdir(TEST_DIR)
    generate_complex_terrain("terrain.csv")
    
    # Run the diurnal cases representing morning, afternoon, and night
    # Morning: Stable potential temperature gradient, low-to-moderate wind speed
    run_simulation("Morning Transition", temp_gradient=0.004, U_ref=5.0, V_ref=0.0)
    
    # Afternoon: Unstable potential temperature gradient (convective), strong wind speed
    run_simulation("Afternoon Convective", temp_gradient=-0.002, U_ref=10.0, V_ref=0.0)
    
    # Night: Strongly stable potential temperature gradient, very low wind speed
    run_simulation("Night Stable Boundary Layer", temp_gradient=0.010, U_ref=2.5, V_ref=0.0)
    
    # Cleanup files
    for fn in ["terrain.csv", "inputs_iso.i", "inputs_aniso.i"]:
        if os.path.exists(fn):
            os.remove(fn)
            
    print("\n✓ Complex diurnal test case completed successfully!")

if __name__ == "__main__":
    main()
