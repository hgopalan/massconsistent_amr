#!/usr/bin/env python3
"""
coupled_wind_fire_example.py - Demonstration of coupled massconsistent_amr + wildfire simulation

This script shows how to run a coupled wind-fire simulation where:
1. massconsistent_amr wind solver computes mass-consistent 3D wind
2. Wind data is passed to wildfire_levelset fire solver
3. Fire solver advances one timestep
4. Process repeats (optionally with feedback from fire to wind)

This demonstrates the workflow for two-way coupling between wind and fire solvers.

Usage:
    # Assuming both massconsistent_amr and wildfire_levelset are built with Python bindings
    PYTHONPATH=wind_build/python:fire_build/python python3 coupled_wind_fire_example.py
"""

import sys
import os
import numpy as np

def check_dependencies():
    """Check if required modules are available"""
    missing = []
    
    try:
        import pyWindSolver
    except ImportError:
        missing.append("pyWindSolver (build massconsistent_amr with -DMASSCONSISTENT_BUILD_PYTHON_BINDINGS=ON)")
    
    try:
        from wildfire_solver import WildfireSolver
    except ImportError:
        missing.append("wildfire_solver (build wildfire_levelset with -DLEVELSET_BUILD_PYTHON_BINDINGS=ON)")
    
    if missing:
        print("Error: Missing required modules:")
        for m in missing:
            print(f"  - {m}")
        print("\nMake sure to:")
        print("  1. Build both solvers with Python bindings enabled")
        print("  2. Set PYTHONPATH to include both build/python directories")
        print("  Example: PYTHONPATH=wind_build/python:fire_build/python")
        return False
    
    return True


def create_test_inputs():
    """Create minimal test inputs for both solvers"""
    
    # Wind solver inputs
    wind_terrain = """0.0 0.0 100.0
1000.0 0.0 100.0
0.0 1000.0 100.0
1000.0 1000.0 120.0
500.0 500.0 110.0
250.0 250.0 105.0
750.0 750.0 115.0
"""
    
    wind_inputs = """# Wind solver inputs
terrain_file = /tmp/coupled_terrain.csv
U_ref = 8.0
V_ref = 2.0
z_ref = 10.0
z0 = 0.1
dx = 50.0
dy = 50.0
dz = 50.0
domain_height = 300.0
alpha_h = 1.0
alpha_v = 1.0
mlmg_verbose = 1
tol_rel = 1.e-8
"""
    
    # Fire solver inputs
    fire_inputs = """# Fire solver inputs
n_cell_x = 20
n_cell_y = 20
max_grid = 16

plo_x = 0.0
plo_y = 0.0
phi_x = 1000.0
phi_y = 1000.0

# Ignition
ignition.type = circle
ignition.x0 = 200.0
ignition.y0 = 200.0
ignition.radius = 50.0
ignition.time = 0.0

# Time control
cfl = 0.5
nsteps = 50
max_time = 600.0

# Propagation
propagation_method = levelset

# Fuel model
rothermel.model_number = 1

# Output
plot_interval = 10
"""
    
    # Write files
    with open("/tmp/coupled_terrain.csv", "w") as f:
        f.write(wind_terrain)
    
    with open("/tmp/coupled_wind.i", "w") as f:
        f.write(wind_inputs)
    
    with open("/tmp/coupled_fire.i", "w") as f:
        f.write(fire_inputs)
    
    print("✓ Created test input files")
    return "/tmp/coupled_wind.i", "/tmp/coupled_fire.i"


def run_coupled_simulation():
    """
    Run coupled wind-fire simulation.
    
    The workflow is:
    1. Initialize both solvers
    2. Solve wind field (mass-consistent)
    3. Extract 3D wind and pass to fire solver
    4. Advance fire solver
    5. Optionally: Extract heat release from fire and feed back to wind
    6. Repeat steps 2-5
    """
    
    print("\n" + "=" * 70)
    print("Coupled Wind-Fire Simulation Example")
    print("=" * 70 + "\n")
    
    # Import modules
    from wind_solver import WindSolver
    from wildfire_solver import WildfireSolver
    
    # Create input files
    wind_inputs, fire_inputs = create_test_inputs()
    
    try:
        # Initialize solvers
        print("Initializing wind solver...")
        wind = WindSolver(wind_inputs)
        
        print("\nInitializing fire solver...")
        fire = WildfireSolver(fire_inputs)
        
        # Check domain compatibility
        print("\nDomain compatibility check:")
        print(f"  Wind domain: X=[{wind.xmin:.1f}, {wind.xmax:.1f}], Y=[{wind.ymin:.1f}, {wind.ymax:.1f}]")
        print(f"  Fire domain: X=[{fire.xmin:.1f}, {fire.xmax:.1f}], Y=[{fire.ymin:.1f}, {fire.ymax:.1f}]")
        
        if (abs(wind.xmin - fire.xmin) > 1.0 or abs(wind.xmax - fire.xmax) > 1.0 or
            abs(wind.ymin - fire.ymin) > 1.0 or abs(wind.ymax - fire.ymax) > 1.0):
            print("  ⚠️  Warning: Domain bounds don't match exactly")
        else:
            print("  ✓ Domains match")
        
        # Solve initial wind field
        print("\nSolving initial mass-consistent wind field...")
        wind.solve()
        
        # Get 3D wind velocity
        vel_3d = wind.get_velocity()
        u_3d, v_3d, w_3d = vel_3d['u'], vel_3d['v'], vel_3d['w']
        
        print(f"\nWind field statistics:")
        print(f"  U: mean={u_3d.mean():.2f} m/s, range=[{u_3d.min():.2f}, {u_3d.max():.2f}]")
        print(f"  V: mean={v_3d.mean():.2f} m/s, range=[{v_3d.min():.2f}, {v_3d.max():.2f}]")
        print(f"  W: mean={w_3d.mean():.2f} m/s, range=[{w_3d.min():.2f}, {w_3d.max():.2f}]")
        
        # Pass 3D wind to fire solver
        print("\nPassing 3D wind field to fire solver...")
        fire.update_wind_3d(u_3d, v_3d, w_3d, wind.nz, wind.zmin, wind.zmax)
        
        # Run coupled time loop
        print("\nRunning coupled simulation...")
        print("-" * 70)
        
        num_wind_updates = 5  # Update wind every N fire steps
        max_fire_steps = 20
        
        for step in range(max_fire_steps):
            # Advance fire solver
            fire.step()
            
            # Get fire state
            state = fire.get_state()
            
            # Calculate burned area
            burned_cells = np.sum(state['phi'] <= 0.0)
            total_cells = state['phi'].size
            burned_fraction = burned_cells / total_cells * 100
            
            print(f"Step {step + 1}: t={state['time']:.1f}s, burned={burned_fraction:.1f}%")
            
            # Periodically update wind (in real coupling, wind might respond to fire)
            if (step + 1) % num_wind_updates == 0 and step < max_fire_steps - 1:
                print("  → Updating wind field...")
                
                # In a real two-way coupling, you might:
                # 1. Extract heat release from fire
                # 2. Add heat source to wind solver
                # 3. Re-solve wind field
                # For this example, we just keep the same wind
                
                # Re-extract wind (could be time-varying or respond to fire)
                vel_3d = wind.get_velocity()
                u_3d, v_3d, w_3d = vel_3d['u'], vel_3d['v'], vel_3d['w']
                fire.update_wind_3d(u_3d, v_3d, w_3d, wind.nz, wind.zmin, wind.zmax)
        
        print("-" * 70)
        
        # Get final state
        final_state = fire.get_state()
        burned_cells = np.sum(final_state['phi'] <= 0.0)
        total_cells = final_state['phi'].size
        burned_fraction = burned_cells / total_cells * 100
        
        print(f"\nFinal state:")
        print(f"  Time: {final_state['time']:.1f} s")
        print(f"  Burned area: {burned_fraction:.1f}%")
        print(f"  Max ROS: {final_state['ros'].max():.2f} m/s")
        print(f"  Max intensity: {final_state['intensity'].max():.0f} kW/m")
        
        # Write output
        print("\nWriting output files...")
        wind.write_plotfile("plt_wind_coupled")
        fire.write_plotfile("plt_fire_coupled")
        
        print("\n✓ Coupled simulation completed successfully!")
        print("\nOutput files:")
        print("  - plt_wind_coupled (AMReX plotfile)")
        print("  - plt_fire_coupled_* (AMReX plotfiles)")
        
        # Cleanup
        wind.finalize()
        fire.finalize()
        
        return True
        
    except Exception as e:
        print(f"\n✗ Simulation failed with error: {e}")
        import traceback
        traceback.print_exc()
        return False
    
    finally:
        # Cleanup temp files
        for f in ["/tmp/coupled_terrain.csv", "/tmp/coupled_wind.i", "/tmp/coupled_fire.i"]:
            if os.path.exists(f):
                os.remove(f)


def main():
    """Main entry point"""
    
    if not check_dependencies():
        return 1
    
    success = run_coupled_simulation()
    
    return 0 if success else 1


if __name__ == "__main__":
    sys.exit(main())
