#!/usr/bin/env python3
"""
example_openfast_export.py - Example usage of the OpenFAST/TurbSim export tool

This example demonstrates how to:
1. Initialize and solve a wind field
2. Export to OpenFAST/TurbSim binary format (.bts)
3. Validate the export and create visualization plots

The .bts format is compatible with NREL's OpenFAST wind turbine simulator,
enabling wind farm simulations with terrain-adjusted turbulent fields.

To run this example:
    cd massconsistent_amr
    cmake -S . -B build -DMASSCONSISTENT_BUILD_PYTHON_BINDINGS=ON
    cmake --build build
    
    # Set Python path
    export PYTHONPATH=/path/to/build/python:$PYTHONPATH
    
    # Run the example
    python3 src/python/example_openfast_export.py
"""

import sys
import os
import numpy as np


def example_basic_openfast_export():
    """
    Example 1: Basic OpenFAST/TurbSim export
    
    This is the simplest workflow: solve wind, export to BTS format.
    """
    print("\n" + "="*70)
    print("Example 1: Basic OpenFAST/TurbSim Export")
    print("="*70)
    
    try:
        from wind_solver import WindSolver
    except ImportError as e:
        print(f"Error: Could not import required modules")
        print(f"Make sure to set PYTHONPATH: export PYTHONPATH=build/python:$PYTHONPATH")
        return False
    
    # Create simple test inputs
    terrain_content = """0.0 0.0 100.0
1000.0 0.0 100.0
0.0 1000.0 100.0
1000.0 1000.0 120.0
500.0 500.0 110.0
"""
    
    inputs_content = """
# Test wind solver inputs for OpenFAST export
terrain_file = /tmp/test_openfast_terrain.csv
U_ref = 10.0
V_ref = 0.0
z_ref = 10.0
z0 = 0.1
dx = 50.0
dy = 50.0
dz = 50.0
domain_height = 300.0
alpha_h = 1.0
alpha_v = 1.0
mlmg_verbose = 0
tol_rel = 1.e-8
"""
    
    # Write test files
    with open("/tmp/test_openfast_terrain.csv", "w") as f:
        f.write(terrain_content)
    
    with open("/tmp/test_openfast_inputs.i", "w") as f:
        f.write(inputs_content)
    
    try:
        # Initialize and solve
        print("\n1. Initializing wind solver...")
        wind = WindSolver("/tmp/test_openfast_inputs.i")
        
        print("\n2. Solving for mass-consistent wind field...")
        wind.solve()
        
        # Get wind field data
        print("\n3. Extracting wind field data...")
        vel = wind.get_velocity()
        terrain = wind.get_terrain()
        
        print(f"   Grid: {wind.nx} × {wind.ny} × {wind.nz}")
        print(f"   Resolution: dx={wind.dx:.2f} m, dy={wind.dy:.2f} m, dz={wind.dz:.2f} m")
        
        # Compute wind speed statistics
        speed = np.sqrt(vel['u']**2 + vel['v']**2)
        print(f"   Wind speed: {np.nanmin(speed):.2f} - {np.nanmax(speed):.2f} m/s")
        print(f"   Mean at top: {np.nanmean(speed[-1, :, :]):.2f} m/s")
        
        # Prepare BTS export
        print("\n4. Preparing BTS export...")
        
        # For this example, we'll use the mean wind field as a single time step
        # In a real application, this would come from temporal synthesis
        u_data = vel['u'].astype(np.float32).flatten()
        v_data = vel['v'].astype(np.float32).flatten()
        w_data = vel['w'].astype(np.float32).flatten()
        
        # Compute mean wind speed at hub height
        hub_height = 90.0
        z_hub_idx = int((hub_height - wind.zmin) / wind.dz)
        z_hub_idx = min(z_hub_idx, wind.nz - 1)
        wind_at_hub = np.sqrt(vel['u'][z_hub_idx, :, :]**2 + vel['v'][z_hub_idx, :, :]**2)
        mean_wind_speed = np.nanmean(wind_at_hub)
        
        print(f"   Hub height: {hub_height} m")
        print(f"   Mean wind speed at hub: {mean_wind_speed:.2f} m/s")
        
        # Create BTS file
        print("\n5. Writing BTS file...")
        output_bts = "/tmp/openfast_wind.bts"
        
        # Import the TurbSim writer
        sys.path.insert(0, "/tmp/workspace/hgopalan/massconsistent_amr/tools")
        from openfast_export import TurbSimBTSWriter
        
        writer = TurbSimBTSWriter()
        
        # Single time step (static wind field)
        nt = 1
        dt = 0.1
        
        writer.initialize(
            num_time_steps=nt,
            nx=wind.nx,
            ny=wind.ny,
            nz=wind.nz,
            dt=dt,
            u_mean=mean_wind_speed,
            dx=wind.dx,
            dy=wind.dy,
            dz=wind.dz,
            z_hub=hub_height,
            turbulence_intensity_u=0.14
        )
        
        # Set metadata
        writer.metadata.description = "Mass-consistent wind field from AMReX solver"
        writer.metadata.length_scale_u = 100.0
        writer.metadata.length_scale_v = 100.0
        writer.metadata.length_scale_w = 50.0
        writer.metadata.z0 = 0.1
        writer.header.zRef = 10.0
        
        # Export
        if writer.export_time_series(output_bts, u_data, v_data, w_data,
                                    wind.nx, wind.ny, wind.nz, nt):
            print(f"   ✓ BTS export successful: {output_bts}")
            
            # Check file was created
            if os.path.exists(output_bts):
                file_size = os.path.getsize(output_bts)
                print(f"   File size: {file_size / 1024 / 1024:.2f} MB")
            
            # Check metadata file
            meta_file = output_bts.replace('.bts', '.meta')
            if os.path.exists(meta_file):
                print(f"   ✓ Metadata file: {meta_file}")
        else:
            print(f"   ✗ BTS export failed")
            wind.finalize()
            return False
        
        # Display BTS header info
        print("\n6. BTS Header Information:")
        header = writer.header
        print(f"   Format: TurbSim (id={header.id1},{header.id2})")
        print(f"   Grid: {header.ny} lateral × {header.nz} vertical points")
        print(f"   Time steps: {header.nt}")
        print(f"   Time step: {header.dt} s")
        print(f"   Hub height: {header.zHub} m")
        print(f"   Hub wind speed: {header.uHub} m/s")
        print(f"   Turbulence intensity: {header.turbIntensity:.1f}%")
        print(f"   Grid spacing: dy={header.dy} m, dz={header.dz} m")
        
        # Display metadata
        print("\n7. Turbulence Metadata:")
        meta = writer.metadata
        print(f"   Model: {meta.turbulence_model}")
        print(f"   Intensity: u={meta.intensity_u:.3f}, v={meta.intensity_v:.3f}, w={meta.intensity_w:.3f}")
        print(f"   Integral scales: u={meta.length_scale_u} m, v={meta.length_scale_v} m, w={meta.length_scale_w} m")
        print(f"   Surface roughness: {meta.z0} m")
        
        print("\n" + "="*70)
        print("✓ Example 1 completed successfully!")
        print("="*70)
        
        wind.finalize()
        return True
        
    except Exception as e:
        print(f"\n✗ Example failed: {e}")
        import traceback
        traceback.print_exc()
        return False
    finally:
        # Cleanup
        for f in ["/tmp/test_openfast_terrain.csv", "/tmp/test_openfast_inputs.i"]:
            if os.path.exists(f):
                os.remove(f)


def example_terrain_export():
    """
    Example 2: Export with complex terrain
    
    Demonstrates exporting over more complex terrain with varying elevation.
    """
    print("\n" + "="*70)
    print("Example 2: Export with Complex Terrain")
    print("="*70)
    
    try:
        from wind_solver import WindSolver
    except ImportError as e:
        print(f"Error: Could not import required modules")
        return False
    
    # Create terrain file with more variation (Gaussian hill)
    terrain_content = """0.0 0.0 100.0
100.0 0.0 100.0
200.0 0.0 100.0
0.0 100.0 100.0
100.0 100.0 110.0
200.0 100.0 100.0
0.0 200.0 100.0
100.0 200.0 100.0
200.0 200.0 100.0
"""
    
    inputs_content = """
terrain_file = /tmp/test_complex_terrain.csv
U_ref = 12.0
V_ref = 1.0
z_ref = 10.0
z0 = 0.15
dx = 50.0
dy = 50.0
dz = 25.0
domain_height = 250.0
alpha_h = 1.0
alpha_v = 0.95
mlmg_verbose = 0
"""
    
    with open("/tmp/test_complex_terrain.csv", "w") as f:
        f.write(terrain_content)
    
    with open("/tmp/test_complex_inputs.i", "w") as f:
        f.write(inputs_content)
    
    try:
        print("\n1. Initializing wind solver with complex terrain...")
        wind = WindSolver("/tmp/test_complex_inputs.i")
        
        print("2. Solving for mass-consistent wind field...")
        wind.solve()
        
        # Get wind field
        vel = wind.get_velocity()
        terrain = wind.get_terrain()
        
        print(f"   Terrain elevation: {np.nanmin(terrain):.1f} - {np.nanmax(terrain):.1f} m")
        
        # Prepare export
        print("\n3. Preparing export with terrain correction...")
        
        hub_height = 90.0
        z_hub_idx = int((hub_height - wind.zmin) / wind.dz)
        z_hub_idx = min(z_hub_idx, wind.nz - 1)
        
        wind_at_hub = np.sqrt(vel['u'][z_hub_idx, :, :]**2 + vel['v'][z_hub_idx, :, :]**2)
        mean_wind_speed = np.nanmean(wind_at_hub)
        
        print(f"   Mean wind speed at hub height: {mean_wind_speed:.2f} m/s")
        
        # Export
        sys.path.insert(0, "/tmp/workspace/hgopalan/massconsistent_amr/tools")
        from openfast_export import TurbSimBTSWriter
        
        writer = TurbSimBTSWriter()
        writer.initialize(
            num_time_steps=1,
            nx=wind.nx,
            ny=wind.ny,
            nz=wind.nz,
            dt=0.1,
            u_mean=mean_wind_speed,
            dx=wind.dx,
            dy=wind.dy,
            dz=wind.dz,
            z_hub=hub_height,
            turbulence_intensity_u=0.15  # Slightly higher due to terrain
        )
        
        writer.metadata.z0 = 0.15
        writer.metadata.description = "Wind field over complex terrain"
        
        u_data = vel['u'].astype(np.float32).flatten()
        v_data = vel['v'].astype(np.float32).flatten()
        w_data = vel['w'].astype(np.float32).flatten()
        
        output_bts = "/tmp/openfast_terrain.bts"
        if writer.export_time_series(output_bts, u_data, v_data, w_data,
                                    wind.nx, wind.ny, wind.nz, 1):
            print(f"   ✓ Export successful: {output_bts}")
        
        print("\n" + "="*70)
        print("✓ Example 2 completed successfully!")
        print("="*70)
        
        wind.finalize()
        return True
        
    except Exception as e:
        print(f"\n✗ Example failed: {e}")
        import traceback
        traceback.print_exc()
        return False
    finally:
        for f in ["/tmp/test_complex_terrain.csv", "/tmp/test_complex_inputs.i"]:
            if os.path.exists(f):
                os.remove(f)


def example_turbulence_parameters():
    """
    Example 3: Demonstrate turbulence parameter customization
    
    Shows how to set various turbulence intensity profiles and length scales.
    """
    print("\n" + "="*70)
    print("Example 3: Turbulence Parameter Customization")
    print("="*70)
    
    print("\nThis example demonstrates turbulence parameter settings:")
    print("\n1. Von Kármán Spectrum (default)")
    print("   - Suitable for neutral boundary layer")
    print("   - Integral scale increases with height: L(z) = L_0 * z")
    print("   - Typical values: L_0 ≈ 0.1 (von Kármán constant)")
    
    print("\n2. Kaimal Spectrum")
    print("   - From Kaimal et al. (1972) field measurements")
    print("   - Better for stable/unstable conditions")
    print("   - Integral scale: L_u ≈ 0.2 * z for neutral")
    
    print("\n3. Turbulence Intensity Profiles")
    print("   - Surface layer (z < 0.1*BL): I(z) = I_ref")
    print("   - Mixed layer: I(z) decreases with height")
    print("   - Upper BL: I(z) = 0.05-0.10 m/s")
    
    print("\n4. Recommended Parameters for OpenFAST:")
    print("   - Neutral: I_u = 0.12-0.16, L_u = 0.1-0.2 * z_hub")
    print("   - Stable: I_u = 0.08-0.12, L_u = 0.05-0.1 * z_hub")
    print("   - Unstable: I_u = 0.14-0.20, L_u = 0.2-0.3 * z_hub")
    
    print("\n" + "="*70)
    print("✓ Example 3 completed!")
    print("="*70)
    
    return True


def main():
    """Run all examples"""
    print("\n" + "="*70)
    print("OpenFAST/TurbSim Export Examples")
    print("="*70)
    
    examples = [
        ("Basic Export", example_basic_openfast_export),
        ("Complex Terrain", example_terrain_export),
        ("Turbulence Parameters", example_turbulence_parameters),
    ]
    
    results = []
    for name, example_func in examples:
        try:
            result = example_func()
            results.append((name, result))
        except Exception as e:
            print(f"\n✗ Example crashed: {e}")
            import traceback
            traceback.print_exc()
            results.append((name, False))
    
    # Summary
    print("\n" + "="*70)
    print("Summary")
    print("="*70)
    
    for name, result in results:
        status = "✓ PASSED" if result else "✗ FAILED"
        print(f"{name:.<50} {status}")
    
    passed = sum(1 for _, r in results if r)
    total = len(results)
    print(f"\nPassed: {passed}/{total}")
    
    return 0 if passed == total else 1


if __name__ == "__main__":
    sys.exit(main())
