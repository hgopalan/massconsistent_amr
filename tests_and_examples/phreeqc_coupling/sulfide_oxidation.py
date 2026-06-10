#!/usr/bin/env python3
"""
sulfide_oxidation.py - Wind-Dependent Sulfide Oxidation Rate Computation

Demonstrates how wind speed modulates sulfide mineral oxidation rates and
resulting acid mine drainage chemistry. Integrates kinetic rate laws with
oxygen delivery controlled by turbulent transport.

This example:
  1. Solves the mass-consistent wind field
  2. Loads sulfide mineral deposit coordinates
  3. Computes wind-driven oxygen delivery factors
  4. Calculates temperature-dependent oxidation kinetics
  5. Predicts acid generation rates
  6. Exports spatially-resolved oxidation rate field
  7. Couples with PHREEQC for reactive transport simulation

References:
    - Nicholson et al. (1990). Pyrite oxidation in carbonate-buffered systems.
    - Sherwood (1954). Mass transfer between phases.
    - Molins & Mayer (2007). Reactive transport modeling of biogeochemical
      processes.
"""

import sys
from pathlib import Path
import numpy as np

# Add src/python directory to path
sys.path.insert(0, str(Path(__file__).parent.parent.parent / "src" / "python"))

from wind_solver import WindSolver
from phreeqc_coupling.sulfide_oxidation import (
    compute_sulfide_oxidation_rates,
    SulfideOxidationComputer,
    SulfideMineralType
)


def create_sample_sulfide_locations_csv(output_file: str) -> str:
    """Create sample sulfide deposit locations CSV.
    
    Parameters:
        output_file (str): Output CSV file path
    
    Returns:
        str: File path
    """
    csv_content = """id,x,y,z,mineral_type,mass_fraction,specific_surface_area,description
sul001,5000,5000,100,PYRITE,0.08,150.0,Primary pyrite ore body
sul002,5050,5050,95,PYRITE,0.05,120.0,Secondary pyrite vein
sul003,5100,4950,105,CHALCOPYRITE,0.02,100.0,Chalcopyrite mineralization
sul004,4950,5050,110,PYRITE,0.06,130.0,Weathered pyrite zone
sul005,5150,5000,90,SPHALERITE,0.03,110.0,Sphalerite-pyrite association
"""
    
    output_path = Path(output_file)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    
    with open(output_path, 'w') as f:
        f.write(csv_content)
    
    return str(output_path)


def create_sample_inputs(output_file: str, terrain_file: str) -> str:
    """Create sample massconsistent_amr input file."""
    input_content = f"""# Wind Solver Input File
amr.n_cell_x = 30
amr.n_cell_y = 30
amr.n_cell_z = 15

domain.lo = 0 0 0
domain.hi = 10000 10000 1500

init_mode = uniform
U_ref = 8.0
V_ref = 0.0
z_ref = 10.0
z0 = 0.1

terrain_file = {terrain_file}

poisson_solver.max_iter = 100
poisson_solver.mg_verbose = 0
"""
    with open(output_file, 'w') as f:
        f.write(input_content)
    return output_file


def create_sample_terrain_csv(output_file: str) -> str:
    """Create a flat terrain CSV file."""
    terrain_content = """# Flat terrain
0.0   0.0   0.0
10000.0  0.0   0.0
10000.0 10000.0 0.0
0.0   10000.0 0.0
"""
    with open(output_file, 'w') as f:
        f.write(terrain_content)
    return output_file


class MockWindSolver:
    """Mock wind solver for fallback execution."""
    def __init__(self):
        self.nx, self.ny, self.nz = 30, 30, 15
        self.dx = self.dy = self.dz = 333.33
        self.xmin = self.ymin = self.zmin = 0.0
        self.xmax = self.ymax = self.zmax = 10000.0
        self.zs_min = 0.0
        self.zs_max = 200.0
        
    def get_velocity(self):
        return {
            'u': np.ones((self.nz, self.ny, self.nx)) * 5.0,
            'v': np.zeros((self.nz, self.ny, self.nx)),
            'w': np.zeros((self.nz, self.ny, self.nx))
        }
        
    def get_terrain(self):
        return np.zeros((self.ny, self.nx))
        
    def solve(self):
        pass
        
    def finalize(self):
        pass


def main():
    """Run sulfide oxidation rate computation example."""
    
    print("=" * 70)
    print("Sulfide Oxidation Rate Computation Example")
    print("=" * 70)
    print("\nDemonstrates wind-dependent oxidation kinetics and acid generation")
    print("prediction for acid mine drainage assessment.")
    
    # Setup
    inputs_file = "inputs_amd.i"
    terrain_file = "terrain_amd.csv"
    sulfide_csv = "sulfide_locations_example.csv"
    output_dir = "oxidation_rates_output"
    temperature = 288.15  # 15°C
    
    create_sample_terrain_csv(terrain_file)
    create_sample_inputs(inputs_file, terrain_file)
    
    try:
        # Step 1: Solve wind field
        print("\n" + "-" * 70)
        print("STEP 1: Solving Mass-Consistent Wind Field")
        print("-" * 70)
        
        try:
            wind = WindSolver(inputs_file)
            wind.solve()
            print(f"✓ Wind field solved successfully")
            print(f"  Domain: {wind.nx} × {wind.ny} × {wind.nz} cells")
        except Exception as e:
            print(f"✗ Wind solver initialization/execution failed: {e}")
            print("  Falling back to MockWindSolver for demonstration...")
            wind = MockWindSolver()
        
        # Step 2: Create sample sulfide locations
        print("\n" + "-" * 70)
        print("STEP 2: Loading Sulfide Mineral Deposit Locations")
        print("-" * 70)
        
        sulfide_csv_path = create_sample_sulfide_locations_csv(sulfide_csv)
        print(f"✓ Created sample sulfide locations file: {sulfide_csv}")
        
        # Step 3: Initialize oxidation computer
        print("\n" + "-" * 70)
        print("STEP 3: Initializing Sulfide Oxidation Computer")
        print("-" * 70)
        
        computer = SulfideOxidationComputer(wind, verbose=True)
        computer.load_sulfide_locations(sulfide_csv)
        
        # Step 4: Compute oxidation rates
        print("\n" + "-" * 70)
        print("STEP 4: Computing Wind-Dependent Oxidation Kinetics")
        print("-" * 70)
        
        print(f"\nSimulation parameters:")
        print(f"  Temperature: {temperature - 273.15:.1f}°C ({temperature:.1f} K)")
        print(f"  Reference O₂ concentration: 270 µmol/m³ (atmospheric saturation)")
        
        rates = computer.compute_sulfide_oxidation_rates(
            temperature=temperature,
            output_dir=output_dir
        )
        
        # Step 5: Display oxidation rate results
        print("\n" + "-" * 70)
        print("STEP 5: Oxidation Rate Results")
        print("-" * 70)
        
        print(f"\nTotal sites: {len(rates)}")
        
        print("\nOxidation kinetics summary:")
        print("-" * 70)
        print(f"{'Site':<10} {'Ox. Rate':<18} {'O₂ Deliv.':<12} {'Wind':<8} {'H⁺ Rate':<18}")
        print(f"{'ID':<10} {'[mol/(m³·s)]':<18} {'Factor':<12} {'[m/s]':<8} {'[mol/(m³·s)]':<18}")
        print("-" * 70)
        
        for rate in rates:
            print(f"{rate.site_id:<10} {rate.oxidation_rate:>16.2e}  "
                  f"{rate.O2_delivery_factor:>10.3f}  "
                  f"{rate.wind_speed:>6.2f}  "
                  f"{rate.acid_generation_rate:>16.2e}")
        
        # Step 6: Statistics
        print("\n" + "-" * 70)
        print("STEP 6: Oxidation Rate Statistics")
        print("-" * 70)
        
        ox_rates = [r.oxidation_rate for r in rates]
        acid_rates = [r.acid_generation_rate for r in rates]
        O2_factors = [r.O2_delivery_factor for r in rates]
        wind_speeds = [r.wind_speed for r in rates]
        
        print(f"\nOxidation Rate [mol/(m³·s)]:")
        print(f"  Mean:   {np.mean(ox_rates):10.2e}")
        print(f"  Median: {np.median(ox_rates):10.2e}")
        print(f"  Min:    {np.min(ox_rates):10.2e}")
        print(f"  Max:    {np.max(ox_rates):10.2e}")
        print(f"  Range:  {np.max(ox_rates) - np.min(ox_rates):10.2e}")
        
        print(f"\nAcid Generation Rate [mol H⁺/(m³·s)]:")
        print(f"  Mean:   {np.mean(acid_rates):10.2e}")
        print(f"  Max:    {np.max(acid_rates):10.2e}")
        print(f"  Total:  {np.sum(acid_rates):10.2e} [integrated over domain]")
        
        print(f"\nOxygen Delivery Factor (wind enhancement):")
        print(f"  Mean:   {np.mean(O2_factors):6.3f}")
        print(f"  Range:  {np.min(O2_factors):.3f} - {np.max(O2_factors):.3f}")
        
        print(f"\nWind Speed at Sulfide Sites [m/s]:")
        print(f"  Mean:   {np.mean(wind_speeds):6.2f}")
        print(f"  Min:    {np.min(wind_speeds):6.2f}")
        print(f"  Max:    {np.max(wind_speeds):6.2f}")
        
        # Step 7: Wind-oxidation correlation
        print("\n" + "-" * 70)
        print("STEP 7: Wind-Oxidation Correlation Analysis")
        print("-" * 70)
        
        if len(wind_speeds) > 1:
            correlation = np.corrcoef(wind_speeds, ox_rates)[0, 1]
            print(f"\nCorrelation between wind speed and oxidation rate: {correlation:.3f}")
            
            if correlation > 0.5:
                print("  ✓ Strong positive correlation: Wind speed significantly enhances oxidation")
            elif correlation > 0.3:
                print("  • Moderate positive correlation: Wind speed moderately enhances oxidation")
            else:
                print("  ○ Weak correlation: Other factors may dominate oxidation")
        
        # Step 8: Sensitivity analysis
        print("\n" + "-" * 70)
        print("STEP 8: Sensitivity Analysis - Wind Effect on Oxidation")
        print("-" * 70)
        
        print(f"\nOxygen delivery enhancement at different wind speeds:")
        for u in [2.0, 5.0, 10.0, 15.0]:
            factor = computer.wind_to_oxygen_delivery(u)
            print(f"  u = {u:2.1f} m/s → O₂ delivery factor = {factor:.3f}")
        
        # Step 9: Hotspot identification
        print("\n" + "-" * 70)
        print("STEP 9: Oxidation Hotspot Identification")
        print("-" * 70)
        
        mean_rate = np.mean(ox_rates)
        high_oxidation = [r for r in rates if r.oxidation_rate > 1.5 * mean_rate]
        
        print(f"\nMean oxidation rate: {mean_rate:.2e} mol/(m³·s)")
        
        if high_oxidation:
            print(f"\n⚠️  {len(high_oxidation)} high-oxidation site(s) identified (>1.5× mean):\n")
            for rate in high_oxidation:
                print(f"  • {rate.site_id}")
                print(f"    Oxidation rate: {rate.oxidation_rate:.2e} mol/(m³·s)")
                print(f"    Acid generation: {rate.acid_generation_rate:.2e} mol H⁺/(m³·s)")
                print(f"    Wind speed: {rate.wind_speed:.2f} m/s")
                print(f"    O₂ delivery factor: {rate.O2_delivery_factor:.3f}")
                print()
        
        # Step 10: Temperature sensitivity
        print("-" * 70)
        print("STEP 10: Temperature Sensitivity of Kinetics")
        print("-" * 70)
        
        print(f"\nActivation energy: 45 kJ/mol (Nicholson et al. 1990)")
        print(f"Current temperature: {temperature - 273.15:.1f}°C")
        
        for T in [273.15 + 5, 273.15 + 15, 273.15 + 25]:  # 5, 15, 25°C
            # Quick estimate using Arrhenius
            exponent = -45000/8.314 * (1/T - 1/temperature)
            factor = np.exp(exponent)
            print(f"  At {T - 273.15:2.0f}°C: ~{factor:.2f}× rate change")
        
        # Step 11: Output summary
        print("\n" + "-" * 70)
        print("STEP 11: Output Files")
        print("-" * 70)
        
        print(f"\n✓ Results saved to: {output_dir}/")
        print(f"  - CSV: oxidation_rates.csv (detailed rates for each site)")
        print(f"  - GeoJSON: oxidation_rates.geojson (for GIS visualization)")
        
        print("\n" + "=" * 70)
        print("✓ Example completed successfully!")
        print("=" * 70)
        
        print("\nNext steps:")
        print("  1. Review oxidation rate results in CSV format")
        print("  2. Visualize spatial distribution using GeoJSON in QGIS")
        print("  3. Compare high-oxidation sites with field AMD observations")
        print("  4. Use oxidation rates as kinetic boundary conditions in PHREEQC")
        print("  5. Perform sensitivity analysis: How much do rates change with temperature?")
        
        wind.finalize()
        return 0
        
    except Exception as e:
        print(f"\n✗ Error: {e}", file=sys.stderr)
        import traceback
        traceback.print_exc()
        return 1


if __name__ == "__main__":
    sys.exit(main())
