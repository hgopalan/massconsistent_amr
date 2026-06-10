#!/usr/bin/env python3
"""
11_end_to_end_facility.py - End-to-End Facility Workflow Example

Demonstrates complete pipeline for reactive transport analysis at a facility:
1. Solve mass-consistent wind field
2. Run dispersion model (puff/LPDM) from processing stack
3. Extract atmospheric pollutant concentration field
4. Run PHREEQC reactive transport (downwind region)
5. Output transformed chemistry map (pH, precipitation, toxic species)

Includes intermediate caching for fast re-runs with alternative chemistry.

Example: REE processing facility with downwind AMD simulation.

References:
    - Parkhurst & Appelo (2013). PHREEQC (Version 3)
    - Businger et al. (1971). Flux-profile relationships
    - Briggs (1984). Plume rise
"""

import numpy as np
from pathlib import Path
import logging
import sys
import time

# Add src/python directory to path
sys.path.insert(0, str(Path(__file__).parent.parent.parent / "src" / "python"))

from phreeqc_coupling.facility_workflow import (
    FacilityWorkflow,
    FacilityConfiguration,
    end_to_end_facility_analysis
)

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


class MockWindSolver:
    """Mock wind solver for demonstration."""
    
    def __init__(self):
        self.nx, self.ny, self.nz = 50, 50, 20
        self.dx = self.dy = self.dz = 20
        self.u = np.random.randn(50, 50, 20) + 5.0  # Base wind ~5 m/s
    
    def solve(self):
        """Simulate wind solve (delay for demonstration)."""
        time.sleep(0.1)
        logger.info(f"Wind solver solved: {self.nx}×{self.ny}×{self.nz} grid")


class MockDispersionModel:
    """Mock dispersion model for demonstration."""
    
    def solve(self, wind_solver, config):
        """Simulate dispersion solve."""
        time.sleep(0.1)
        logger.info("Dispersion model solved")
        return np.random.exponential(scale=0.1, size=(50, 50, 20))


class MockPHREEQCInterface:
    """Mock PHREEQC interface for demonstration."""
    
    def solve_reactive_transport(self, concentration_field, wind_field, config):
        """Simulate PHREEQC solve."""
        time.sleep(0.1)
        logger.info("PHREEQC reactive transport solved")
        
        # Simulate pH decrease with concentration
        pH = 7.0 - 2.0 * concentration_field / np.max(concentration_field)
        return {
            'pH': pH,
            'dissolved_species': concentration_field * 0.5,
            'precipitation': concentration_field * 0.3
        }


def main():
    """Run end-to-end facility workflow example."""
    
    print("=" * 70)
    print("End-to-End Facility Workflow: REE Processing Facility")
    print("=" * 70)
    
    # Step 1: Define facility configuration
    print("\n[Step 1] Configuring REE processing facility...")
    
    facility_config = FacilityConfiguration(
        name='REE Processing Facility - Downwind AMD Analysis',
        x_facility=500.0,
        y_facility=500.0,
        z_stack=50.0,
        stack_diameter=2.0,
        emission_rate=0.1,  # kg/s
        pollutant_species='H2SO4 (sulfuric acid mist)',
        stack_temperature=600.0  # K
    )
    
    print(f"  ✓ Facility: {facility_config.name}")
    print(f"  ✓ Location: ({facility_config.x_facility}, {facility_config.y_facility}) m")
    print(f"  ✓ Stack height: {facility_config.z_stack} m")
    print(f"  ✓ Emission rate: {facility_config.emission_rate} kg/s")
    print(f"  ✓ Pollutant: {facility_config.pollutant_species}")
    
    # Step 2: Initialize workflow
    print("\n[Step 2] Initializing workflow with caching...")
    
    workflow = FacilityWorkflow(facility_config, cache_dir='./cache_facility')
    
    print(f"  ✓ Workflow initialized")
    print(f"  ✓ Cache directory: ./cache_facility")
    
    # Step 3: Create mock solvers
    print("\n[Step 3] Creating solvers and interfaces...")
    
    wind_solver = MockWindSolver()
    dispersion_model = MockDispersionModel()
    phreeqc_interface = MockPHREEQCInterface()
    
    print(f"  ✓ Wind solver (mock)")
    print(f"  ✓ Dispersion model (mock)")
    print(f"  ✓ PHREEQC interface (mock)")
    
    # Step 4: Run complete workflow
    print("\n[Step 4] Running complete workflow...")
    print(f"  Expected total time: ~20 minutes (wind 10m + dispersion 2-5m + chemistry 5-8m)")
    print(f"  This demo will complete faster with mock solvers\n")
    
    try:
        results = workflow.run_all(
            wind_solver=wind_solver,
            dispersion_model=dispersion_model,
            phreeqc_interface=phreeqc_interface,
            output_dir='./results_facility',
            use_cache=True
        )
        
        # Step 5: Analyze results
        print("\n[Step 5] Analyzing results...")
        
        print(f"\n  Workflow execution summary:")
        print(f"  {'-'*60}")
        
        for step_name, output in results.items():
            status_symbol = "✓" if output.status == "SUCCESS" else "✗"
            print(f"  {status_symbol} {step_name}")
            print(f"      Status: {output.status}")
            print(f"      Time: {output.duration:.2f}s")
            
            if output.cache_file:
                print(f"      Cache: {output.cache_file}")
        
        # Calculate total time
        total_time = sum(o.duration for o in results.values())
        print(f"  {'-'*60}")
        print(f"  Total execution time: {total_time:.2f}s ({total_time/60:.2f} min)")
        
        # Step 6: Chemistry insights
        print("\n[Step 6] Chemistry prediction insights...")
        
        if workflow.dispersion_field and workflow.chemistry_field:
            print(f"\n  Dispersion predictions:")
            print(f"    - Peak concentration: {workflow.dispersion_field['peak_concentration']:.2e} (units)")
            print(f"    - Mean concentration: {workflow.dispersion_field['mean_concentration']:.2e} (units)")
            
            pH_field = workflow.chemistry_field.get('pH')
            if pH_field is not None:
                print(f"\n  Chemistry predictions:")
                print(f"    - pH range: {np.min(pH_field):.2f} - {np.max(pH_field):.2f}")
                print(f"    - Mean pH: {np.mean(pH_field):.2f}")
                
                # Identify acidic zones
                n_acidic = np.sum(pH_field < 6.0)
                pct_acidic = 100 * n_acidic / pH_field.size
                print(f"    - Acidic zones (pH < 6): {pct_acidic:.1f}% of domain")
                
                # Identify critical zones (pH < 5, may trigger AMD)
                n_critical = np.sum(pH_field < 5.0)
                pct_critical = 100 * n_critical / pH_field.size
                print(f"    - Critical zones (pH < 5): {pct_critical:.1f}% of domain")
        
        # Step 7: Downwind impact analysis
        print("\n[Step 7] Downwind impact assessment...")
        
        # Simulate impact at different distances
        distances = [500, 1000, 2000, 5000]  # meters
        
        print(f"\n  Downwind impact at various distances:")
        print(f"  {'Distance (m)':<20} {'Est. pH':<15} {'Risk Assessment':<25}")
        print(f"  {'-'*60}")
        
        for dist in distances:
            # Simple decay model
            pH_at_dist = 7.0 - (2.0 * np.exp(-dist / 1000))
            
            if pH_at_dist < 5.0:
                risk = "CRITICAL (strong AMD)"
            elif pH_at_dist < 6.0:
                risk = "HIGH (moderate AMD)"
            elif pH_at_dist < 6.5:
                risk = "MEDIUM (acidic)"
            else:
                risk = "LOW (neutral/basic)"
            
            print(f"  {dist:<20} {pH_at_dist:<15.2f} {risk:<25}")
        
        # Step 8: Caching efficiency
        print("\n[Step 8] Caching efficiency analysis...")
        
        print(f"\n  Reuse scenario (alternative chemistry, same wind/dispersion):")
        print(f"  ")
        print(f"  First run (step1-step4):")
        
        wind_time = results['step1_wind'].duration
        disp_time = results['step2_dispersion'].duration
        chem_time = results['step4_reactive_transport'].duration
        
        print(f"    - Wind solve: {wind_time:.2f}s")
        print(f"    - Dispersion: {disp_time:.2f}s")
        print(f"    - Chemistry: {chem_time:.2f}s")
        print(f"    - Total: {wind_time + disp_time + chem_time:.2f}s")
        
        print(f"\n  Second run (with cache, chemistry only):")
        print(f"    - Wind solve: ~0.05s (cached)")
        print(f"    - Dispersion: ~0.05s (cached)")
        print(f"    - Chemistry: {chem_time:.2f}s (full recomputation)")
        print(f"    - Total: ~{chem_time + 0.1:.2f}s")
        
        speedup = (wind_time + disp_time + chem_time) / (chem_time + 0.1)
        print(f"  Speedup: {speedup:.1f}× faster for chemistry-only re-runs")
        
        # Step 9: Output files
        print("\n[Step 9] Output files generated...")
        
        results_dir = Path('./results_facility')
        if results_dir.exists():
            output_files = list(results_dir.glob('*.npy')) + list(results_dir.glob('*.json'))
            print(f"\n  Output directory: {results_dir}")
            for f in sorted(output_files)[:5]:
                print(f"    - {f.name}")
            if len(output_files) > 5:
                print(f"    ... and {len(output_files)-5} more files")
        
    except Exception as e:
        logger.error(f"Workflow failed: {e}", exc_info=True)
        print(f"\n✗ Workflow execution failed")
        return 1
    
    # Step 10: Recommendations
    print("\n[Step 10] Recommendations for operational deployment...")
    
    print(f"""
  1. Calibration & Validation:
     - Validate predictions against field observations at facility
     - Calibrate pH adjustment factors for local dust composition
     - Verify dispersion model with tracer studies
  
  2. Continuous Monitoring:
     - Deploy automated wind sensors at facility and downwind points
     - Monitor groundwater pH and AMD indicators downwind
     - Use workflow for real-time risk forecasting
  
  3. Environmental Controls:
     - Install dust suppression system if acidic zones extend beyond property
     - Consider SO₄ neutralization in drainage systems
     - Optimize stack height to reduce ground-level impacts
  
  4. Future Enhancements:
     - Couple with PHREEQC for full reactive transport (vs. simplified model)
     - Add precipitation scavenging (washout) during rainy periods
     - Implement probabilistic ensemble forecasts (wind uncertainty)
     - Extend to multi-phase transport (aerosol settling, wet deposition)
    """)
    
    print("\n" + "=" * 70)
    print("✓ End-to-end facility workflow example complete")
    print("=" * 70)
    
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except Exception as e:
        logger.error(f"Fatal error: {e}", exc_info=True)
        sys.exit(1)
