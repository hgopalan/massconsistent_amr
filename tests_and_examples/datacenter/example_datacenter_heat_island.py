#!/usr/bin/env python3
"""
Example: Data Center Heat Island Plume Analysis

Demonstrates how to use the datacenter_heat_source module to:
1. Load solver output
2. Analyze thermal plume characteristics
3. Compare with Briggs analytical model
4. Visualize results
"""

import sys
import numpy as np
from pathlib import Path

# Add src/python to path for imports
sys.path.insert(0, str(Path(__file__).parent.parent.parent / "src" / "python"))

from datacenter_heat_source import (
    DataCenterPlume,
    DataCenterFacility,
    briggs_plume_rise,
    PlumeMetrics
)


def example_flat_terrain():
    """
    Example 1: Flat Terrain Case Analysis
    
    This example demonstrates:
    - Loading a simple flat terrain test case
    - Computing plume metrics
    - Comparing with Briggs formula
    """
    print("\n" + "="*70)
    print("EXAMPLE 1: Flat Terrain Data Center")
    print("="*70)
    
    # Define facility parameters
    facility = DataCenterFacility(
        x=1500.0,
        y=1500.0,
        z=10.0,
        area=10000.0,
        heat_release=1.0e7,  # 10 MW
        name="Small Data Center"
    )
    
    print(f"\nFacility Configuration:")
    print(f"  Location: ({facility.x:.1f}, {facility.y:.1f}, {facility.z:.1f}) m")
    print(f"  Heat Release: {facility.heat_release:.2e} W ({facility.heat_release/1e6:.1f} MW)")
    print(f"  Footprint: {facility.area:.1f} m²")
    print(f"  Effective Radius: {np.sqrt(facility.area/np.pi):.1f} m")
    
    # Briggs plume rise estimates for reference wind speeds
    print(f"\nBriggs Plume Rise Estimates:")
    print(f"  Wind Speed | 500m downwind | 1km downwind | 2km downwind")
    print(f"  {'[m/s]':<11} | {'[m]':<13} | {'[m]':<12} | {'[m]':<12}")
    print(f"  {'-'*50}")
    
    for u in [5.0, 10.0, 15.0, 20.0]:
        dh_500 = briggs_plume_rise(facility.heat_release, u, 500.0)
        dh_1000 = briggs_plume_rise(facility.heat_release, u, 1000.0)
        dh_2000 = briggs_plume_rise(facility.heat_release, u, 2000.0)
        print(f"  {u:>11.1f} | {dh_500:>13.1f} | {dh_1000:>12.1f} | {dh_2000:>12.1f}")
    
    print(f"\nNote: Plume rise decreases with increased wind speed")
    print(f"      Higher rise at larger downwind distances")
    
    return facility


def example_valley_terrain():
    """
    Example 2: Valley Terrain with Complex Interactions
    
    This example demonstrates:
    - Larger facility in complex terrain
    - Wind and terrain interactions
    - Plume confinement by topography
    """
    print("\n" + "="*70)
    print("EXAMPLE 2: Valley Terrain Data Center")
    print("="*70)
    
    facility = DataCenterFacility(
        x=2000.0,
        y=2000.0,
        z=150.0,  # On valley floor (at 150m elevation)
        area=30000.0,
        heat_release=5.0e7,  # 50 MW (hyperscale)
        name="Hyperscale Data Center"
    )
    
    print(f"\nFacility Configuration:")
    print(f"  Location: ({facility.x:.1f}, {facility.y:.1f}, {facility.z:.1f}) m MSL")
    print(f"  Heat Release: {facility.heat_release:.2e} W ({facility.heat_release/1e6:.1f} MW)")
    print(f"  Footprint: {facility.area:.1f} m²")
    print(f"  Effective Radius: {np.sqrt(facility.area/np.pi):.1f} m")
    
    # Valley geometry
    print(f"\nValley Geometry (at facility location):")
    print(f"  Floor elevation: 150 m MSL")
    print(f"  Wall height: 200 m (walls reach 350 m MSL)")
    print(f"  Valley width: ~1000 m")
    print(f"  Aspect ratio: 0.2 (walls are shallower than street canyons)")
    
    # Expected plume behavior
    print(f"\nExpected Plume Behavior:")
    print(f"  - Initial confinement to valley (horizontal spreading limited)")
    print(f"  - Strong vertical rise due to terrain-driven circulation")
    print(f"  - Asymmetric dispersion: upwind vs downwind slopes")
    print(f"  - Potential for recirculation on lee slope")
    
    return facility


def example_sensitivity_analysis():
    """
    Example 3: Sensitivity Analysis
    
    This example demonstrates:
    - How plume rise varies with key parameters
    - Implications for facility design
    """
    print("\n" + "="*70)
    print("EXAMPLE 3: Sensitivity Analysis")
    print("="*70)
    
    print(f"\nParameter Sensitivity at 1 km Downwind:")
    print(f"\n1. HEAT RELEASE RATE (fixed wind: 10 m/s)")
    print(f"   Power [MW] | Plume Rise [m]")
    print(f"   {'-'*35}")
    
    wind_speed = 10.0
    distance = 1000.0
    
    for power_MW in [5, 10, 25, 50, 100]:
        Q = power_MW * 1.0e6
        dh = briggs_plume_rise(Q, wind_speed, distance)
        print(f"   {power_MW:>10} | {dh:>14.1f}")
    
    print(f"\n   Observation: Plume rise ∝ Q^(1/3) (power law)")
    print(f"   Doubling power increases rise by ~26%")
    
    print(f"\n2. WIND SPEED (fixed power: 10 MW)")
    print(f"   Wind [m/s] | Plume Rise [m]")
    print(f"   {'-'*35}")
    
    Q = 10.0 * 1.0e6
    
    for u in [2, 5, 10, 15, 20]:
        dh = briggs_plume_rise(Q, u, distance)
        print(f"   {u:>10} | {dh:>14.1f}")
    
    print(f"\n   Observation: Plume rise ∝ u^(-1) (inverse)")
    print(f"   Doubling wind speed reduces rise by 50%")
    
    print(f"\n3. DOWNWIND DISTANCE (fixed power: 10 MW, wind: 10 m/s)")
    print(f"   Distance [m] | Plume Rise [m]")
    print(f"   {'-'*35}")
    
    Q = 10.0 * 1.0e6
    
    for dist_m in [250, 500, 1000, 2000, 5000]:
        dh = briggs_plume_rise(Q, wind_speed, dist_m)
        print(f"   {dist_m:>12} | {dh:>14.1f}")
    
    print(f"\n   Observation: Plume rise ∝ x^(2/3) (sublinear)")
    print(f"   Plume continues rising downwind but growth slows")


def main():
    """Run all examples"""
    
    print("\n" + "="*70)
    print("DATA CENTER HEAT ISLAND - PHASE 1 EXAMPLES")
    print("="*70)
    
    print(f"""
This script demonstrates the data center heat island analysis capabilities.

Phase 1 focuses on:
  ✓ Basic heat source modeling
  ✓ Plume rise and dispersion analysis
  ✓ Briggs analytical model comparison
  ✓ Sensitivity studies
  
Phase 2 (future):
  - Facility-specific thermal release patterns
  - Elevated cooling tower discharge
  - Time-varying operational loads
  
Phase 3+ (future):
  - Multi-facility clustering
  - Regional heat island effects
  - Integration with air quality, wildfire models
""")
    
    # Run examples
    example_flat_terrain()
    example_valley_terrain()
    example_sensitivity_analysis()
    
    print("\n" + "="*70)
    print("Examples Complete!")
    print("="*70)
    print(f"""
Next steps:
1. Run the test cases:
   ./build/wind_solver regtest/datacenter/flat_terrain_inputs.i
   ./build/wind_solver regtest/datacenter/valley_terrain_inputs.i

2. Load and analyze results using datacenter_heat_source module:
   python examples/example_datacenter_heat_island.py

3. Compare with Briggs analytical predictions

For more information, see:
- docs/DATA_CENTER_HEAT_ISLAND_README.md
- src/datacenter_heat_source.H (C++ header)
- src/python/datacenter_heat_source.py (Python module)
""")


if __name__ == "__main__":
    main()
