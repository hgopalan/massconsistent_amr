#!/usr/bin/env python3
import sys
import subprocess
import re
from pathlib import Path

def run_solver(extra_args=[]):
    exe = Path(__file__).parent.parent.parent / "build" / "wind_solver"
    if not exe.exists():
        exe = Path(__file__).parent.parent.parent / "build" / "Release" / "wind_solver"
    if not exe.exists():
        exe = Path(__file__).parent.parent.parent / "build" / "Debug" / "wind_solver"
    if not exe.exists():
        # Fallback to searching
        matches = list(Path(__file__).parent.parent.parent.glob("**/wind_solver"))
        if not matches:
            matches = list(Path(__file__).parent.parent.parent.glob("**/wind_solver.exe"))
        if matches:
            exe = matches[0]
        else:
            raise FileNotFoundError("Could not find wind_solver executable")
            
    inputs = Path(__file__).parent / "inputs.i"
    cmd = [str(exe), str(inputs)] + extra_args
    print(f"Running command: {' '.join(cmd)}")
    result = subprocess.run(cmd, capture_output=True, text=True, check=True, cwd=str(Path(__file__).parent))
    return result.stdout

def test_carson():
    print("Testing Carson's Model...")
    stdout = run_solver(["thermodynamic_lid_model=carson"])
    print(stdout)
    
    # Parse calculated values
    # Match: wind_solver: thermodynamic lid model 'carson' calculated z_i(t) = <val> m at t = <time> s
    pattern = r"wind_solver: thermodynamic lid model 'carson' calculated z_i\(t\) = ([\d\.]+) m at t = ([\d\.]+) s"
    matches = re.findall(pattern, stdout)
    
    assert len(matches) == 3, f"Expected 3 calculated z_i outputs, found {len(matches)}"
    
    times_expected = [0.0, 3600.0, 7200.0]
    # Calculated earlier:
    # t=0: 100.0 m
    # t=3600: ~510.63 m
    # t=7200: ~743.83 m
    zi_expected = [100.0, 510.63, 743.83]
    
    for i, (zi_str, t_str) in enumerate(matches):
        zi_val = float(zi_str)
        t_val = float(t_str)
        print(f"Match {i}: t = {t_val} s, z_i = {zi_val} m")
        assert abs(t_val - times_expected[i]) < 1e-2, f"Time mismatch at index {i}: expected {times_expected[i]}, got {t_val}"
        assert abs(zi_val - zi_expected[i]) < 0.5, f"z_i mismatch at index {i}: expected {zi_expected[i]}, got {zi_val}"
        
    print("✓ Carson's Model Test Passed!")

def test_maul():
    print("Testing Maul's Model...")
    stdout = run_solver(["thermodynamic_lid_model=maul"])
    print(stdout)
    
    # Match: wind_solver: thermodynamic lid model 'maul' calculated z_i(t) = <val> m at t = <time> s
    pattern = r"wind_solver: thermodynamic lid model 'maul' calculated z_i\(t\) = ([\d\.]+) m at t = ([\d\.]+) s"
    matches = re.findall(pattern, stdout)
    
    assert len(matches) == 3, f"Expected 3 calculated z_i outputs, found {len(matches)}"
    
    # Let's verify that z_i grows over time:
    # zi(0) = 100.0
    # zi(3600) > 100.0
    # zi(7200) > zi(3600)
    zi_vals = [float(match[0]) for match in matches]
    t_vals = [float(match[1]) for match in matches]
    
    print(f"Maul's Model heights: t=0: {zi_vals[0]}, t=3600: {zi_vals[1]}, t=7200: {zi_vals[2]}")
    assert abs(zi_vals[0] - 100.0) < 1e-2, f"Initial z_i should be 100, got {zi_vals[0]}"
    assert zi_vals[1] > 100.0, "Convective boundary layer should grow"
    assert zi_vals[2] > zi_vals[1], "Convective boundary layer should continue to grow"
    
    print("✓ Maul's Model Test Passed!")

if __name__ == "__main__":
    try:
        test_carson()
        test_maul()
        print("\nAll Thermodynamic Lid tests PASSED successfully!")
        sys.exit(0)
    except AssertionError as e:
        print(f"\nAssertion Error: {e}")
        sys.exit(1)
    except Exception as e:
        print(f"\nUnexpected Error: {e}")
        sys.exit(1)
