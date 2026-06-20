#!/usr/bin/env python3
# ============================================================================
# parameter_sensitivity.py
# Batch parameter sweep utility for mass-consistent wind solver
#
# Performs systematic parameter variation studies to quantify how key
# parameters affect solver output. Supports:
#   - Single parameter sweeps
#   - Multi-parameter combinations
#   - Comparative statistics (mean, std, min, max)
#   - CSV output and visualization data
#   - Convergence diagnostics
#
# Key Parameters:
#   z0        - Aerodynamic roughness length [m]
#   alpha_h   - Horizontal anisotropy coefficient [dimensionless]
#   alpha_v   - Vertical anisotropy coefficient [dimensionless]
#   z_ref     - Reference height for log-law [m]
#   domain_height - Vertical domain extent [m]
#
# Usage:
#   ./parameter_sensitivity.py --inputs base_inputs.i --param z0 \
#       --range 0.001 0.1 --steps 10 --output sensitivity_z0.csv
#
#   ./parameter_sensitivity.py --inputs base_inputs.i \
#       --multi-param z0 alpha_v --ranges 0.001 0.1 0.5 2.0 \
#       --steps 5 5 --output sensitivity_multi.csv
#
# Physical Constants:
#   κ (von Karman) = 0.41 (standard value)
#   z0 typical range: 0.001 m (smooth water) to 1.0 m (dense forest)
#   alpha_h, alpha_v typical range: 0.5 to 2.0
# ============================================================================

import argparse
import csv
import os
import subprocess
import sys
import numpy as np
import tempfile
import shutil
from pathlib import Path
from collections import defaultdict
import time

# ============================================================================
# Configuration & Constants
# ============================================================================

# Typical parameter ranges for physical validity
PARAMETER_RANGES = {
    'z0': (0.0001, 2.0),           # Roughness [m]
    'alpha_h': (0.1, 5.0),          # Horizontal anisotropy
    'alpha_v': (0.1, 5.0),          # Vertical anisotropy
    'z_ref': (1.0, 50.0),           # Reference height [m]
    'domain_height': (50.0, 500.0), # Domain height [m]
    'U_ref': (1.0, 20.0),           # Reference wind speed [m/s]
    'V_ref': (-20.0, 20.0),         # Reference wind component [m/s]
}

# Parameter descriptions for documentation
PARAMETER_DESCRIPTIONS = {
    'z0': 'Aerodynamic roughness length [m]',
    'alpha_h': 'Horizontal anisotropy coefficient (penalty weight)',
    'alpha_v': 'Vertical anisotropy coefficient (penalty weight)',
    'z_ref': 'Reference height for log-law profile [m]',
    'domain_height': 'Vertical domain extent above terrain [m]',
    'U_ref': 'Reference U-component of wind [m/s]',
    'V_ref': 'Reference V-component of wind [m/s]',
}

# ============================================================================
# Utility Functions
# ============================================================================

def read_input_file(filepath):
    """
    Parse wind_solver inputs.i file into a dictionary.
    
    Format: key = value (one per line, # for comments)
    """
    params = {}
    try:
        with open(filepath, 'r') as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith('#'):
                    continue
                if '=' in line:
                    key, val = line.split('=', 1)
                    key = key.strip()
                    val = val.strip()
                    # Remove trailing comments
                    if '#' in val:
                        val = val.split('#')[0].strip()
                    params[key] = val
    except Exception as e:
        print(f"ERROR: Could not read input file {filepath}: {e}")
        sys.exit(1)
    return params

def write_input_file(params, filepath):
    """
    Write parameter dictionary to inputs.i format.
    """
    try:
        with open(filepath, 'w') as f:
            for key in sorted(params.keys()):
                f.write(f"{key} = {params[key]}\n")
    except Exception as e:
        print(f"ERROR: Could not write input file {filepath}: {e}")
        sys.exit(1)

def run_solver(input_file, work_dir, solver_exe='wind_solver', timeout=300):
    """
    Execute wind_solver with given input file.
    
    Returns: (success, elapsed_time_s)
    """
    try:
        start_time = time.time()
        result = subprocess.run(
            [solver_exe, os.path.basename(input_file)],
            cwd=work_dir,
            timeout=timeout,
            capture_output=True,
            text=True
        )
        elapsed = time.time() - start_time
        
        if result.returncode != 0:
            print(f"  SOLVER FAILED: {result.stderr[-200:]}", file=sys.stderr)
            return False, elapsed
        return True, elapsed
    except subprocess.TimeoutExpired:
        print(f"  SOLVER TIMEOUT after {timeout}s", file=sys.stderr)
        return False, timeout
    except Exception as e:
        print(f"  SOLVER ERROR: {e}", file=sys.stderr)
        return False, 0.0

def extract_convergence_metrics(work_dir, plot_prefix='plt_wind'):
    """
    Extract solver convergence metrics from plotfile.
    
    Returns: dict with max_div, mean_div, etc.
    """
    metrics = {
        'max_divergence': 0.0,
        'mean_divergence': 0.0,
        'converged': True,
    }
    
    # TODO: Parse AMReX plotfile or solver output log for convergence data
    # For now, return placeholder metrics
    
    return metrics

def generate_parameter_sweep(base_value, value_min, value_max, num_steps):
    """
    Generate logarithmically-spaced parameter sweep.
    
    Uses log spacing for parameters that span orders of magnitude (like z0).
    Returns array of num_steps values from value_min to value_max.
    """
    if num_steps < 2:
        return [base_value]
    
    # Use logarithmic spacing for better coverage across wide ranges
    if value_min > 0 and value_max > 0:
        log_min = np.log10(value_min)
        log_max = np.log10(value_max)
        log_values = np.linspace(log_min, log_max, num_steps)
        return 10.0 ** log_values
    else:
        # Linear spacing for parameters that can be negative or zero
        return np.linspace(value_min, value_max, num_steps)

# ============================================================================
# Single Parameter Sweep
# ============================================================================

def run_single_parameter_sweep(
    input_file,
    parameter,
    value_min,
    value_max,
    num_steps,
    output_csv=None,
    solver_exe='wind_solver',
    preserve_dir=False
):
    """
    Perform single-parameter sensitivity sweep.
    
    Systematically varies one parameter while holding others constant,
    and reports output statistics (max divergence, convergence time, etc.).
    """
    print(f"\n{'='*70}")
    print(f"Parameter Sensitivity Sweep: {parameter}")
    print(f"Range: [{value_min}, {value_max}], Steps: {num_steps}")
    print(f"{'='*70}\n")
    
    # Read base parameters
    base_params = read_input_file(input_file)
    original_value = base_params.get(parameter, None)
    
    if original_value is None:
        print(f"ERROR: Parameter '{parameter}' not found in input file")
        sys.exit(1)
    
    # Generate parameter values
    param_values = generate_parameter_sweep(float(original_value), value_min, value_max, num_steps)
    
    # Create temporary workspace
    temp_workspace = tempfile.mkdtemp(prefix='sensitivity_', suffix='_tmp')
    print(f"Using temporary workspace: {temp_workspace}")
    
    # Copy data files
    input_dir = os.path.dirname(os.path.abspath(input_file))
    csv_files = [f for f in os.listdir(input_dir) if f.endswith('.csv')]
    for csv_file in csv_files:
        shutil.copy(os.path.join(input_dir, csv_file), temp_workspace)
    
    results = []
    
    try:
        for idx, param_value in enumerate(param_values):
            print(f"[{idx+1}/{num_steps}] {parameter} = {param_value:.6e}")
            
            # Create modified input file
            test_params = base_params.copy()
            test_params[parameter] = str(param_value)
            test_input = os.path.join(temp_workspace, f'inputs_{idx}.i')
            write_input_file(test_params, test_input)
            
            # Run solver
            success, elapsed = run_solver(test_input, temp_workspace, solver_exe, timeout=300)
            
            # Extract metrics
            metrics = extract_convergence_metrics(temp_workspace)
            
            result = {
                'step': idx,
                'parameter': parameter,
                'value': param_value,
                'success': success,
                'elapsed_s': elapsed,
                'max_div': metrics['max_divergence'],
                'mean_div': metrics['mean_divergence'],
            }
            results.append(result)
            
            status = "✓" if success else "✗"
            print(f"  {status} Elapsed: {elapsed:.2f}s, Max Div: {metrics['max_divergence']:.2e}")
    
    finally:
        # Cleanup
        if not preserve_dir:
            shutil.rmtree(temp_workspace, ignore_errors=True)
            print(f"\nCleaned up workspace")
        else:
            print(f"\nPreserved workspace: {temp_workspace}")
    
    # Write output CSV if requested
    if output_csv:
        write_sweep_results(results, output_csv)
        print(f"\nResults written to: {output_csv}")
    
    # Print summary statistics
    print_sweep_summary(results)
    
    return results

# ============================================================================
# Multi-Parameter Sweep
# ============================================================================

def run_multi_parameter_sweep(
    input_file,
    parameters,
    value_ranges,
    num_steps_list,
    output_csv=None,
    solver_exe='wind_solver',
    preserve_dir=False
):
    """
    Perform multi-parameter sensitivity sweep (factorial design).
    
    Varies multiple parameters together to explore parameter space.
    Total runs = product of steps for each parameter.
    """
    print(f"\n{'='*70}")
    print(f"Multi-Parameter Sensitivity Sweep")
    print(f"Parameters: {parameters}")
    print(f"{'='*70}\n")
    
    # Read base parameters
    base_params = read_input_file(input_file)
    
    # Generate parameter grids
    param_grids = {}
    total_runs = 1
    
    for param, (vmin, vmax), nsteps in zip(parameters, value_ranges, num_steps_list):
        grid = generate_parameter_sweep(float(base_params[param]), vmin, vmax, nsteps)
        param_grids[param] = grid
        total_runs *= len(grid)
        print(f"  {param}: {len(grid)} values from {vmin} to {vmax}")
    
    print(f"\nTotal solver runs: {total_runs}\n")
    
    # Create temporary workspace
    temp_workspace = tempfile.mkdtemp(prefix='sensitivity_', suffix='_tmp')
    print(f"Using temporary workspace: {temp_workspace}")
    
    # Copy data files
    input_dir = os.path.dirname(os.path.abspath(input_file))
    csv_files = [f for f in os.listdir(input_dir) if f.endswith('.csv')]
    for csv_file in csv_files:
        shutil.copy(os.path.join(input_dir, csv_file), temp_workspace)
    
    results = []
    run_idx = 0
    
    try:
        # Nested loops over all parameter combinations
        def iterate_combinations(param_idx, current_params):
            nonlocal run_idx, results
            
            if param_idx >= len(parameters):
                # Base case: all parameters set, run solver
                run_idx += 1
                print(f"[{run_idx}/{total_runs}] Running with: ", end='')
                for p in parameters:
                    print(f"{p}={current_params[p]:.3e} ", end='')
                print()
                
                # Create modified input file
                test_params = base_params.copy()
                test_params.update(current_params)
                test_input = os.path.join(temp_workspace, f'inputs_{run_idx}.i')
                write_input_file(test_params, test_input)
                
                # Run solver
                success, elapsed = run_solver(test_input, temp_workspace, solver_exe, timeout=300)
                
                # Extract metrics
                metrics = extract_convergence_metrics(temp_workspace)
                
                result = {
                    'step': run_idx,
                }
                for p in parameters:
                    result[p] = current_params[p]
                result['success'] = success
                result['elapsed_s'] = elapsed
                result['max_div'] = metrics['max_divergence']
                result['mean_div'] = metrics['mean_divergence']
                
                results.append(result)
                
                status = "✓" if success else "✗"
                print(f"  {status} Elapsed: {elapsed:.2f}s")
                return
            
            # Recursive case: iterate over current parameter
            param = parameters[param_idx]
            for value in param_grids[param]:
                current_params[param] = value
                iterate_combinations(param_idx + 1, current_params)
        
        iterate_combinations(0, {})
    
    finally:
        # Cleanup
        if not preserve_dir:
            shutil.rmtree(temp_workspace, ignore_errors=True)
            print(f"\nCleaned up workspace")
        else:
            print(f"\nPreserved workspace: {temp_workspace}")
    
    # Write output CSV if requested
    if output_csv:
        write_sweep_results(results, output_csv)
        print(f"\nResults written to: {output_csv}")
    
    # Print summary statistics
    print_sweep_summary(results)
    
    return results

# ============================================================================
# Output & Reporting
# ============================================================================

def write_sweep_results(results, output_csv):
    """
    Write sweep results to CSV file.
    """
    if not results:
        print("WARNING: No results to write")
        return
    
    try:
        fieldnames = list(results[0].keys())
        with open(output_csv, 'w', newline='') as f:
            writer = csv.DictWriter(f, fieldnames=fieldnames)
            writer.writeheader()
            writer.writerows(results)
    except Exception as e:
        print(f"ERROR: Could not write output CSV {output_csv}: {e}")
        sys.exit(1)

def print_sweep_summary(results):
    """
    Print summary statistics for sweep results.
    """
    if not results:
        print("No results to summarize")
        return
    
    successful = [r for r in results if r['success']]
    
    if not successful:
        print("\n⚠ WARNING: No successful solver runs")
        return
    
    print(f"\n{'='*70}")
    print("Summary Statistics")
    print(f"{'='*70}")
    print(f"Total runs:       {len(results)}")
    print(f"Successful:       {len(successful)} ({100*len(successful)/len(results):.1f}%)")
    print(f"Failed:           {len(results)-len(successful)}")
    
    elapsed_times = [r['elapsed_s'] for r in successful]
    max_divs = [r['max_div'] for r in successful]
    
    print(f"\nElapsed time:")
    print(f"  Min:  {np.min(elapsed_times):.2f}s")
    print(f"  Mean: {np.mean(elapsed_times):.2f}s")
    print(f"  Max:  {np.max(elapsed_times):.2f}s")
    
    print(f"\nMax divergence:")
    print(f"  Min:  {np.min(max_divs):.2e}")
    print(f"  Mean: {np.mean(max_divs):.2e}")
    print(f"  Max:  {np.max(max_divs):.2e}")
    
    print(f"\n{'='*70}\n")

# ============================================================================
# Command-line Interface
# ============================================================================

def main():
    parser = argparse.ArgumentParser(
        description='Parameter sensitivity sweep utility for mass-consistent wind solver',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
Examples:

  # Single parameter sweep
  %(prog)s --inputs regtest/terrain/gaussian_hill/inputs.i --param z0 \\
           --range 0.001 0.1 --steps 10 --output sensitivity_z0.csv

  # Multi-parameter sweep
  %(prog)s --inputs regtest/terrain/gaussian_hill/inputs.i \\
           --multi-param z0 alpha_v \\
           --ranges 0.001 0.1 0.5 2.0 \\
           --steps 5 5 \\
           --output sensitivity_multi.csv

  # List available parameters
  %(prog)s --list-parameters

Physical Constants:
  κ (von Karman constant) = 0.41
  Typical z0 range: 0.001 m (smooth water) to 1.0 m (forest)
  Typical alpha_h, alpha_v range: 0.5 to 2.0
        '''
    )
    
    parser.add_argument('--inputs', type=str, required=False,
                        help='Path to base inputs.i file (required unless using --list-parameters)')
    parser.add_argument('--solver', type=str, default='wind_solver',
                        help='Path to wind_solver executable (default: wind_solver)')
    parser.add_argument('--param', type=str, default=None,
                        help='Single parameter to sweep')
    parser.add_argument('--range', type=float, nargs=2, metavar=('MIN', 'MAX'),
                        help='Parameter range [MIN, MAX]')
    parser.add_argument('--steps', type=int, default=10,
                        help='Number of sweep steps (default: 10)')
    parser.add_argument('--multi-param', type=str, nargs='+', default=None,
                        help='Multiple parameters to sweep')
    parser.add_argument('--ranges', type=float, nargs='+', metavar='VALUE',
                        help='Min/max values for multi-param sweep (pairs of values)')
    parser.add_argument('--output', type=str, default=None,
                        help='Output CSV file for results')
    parser.add_argument('--list-parameters', action='store_true',
                        help='List available parameters and exit')
    parser.add_argument('--preserve-workspace', action='store_true',
                        help='Preserve temporary workspace for debugging')
    
    args = parser.parse_args()
    
    # Handle list-parameters request
    if args.list_parameters:
        print("\nAvailable Parameters for Sensitivity Analysis:\n")
        print(f"{'Parameter':<15} {'Range':<30} {'Description':<45}")
        print(f"{'-'*90}")
        for param in sorted(PARAMETER_RANGES.keys()):
            vmin, vmax = PARAMETER_RANGES[param]
            desc = PARAMETER_DESCRIPTIONS.get(param, '')
            print(f"{param:<15} [{vmin}, {vmax}]          {desc:<45}")
        print()
        return
    
    # Validate that --inputs is provided for sweep operations
    if not args.inputs:
        print("ERROR: --inputs required for sweep operations")
        parser.print_help()
        sys.exit(1)
    
    # Validate input file
    if not os.path.isfile(args.inputs):
        print(f"ERROR: Input file not found: {args.inputs}")
        sys.exit(1)
    
    # Single parameter sweep
    if args.param:
        if not args.range:
            print("ERROR: --range required for single parameter sweep")
            sys.exit(1)
        
        run_single_parameter_sweep(
            args.inputs,
            args.param,
            args.range[0],
            args.range[1],
            args.steps,
            args.output,
            args.solver,
            args.preserve_workspace
        )
    
    # Multi-parameter sweep
    elif args.multi_param:
        if not args.ranges or len(args.ranges) != 2 * len(args.multi_param):
            print(f"ERROR: --ranges requires {2*len(args.multi_param)} values "
                  f"(min/max for each of {len(args.multi_param)} parameters)")
            sys.exit(1)
        
        # Parse min/max pairs
        value_ranges = []
        for i in range(len(args.multi_param)):
            vmin = args.ranges[2*i]
            vmax = args.ranges[2*i+1]
            value_ranges.append((vmin, vmax))
        
        run_multi_parameter_sweep(
            args.inputs,
            args.multi_param,
            value_ranges,
            [args.steps] * len(args.multi_param),
            args.output,
            args.solver,
            args.preserve_workspace
        )
    
    else:
        print("ERROR: Specify either --param or --multi-param")
        parser.print_help()
        sys.exit(1)

if __name__ == '__main__':
    main()
