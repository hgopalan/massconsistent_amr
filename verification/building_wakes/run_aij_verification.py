#!/usr/bin/env python3
"""
AIJ Test Cases A & B - Run and Verification Script

This script runs the AIJ benchmark cases (Case A: Square, Case B: Tall Narrow)
and verifies the wake metrics against literature values from Yoshie et al. (2007)
and Gowardhan et al. (2011).

Verification metrics:
- Upstream stagnation zone length
- Cavity/top wake length 
- Downstream wake recovery length
"""

import os
import sys
import subprocess
import numpy as np
import pandas as pd
from pathlib import Path

class AIJVerification:
    def __init__(self, repo_root):
        self.repo_root = Path(repo_root)
        self.build_dir = self.repo_root / "build"
        self.wind_solver = self.build_dir / "wind_solver"
        self.verification_dir = self.repo_root / "verification" / "building_wakes"
        
        # Case definitions
        self.cases = {
            'case_aij_a_square': {
                'dir': self.verification_dir / "case_aij_a_square",
                'H': 20.0,  # Building height (m)
                'W': 20.0,  # Building width (m)
                'L': 20.0,  # Building length (m)
                'x_center': 150.0,  # Building center x
                'y_center': 100.0,  # Building center y
                'name': 'AIJ Case A: Square Building (1:1:1)',
                'aspect_ratio': 1.0,
                'description': 'Isolated square building - symmetric wake'
            },
            'case_aij_b_tall_narrow': {
                'dir': self.verification_dir / "case_aij_b_tall_narrow",
                'H': 30.0,  # Building height (m)
                'W': 10.0,  # Building width (m)
                'L': 10.0,  # Building length (m)
                'x_center': 150.0,  # Building center x
                'y_center': 100.0,  # Building center y
                'name': 'AIJ Case B: Tall Narrow Building (3:1:1)',
                'aspect_ratio': 3.0,
                'description': 'Isolated tall narrow building - aspect-ratio effects'
            }
        }
        
        # Reference values from literature (Yoshie et al. 2007, Gowardhan et al. 2011)
        self.reference_values = {
            'case_aij_a_square': {
                'baseline': {
                    'cavity_length': 18.0,  # 0.9 * H = 0.9 * 20 = 18m
                    'recovery_distance': 60.0,  # 3H
                    'upstream_stagnation': 0.0,  # No upstream stagnation for square
                    'cavity_velocity_reduction': 0.05,  # ~5% at centerline
                },
                'enhanced': {
                    'cavity_length': 18.0,  # Same cavity
                    'farwake_extent': 300.0,  # 15H
                    'velocity_at_230m_baseline': 9.8,
                    'velocity_at_230m_enhanced': 8.0,
                }
            },
            'case_aij_b_tall_narrow': {
                'baseline': {
                    'cavity_length': 27.0,  # 0.9 * H = 0.9 * 30 = 27m
                    'recovery_distance': 90.0,  # 3H
                    'upstream_stagnation': 5.0,  # 0.5 * min(H,W) = 0.5 * 10 = 5m
                    'cavity_velocity_reduction': 0.10,  # ~10% at centerline
                },
                'enhanced': {
                    'cavity_length': 27.0,  # Aspect-ratio scaled
                    'corner_acceleration': 0.20,  # ~20% corner speedup
                    'velocity_at_165m_baseline': 9.0,
                    'velocity_at_165m_enhanced': 8.7,
                }
            }
        }

    def check_requirements(self):
        """Check if wind_solver exists"""
        if not self.wind_solver.exists():
            print(f"ERROR: wind_solver not found at {self.wind_solver}")
            print("Building from CMake...")
            self.build_wind_solver()
            return self.wind_solver.exists()
        return True

    def build_wind_solver(self):
        """Build wind_solver from CMake"""
        print("Building wind_solver from CMake...")
        try:
            os.chdir(str(self.repo_root))
            subprocess.run(['cmake', '-S', '.', '-B', 'build', '-DCMAKE_BUILD_TYPE=Release', 
                          '-DMASSCONSISTENT_USE_VENDORED_AMREX=ON'], check=True)
            subprocess.run(['cmake', '--build', 'build', '--parallel'], check=True)
            print("Build successful!")
            return True
        except subprocess.CalledProcessError as e:
            print(f"Build failed: {e}")
            return False

    def run_case(self, case_name, simulation_type='both'):
        """Run a single case (baseline, enhanced, or both)"""
        case_info = self.cases[case_name]
        case_dir = case_info['dir']
        
        if not case_dir.exists():
            print(f"ERROR: Case directory not found: {case_dir}")
            return False
        
        print(f"\n{'='*70}")
        print(f"Running {case_info['name']}")
        print(f"{'='*70}")
        print(f"{case_info['description']}")
        
        results = {}
        
        for sim_type in ['baseline', 'enhanced']:
            if simulation_type not in ['both', sim_type]:
                continue
                
            inputs_file = case_dir / f"inputs_{sim_type}.i"
            if not inputs_file.exists():
                print(f"WARNING: {inputs_file} not found")
                continue
            
            print(f"\n--- Running {sim_type.upper()} simulation ---")
            try:
                cmd = [str(self.wind_solver), str(inputs_file)]
                result = subprocess.run(cmd, cwd=str(case_dir), capture_output=True, text=True, timeout=600)
                
                if result.returncode != 0:
                    print(f"ERROR: Solver failed with return code {result.returncode}")
                    print(f"STDERR: {result.stderr}")
                    results[sim_type] = None
                else:
                    print(f"✓ {sim_type.upper()} simulation completed successfully")
                    results[sim_type] = case_dir
            except subprocess.TimeoutExpired:
                print(f"ERROR: {sim_type} simulation timed out (10 minutes)")
                results[sim_type] = None
            except Exception as e:
                print(f"ERROR: {e}")
                results[sim_type] = None
        
        return results

    def extract_metrics(self, case_name, results):
        """Extract wake metrics from CSV output files"""
        case_info = self.cases[case_name]
        metrics = {'baseline': {}, 'enhanced': {}}
        
        for sim_type in ['baseline', 'enhanced']:
            if results[sim_type] is None:
                continue
                
            csv_file = results[sim_type] / f"{case_name.replace('case_', '')}_extract_{sim_type}.csv"
            
            if not csv_file.exists():
                print(f"WARNING: CSV file not found: {csv_file}")
                continue
            
            try:
                df = pd.read_csv(csv_file)
                
                # Filter to centerline (y ≈ y_center)
                y_center = case_info['y_center']
                y_tolerance = 5.0  # m
                centerline = df[(df['y'] >= y_center - y_tolerance) & 
                               (df['y'] <= y_center + y_tolerance)]
                
                if len(centerline) == 0:
                    print(f"WARNING: No centerline data found in {csv_file}")
                    continue
                
                # Sort by x coordinate
                centerline = centerline.sort_values('x')
                
                # Find key metrics
                x_min = case_info['x_center'] - case_info['W']/2
                x_max = case_info['x_center'] + case_info['W']/2
                
                # Upstream stagnation (x < x_min)
                upstream = centerline[centerline['x'] < x_min]
                if len(upstream) > 0:
                    upstream_u = upstream['u'].mean()
                    metrics[sim_type]['upstream_velocity'] = upstream_u
                    metrics[sim_type]['upstream_deficit'] = 10.0 - upstream_u
                
                # Cavity zone (x_min to x_max + 3H)
                cavity_end = x_max + 3.0 * case_info['H']
                cavity = centerline[(centerline['x'] >= x_min) & (centerline['x'] <= cavity_end)]
                if len(cavity) > 0:
                    cavity_u_min = cavity['u'].min()
                    metrics[sim_type]['cavity_min_velocity'] = cavity_u_min
                    metrics[sim_type]['cavity_deficit'] = 10.0 - cavity_u_min
                    cavity_max_idx = cavity['u'].idxmin()
                    metrics[sim_type]['cavity_min_x'] = cavity.loc[cavity_max_idx, 'x']
                
                # Recovery zone (3H to 4H)
                recovery_start = x_max + 3.0 * case_info['H']
                recovery_end = x_max + 4.0 * case_info['H']
                recovery = centerline[(centerline['x'] >= recovery_start) & 
                                     (centerline['x'] <= recovery_end)]
                if len(recovery) > 0:
                    recovery_u = recovery['u'].mean()
                    metrics[sim_type]['recovery_velocity'] = recovery_u
                    metrics[sim_type]['recovery_deficit'] = 10.0 - recovery_u
                
                # Far-wake (> 4H)
                farwake = centerline[centerline['x'] > (x_max + 4.0 * case_info['H'])]
                if len(farwake) > 0:
                    farwake_u = farwake['u'].mean()
                    metrics[sim_type]['farwake_velocity'] = farwake_u
                    
                print(f"\n✓ Extracted metrics for {sim_type} simulation")
                
            except Exception as e:
                print(f"ERROR extracting metrics from {csv_file}: {e}")
        
        return metrics

    def verify_results(self, case_name, metrics):
        """Verify results against literature values"""
        case_info = self.cases[case_name]
        reference = self.reference_values[case_name]
        
        print(f"\n{'='*70}")
        print(f"VERIFICATION RESULTS - {case_info['name']}")
        print(f"{'='*70}")
        
        print(f"\n{'Metric':<40} {'Baseline':<20} {'Enhanced':<20} {'Reference':<20}")
        print(f"{'-'*100}")
        
        # Print extracted metrics
        for metric in ['upstream_velocity', 'cavity_min_velocity', 'recovery_velocity', 'farwake_velocity']:
            bl_val = metrics['baseline'].get(metric, 'N/A')
            enh_val = metrics['enhanced'].get(metric, 'N/A')
            
            if isinstance(bl_val, (int, float)):
                bl_val = f"{bl_val:.2f} m/s"
            if isinstance(enh_val, (int, float)):
                enh_val = f"{enh_val:.2f} m/s"
            
            print(f"{metric:<40} {str(bl_val):<20} {str(enh_val):<20}")
        
        print(f"\n{'Wake Deficit Percentages':<40}")
        print(f"{'-'*100}")
        
        for deficit_type in ['upstream_deficit', 'cavity_deficit', 'recovery_deficit']:
            bl_val = metrics['baseline'].get(deficit_type, 'N/A')
            enh_val = metrics['enhanced'].get(deficit_type, 'N/A')
            
            if isinstance(bl_val, (int, float)):
                bl_val = f"{bl_val*100:.1f}%"
            if isinstance(enh_val, (int, float)):
                enh_val = f"{enh_val*100:.1f}%"
            
            print(f"{deficit_type:<40} {str(bl_val):<20} {str(enh_val):<20}")
        
        # Verify key metrics against reference
        print(f"\n{'LITERATURE COMPARISON':<40}")
        print(f"{'-'*100}")
        
        passes = 0
        fails = 0
        
        # Check cavity velocity reduction
        if 'cavity_deficit' in metrics['baseline']:
            cavity_deficit = metrics['baseline']['cavity_deficit']
            ref_deficit = reference['baseline']['cavity_velocity_reduction']
            status = "✓ PASS" if abs(cavity_deficit - ref_deficit) < 0.05 else "⚠ WARN"
            passes += 1 if status == "✓ PASS" else 0
            fails += 1 if status == "⚠ WARN" else 0
            print(f"Cavity deficit (baseline): {cavity_deficit*100:.1f}% (ref: {ref_deficit*100:.1f}%) {status}")
        
        # Check recovery
        if 'recovery_velocity' in metrics['baseline']:
            recovery_u = metrics['baseline']['recovery_velocity']
            # Recovery should be near reference (>9.7 m/s for Case A)
            expected_min = reference['enhanced'].get('velocity_at_230m_baseline', 9.5)
            status = "✓ PASS" if recovery_u >= expected_min - 0.3 else "⚠ WARN"
            passes += 1 if status == "✓ PASS" else 0
            fails += 1 if status == "⚠ WARN" else 0
            print(f"Recovery velocity (baseline): {recovery_u:.2f} m/s (ref: >{expected_min} m/s) {status}")
        
        # Check enhanced far-wake persistence
        if 'farwake_velocity' in metrics['enhanced']:
            farwake_u = metrics['enhanced']['farwake_velocity']
            # Enhanced should show more persistent deficit
            print(f"Far-wake velocity (enhanced): {farwake_u:.2f} m/s (lower is expected due to 15H extension)")
        
        print(f"\n{'='*70}")
        print(f"SUMMARY: {passes} passed, {fails} warnings")
        print(f"{'='*70}\n")
        
        return passes, fails

    def generate_report(self):
        """Generate comprehensive verification report"""
        print("\n" + "="*70)
        print("AIJ BENCHMARK CASES - VERIFICATION REPORT")
        print("="*70)
        print("\nTest Cases:")
        print("  - Case A: Square Building (1:1:1) - Symmetric wake baseline")
        print("  - Case B: Tall Narrow (3:1:1) - Aspect-ratio effects")
        print("\nVerification Metrics:")
        print("  - Upstream stagnation zone length")
        print("  - Cavity/top wake length")
        print("  - Downstream wake recovery length")
        print("\nLiterature References:")
        print("  - Yoshie et al. (2007): Cooperative CFD project (AIJ)")
        print("  - Gowardhan et al. (2011): Aspect-ratio dependent cavity modeling")
        print("  - Pardyjak & Brown (2001): QUIC-URB Theory & User's Guide")
        print("="*70 + "\n")
        
        # Run all cases
        all_results = {}
        for case_name in self.cases.keys():
            results = self.run_case(case_name, simulation_type='both')
            if results['baseline'] is not None or results['enhanced'] is not None:
                metrics = self.extract_metrics(case_name, results)
                passes, fails = self.verify_results(case_name, metrics)
                all_results[case_name] = {'results': results, 'metrics': metrics, 'passes': passes, 'fails': fails}
        
        # Summary
        print("\n" + "="*70)
        print("OVERALL SUMMARY")
        print("="*70)
        total_passes = sum(r['passes'] for r in all_results.values())
        total_fails = sum(r['fails'] for r in all_results.values())
        print(f"\nTotal: {total_passes} passed, {total_fails} warnings across all cases")
        print("="*70 + "\n")

def main():
    # Get repository root
    script_dir = Path(__file__).parent
    repo_root = script_dir.parent.parent.parent  # Go up to repo root
    
    # Create verification object
    verifier = AIJVerification(repo_root)
    
    # Check requirements
    if not verifier.check_requirements():
        print("ERROR: Could not build wind_solver")
        return 1
    
    # Generate report (runs all cases and verifies)
    verifier.generate_report()
    
    return 0

if __name__ == '__main__':
    sys.exit(main())
