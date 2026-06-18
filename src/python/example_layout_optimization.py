#!/usr/bin/env python3
"""
example_layout_optimization.py - Complete wind farm layout optimization example

This example demonstrates the full workflow for optimizing wind farm turbine layouts:
1. Load or create a solved wind field
2. Cache the wind field for fast evaluation
3. Define candidate turbine layouts
4. Run optimization to find best positions
5. Visualize results and export to standard formats

To run this example:
    cd /path/to/massconsistent_amr
    python3 src/python/example_layout_optimization.py

Requirements:
    - numpy, scipy, matplotlib, h5py (standard scientific stack)
    - pyWindSolver (C++ bindings from solver)
"""

import numpy as np
import sys
import os
from pathlib import Path

# Add src/python to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__)))


def example_1_synthetic_wind_field():
    """
    Example 1: Create a synthetic wind field and run layout optimization.
    
    This example uses synthetic wind data to demonstrate the optimization
    framework without requiring a solver executable or inputs file.
    """
    print("\n" + "="*70)
    print("Example 1: Layout Optimization with Synthetic Wind Field")
    print("="*70)
    
    try:
        from wind_field_cache import WindFieldCache
        from layout_optimizer import WindFarmLayoutOptimizer
    except ImportError as e:
        print(f"Error importing modules: {e}")
        print("Ensure wind_field_cache.py and layout_optimizer.py exist")
        return False
    
    # Create synthetic wind field
    print("\nCreating synthetic wind field...")
    
    nx, ny, nz = 50, 50, 20
    dx, dy, dz = 50.0, 50.0, 10.0
    
    # Create 3D velocity field (constant wind from west at 10 m/s)
    u_field = np.ones((nz, ny, nx)) * 10.0
    v_field = np.zeros((nz, ny, nx))
    w_field = np.zeros((nz, ny, nx))
    
    # Create flat terrain
    terrain = np.zeros((ny, nx))
    
    # Manually create cache (instead of from solver)
    cache = WindFieldCache()
    cache.u_field = u_field
    cache.v_field = v_field
    cache.w_field = w_field
    cache.terrain = terrain
    cache.nx = nx
    cache.ny = ny
    cache.nz = nz
    cache.dx = dx
    cache.dy = dy
    cache.dz = dz
    cache.xmin = 0.0
    cache.ymin = 0.0
    cache.zmin = 0.0
    cache.grid_x = np.linspace(cache.xmin, cache.xmin + (nx-1)*dx, nx)
    cache.grid_y = np.linspace(cache.ymin, cache.ymin + (ny-1)*dy, ny)
    cache.grid_z = np.linspace(cache.zmin, cache.zmin + (nz-1)*dz, nz)
    cache.metadata = {'example': 'synthetic'}
    
    print(f"  Created synthetic {nx}×{ny}×{nz} grid")
    bounds = cache.get_domain_bounds()
    print(f"  Domain: 0-{bounds['xmax']:.0f} m × 0-{bounds['ymax']:.0f} m")
    
    # Define initial layout (random configuration)
    np.random.seed(42)
    num_turbines = 10
    initial_layout = []
    
    for i in range(num_turbines):
        # Distribute turbines randomly in domain
        x = np.random.uniform(200, 2000)
        y = np.random.uniform(200, 2000)
        initial_layout.append({
            'id': i,
            'x': x,
            'y': y,
            'z': 0.0
        })
    
    print(f"\nInitial layout: {num_turbines} turbines")
    for turb in initial_layout[:3]:
        print(f"  Turbine {turb['id']}: ({turb['x']:.0f}, {turb['y']:.0f})")
    print(f"  ... ({len(initial_layout)} total)")
    
    # Create optimizer
    print("\nInitializing optimizer...")
    optimizer = WindFarmLayoutOptimizer(
        wind_cache=cache,
        turbines=initial_layout,
        hub_height=90.0,
        rotor_diameter=100.0,
        turbulence_intensity=0.10,
        min_spacing=400.0
    )
    
    # Evaluate baseline layout
    baseline_aep, baseline_speeds = optimizer.evaluate_layout(initial_layout)
    print(f"Baseline AEP: {baseline_aep:.2f} MWh/year")
    
    # Run optimization
    print("\nRunning optimization (differential evolution)...")
    result = optimizer.optimize(
        method='differential_evolution',
        max_iterations=50,
        population_size=30,
        verbose=True
    )
    
    # Export results
    output_dir = Path("/tmp/layout_optimization_example")
    output_dir.mkdir(exist_ok=True)
    
    layout_file = output_dir / "optimized_layout.csv"
    result_file = output_dir / "optimization_result.json"
    
    optimizer.export_layout_csv(result.layout, str(layout_file))
    optimizer.export_result_json(result, str(result_file))
    
    print(f"\nResults saved to {output_dir}/")
    print(f"\nOptimization Summary:")
    print(f"  Baseline AEP: {baseline_aep:.2f} MWh/year")
    print(f"  Optimized AEP: {result.aep_mwh:.2f} MWh/year")
    print(f"  Improvement: +{result.aep_improvement:.2f}%")
    print(f"  Total evaluations: {result.num_iterations}")
    print(f"  Computation time: {result.computation_time_s:.1f}s")
    
    return True


def example_2_with_wind_solver():
    """
    Example 2: Optimization using actual wind solver results.
    
    This example requires:
    - Built solver with Python bindings
    - Valid inputs.i file for wind solver
    
    Demonstrates the full pipeline:
    - Solve wind field with C++ solver
    - Cache for fast evaluation
    - Optimize layout
    """
    print("\n" + "="*70)
    print("Example 2: Layout Optimization with Wind Solver")
    print("="*70)
    
    try:
        from wind_solver import WindSolver
        from wind_field_cache import WindFieldCache
        from layout_optimizer import WindFarmLayoutOptimizer
    except ImportError as e:
        print(f"Could not import required modules: {e}")
        print("This example requires built pyWindSolver bindings")
        return False
    
    # Assume inputs.i exists in current directory
    inputs_file = "inputs.i"
    
    if not os.path.exists(inputs_file):
        print(f"Input file not found: {inputs_file}")
        print("Skipping Example 2 (requires solver inputs)")
        return False
    
    try:
        # 1. Solve wind field
        print(f"\nSolving wind field from {inputs_file}...")
        wind = WindSolver(inputs_file)
        wind.solve()
        
        # 2. Cache the solution
        print("\nCaching wind field...")
        cache = WindFieldCache.from_solver(wind)
        cache.save("/tmp/wind_field_cache.h5")
        
        # 3. Define initial layout
        print("\nDefining turbine layout...")
        initial_layout = [
            {'id': 0, 'x': 500, 'y': 500, 'z': 0},
            {'id': 1, 'x': 1500, 'y': 500, 'z': 0},
            {'id': 2, 'x': 2500, 'y': 500, 'z': 0},
        ]
        
        # 4. Create and run optimizer
        print("\nOptimizing layout...")
        optimizer = WindFarmLayoutOptimizer(
            wind_cache=cache,
            turbines=initial_layout,
            hub_height=90.0,
            rotor_diameter=100.0
        )
        
        result = optimizer.optimize(
            method='differential_evolution',
            max_iterations=100,
            population_size=50,
            verbose=True
        )
        
        # 5. Export results
        optimizer.export_layout_csv(result.layout, "/tmp/optimized_layout.csv")
        print("\n✓ Optimization completed successfully!")
        
        # Clean up
        wind.finalize()
        
    except Exception as e:
        print(f"Error in Example 2: {e}")
        import traceback
        traceback.print_exc()
        return False
    
    return True


def example_3_visualization():
    """
    Example 3: Visualize optimization results with matplotlib.
    
    This example shows how to plot:
    - Initial vs. optimized layouts
    - Convergence history
    - Wind field heatmaps
    """
    print("\n" + "="*70)
    print("Example 3: Visualization of Results")
    print("="*70)
    
    try:
        import matplotlib.pyplot as plt
        from matplotlib.patches import Circle
    except ImportError:
        print("matplotlib not available, skipping visualization")
        return False
    
    # Create synthetic data for visualization
    np.random.seed(42)
    num_turbines = 10
    
    # Initial layout
    initial_layout = []
    for i in range(num_turbines):
        initial_layout.append({
            'id': i,
            'x': np.random.uniform(200, 2000),
            'y': np.random.uniform(200, 2000)
        })
    
    # Simulated optimized layout
    optimized_layout = []
    for turb in initial_layout:
        optimized_layout.append({
            'id': turb['id'],
            'x': turb['x'] + np.random.uniform(-100, 100),
            'y': turb['y'] + np.random.uniform(-100, 100)
        })
    
    # Create figure
    fig, axes = plt.subplots(1, 2, figsize=(14, 6))
    
    # Plot initial layout
    ax = axes[0]
    for turb in initial_layout:
        circle = Circle((turb['x'], turb['y']), 50, color='red', alpha=0.6)
        ax.add_patch(circle)
        ax.text(turb['x'], turb['y'], str(turb['id']), ha='center', va='center', fontsize=8)
    
    ax.set_xlim(0, 2500)
    ax.set_ylim(0, 2500)
    ax.set_aspect('equal')
    ax.set_xlabel('X (m)')
    ax.set_ylabel('Y (m)')
    ax.set_title('Initial Layout')
    ax.grid(True, alpha=0.3)
    
    # Plot optimized layout
    ax = axes[1]
    for turb in optimized_layout:
        circle = Circle((turb['x'], turb['y']), 50, color='green', alpha=0.6)
        ax.add_patch(circle)
        ax.text(turb['x'], turb['y'], str(turb['id']), ha='center', va='center', fontsize=8)
    
    ax.set_xlim(0, 2500)
    ax.set_ylim(0, 2500)
    ax.set_aspect('equal')
    ax.set_xlabel('X (m)')
    ax.set_ylabel('Y (m)')
    ax.set_title('Optimized Layout')
    ax.grid(True, alpha=0.3)
    
    plt.tight_layout()
    
    # Save figure
    output_file = "/tmp/layout_comparison.png"
    plt.savefig(output_file, dpi=150, bbox_inches='tight')
    print(f"✓ Visualization saved to {output_file}")
    
    return True


if __name__ == "__main__":
    print("\n" + "="*70)
    print("Wind Farm Layout Optimization Examples")
    print("="*70)
    
    # Run examples
    success = True
    
    # Example 1: Synthetic wind field (always works)
    if not example_1_synthetic_wind_field():
        success = False
    
    # Example 2: With real wind solver (optional)
    example_2_with_wind_solver()
    
    # Example 3: Visualization
    if not example_3_visualization():
        print("(Skipped - matplotlib not available)")
    
    print("\n" + "="*70)
    if success:
        print("✓ Examples completed successfully!")
    else:
        print("⚠ Some examples had issues - check output above")
    print("="*70)
