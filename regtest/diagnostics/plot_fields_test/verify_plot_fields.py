#!/usr/bin/env python3
import sys
import os
import subprocess

# For plot_fields = "velocity,terrain"
# Expected fields: u, v, w, vel_magnitude, terrain_z
# Note: These should appear in sorted order in the plotfile
EXPECTED_FIELDS = ["terrain_z", "u", "v", "vel_magnitude", "w"]
CMAKE_BUILD_CONFIGS = ["Debug", "Release", "RelWithDebInfo", "MinSizeRel"]
SOLVER_EXE_NAME = "wind_solver"

def run_solver(inputs_file, work_dir):
    """Run the wind solver to generate plotfile"""
    # Get the wind_solver executable path from environment or build directory
    solver_exe = os.environ.get('MASSCONSISTENT_EXE')
    if not solver_exe:
        # Try to find it in the build directory
        build_dir = os.path.dirname(os.path.dirname(os.path.dirname(work_dir)))
        # Check standard, configuration-specific, and fallback locations.
        is_windows = sys.platform == 'win32'
        exe_suffix = ".exe" if is_windows else ""
        candidates = [os.path.join(build_dir, f"{SOLVER_EXE_NAME}{exe_suffix}")]
        # Check common multi-configuration build subdirectories
        for config in CMAKE_BUILD_CONFIGS:
            candidates.append(os.path.join(build_dir, config, f"{SOLVER_EXE_NAME}{exe_suffix}"))

        for candidate in candidates:
            if os.path.isfile(candidate):
                solver_exe = candidate
                break
    
    if not solver_exe or not os.path.isfile(solver_exe):
        if solver_exe:
            print(f"Error: wind_solver executable not found at {solver_exe}")
        else:
            print("Error: wind_solver executable not found")
        return False
    
    print(f"Running solver: {solver_exe}")
    print(f"Input file: {inputs_file}")
    print(f"Working directory: {work_dir}")
    
    try:
        result = subprocess.run(
            [solver_exe, inputs_file],
            cwd=work_dir,
            capture_output=True,
            text=True,
            timeout=300
        )
        
        if result.returncode != 0:
            print(f"Solver failed with return code {result.returncode}")
            print(f"STDOUT:\n{result.stdout}")
            print(f"STDERR:\n{result.stderr}")
            return False
        
        print("Solver completed successfully")
        print(f"STDOUT:\n{result.stdout}")
        return True
    except subprocess.TimeoutExpired:
        print("Solver timed out after 300 seconds")
        return False
    except Exception as e:
        print(f"Error running solver: {e}")
        return False

def validate_plot_fields(plotfile_dir):
    """Validate that plot_fields filtering was applied correctly"""
    errors = []
    print(f"Plotfile directory: {plotfile_dir}")
    if not os.path.isdir(plotfile_dir):
        errors.append(f"Plotfile directory not found: {plotfile_dir}")
        return False, errors
    header_file = os.path.join(plotfile_dir, "Header")
    if not os.path.isfile(header_file):
        errors.append(f"Header file not found: {header_file}")
        return False, errors
    
    with open(header_file, 'r') as f:
        header_content = f.read()

    lines = header_content.split('\n')
    idx = 0
    # Skip first line (version)
    idx += 1
    # Find number of components
    while idx < len(lines) and lines[idx].strip() == '':
        idx += 1
    
    ncomp = int(lines[idx].strip())
    print(f"Found {ncomp} components in plotfile")
    
    if ncomp != len(EXPECTED_FIELDS):
        errors.append(f"Expected {len(EXPECTED_FIELDS)} components, but found {ncomp}")
        print(f"Expected: {EXPECTED_FIELDS}")
        return False, errors
        
    actual_fields = []
    idx += 1
    for i in range(ncomp):
        if idx >= len(lines):
            break
        actual_fields.append(lines[idx].strip())
        idx += 1

    print(f"Actual fields: {actual_fields}")
    print(f"Expected fields: {EXPECTED_FIELDS}")

    # Check that all expected fields are present (order may differ due to set sorting)
    actual_set = set(actual_fields)
    expected_set = set(EXPECTED_FIELDS)
    
    if actual_set != expected_set:
        missing = expected_set - actual_set
        extra = actual_set - expected_set
        if missing:
            errors.append(f"Missing fields: {missing}")
        if extra:
            errors.append(f"Extra fields: {extra}")
        return False, errors

    # Check level_0 folder data files (just to be sure)
    level0_dir = os.path.join(plotfile_dir, "Level_0")
    if not os.path.isdir(level0_dir):
        errors.append(f"Level_0 directory not found in plotfile: {level0_dir}")
        return False, errors

    data_files = [f for f in os.listdir(level0_dir) if f.startswith("Cell_D")]
    if not data_files:
        errors.append("No Cell_D* data files found in Level_0 folder")
        return False, errors

    print("plot_fields validation PASSED!")
    return True, errors

def main():
    if len(sys.argv) < 3:
        print("Usage: python3 verify_plot_fields.py <inputs.i> <work_dir>")
        sys.exit(1)
    
    inputs_file = sys.argv[1]
    work_dir = sys.argv[2]
    plotfile_dir = os.path.join(work_dir, "plt_gaussian_hill00000")
    
    # First, run the solver to generate the plotfile
    if not run_solver(inputs_file, work_dir):
        print("Error: Failed to run solver")
        sys.exit(1)
    
    # Then validate the plot_fields filtering
    success, errors = validate_plot_fields(plotfile_dir)
    if errors:
        for error in errors:
            print(f"Error: {error}")
        sys.exit(1)
    sys.exit(0)

if __name__ == '__main__':
    main()
