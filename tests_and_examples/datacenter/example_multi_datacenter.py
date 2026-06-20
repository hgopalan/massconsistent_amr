#!/usr/bin/env python3
"""
Example: Multiple Data Center Heat Island Analysis

Demonstrates analysis of multiple data center facilities with thermal
plume interactions, cumulative heating effects, and diagnostic metrics.
"""

import numpy as np
import pandas as pd
from datacenter_heat_source import (
    DataCenterPlume,
    DataCenterFacility,
    briggs_plume_rise
)


def example_multi_facility_analysis():
    """
    Analyze thermal signatures from multiple data center facilities.
    
    Demonstrates:
    1. Configuration of multiple facilities
    2. Computing individual and combined plume metrics
    3. Regional cumulative heating analysis
    4. Facility interaction diagnostics
    """
    
    print("\n" + "="*70)
    print("Multi-Facility Data Center Heat Island Analysis Example")
    print("="*70 + "\n")
    
    # Define three data center facilities
    facilities = [
        DataCenterFacility(
            x=1000.0, y=1000.0, z=10.0,
            area=10000.0, heat_release=1.0e7,
            name="DataCenter_A"
        ),
        DataCenterFacility(
            x=1500.0, y=2000.0, z=15.0,
            area=5000.0, heat_release=5.0e6,
            name="DataCenter_B"
        ),
        DataCenterFacility(
            x=2500.0, y=1500.0, z=12.0,
            area=8000.0, heat_release=8.0e6,
            name="DataCenter_C"
        ),
    ]
    
    print("Defined Facilities:")
    print("-" * 70)
    total_heat = 0.0
    for facility in facilities:
        print(f"\n{facility.name}:")
        print(f"  Location: ({facility.x:.1f}, {facility.y:.1f}, {facility.z:.1f}) m")
        print(f"  Heat release: {facility.heat_release / 1.0e6:.1f} MW")
        print(f"  Footprint area: {facility.area:.0f} m²")
        total_heat += facility.heat_release
    
    print(f"\nTotal combined heat release: {total_heat / 1.0e6:.1f} MW")
    print("\n" + "-" * 70)
    
    # Example 1: Briggs plume rise for each facility at 1 km downwind
    print("\nExample 1: Briggs Plume Rise at 1 km Downwind")
    print("-" * 70)
    
    wind_speed = 10.0  # m/s
    distance = 1000.0   # m
    
    for facility in facilities:
        rise = briggs_plume_rise(facility.heat_release, wind_speed, distance)
        print(f"\n{facility.name}:")
        print(f"  Heat release: {facility.heat_release / 1.0e6:.1f} MW")
        print(f"  Wind speed: {wind_speed:.1f} m/s")
        print(f"  Downwind distance: {distance:.0f} m")
        print(f"  Plume rise: {rise:.1f} m")
    
    # Example 2: Sensitivity analysis for all facilities
    print("\n\nExample 2: Sensitivity Analysis - Heat Release vs Wind Speed")
    print("-" * 70)
    
    wind_speeds = np.array([2.0, 5.0, 10.0, 15.0, 20.0])
    
    for facility in facilities:
        print(f"\n{facility.name} (Q = {facility.heat_release / 1.0e6:.1f} MW):")
        print(f"{'Wind Speed (m/s)':>20} {'Plume Rise (m)':>20}")
        print(f"{'-'*20} {'-'*20}")
        
        for u in wind_speeds:
            rise = briggs_plume_rise(facility.heat_release, u, distance)
            print(f"{u:>20.1f} {rise:>20.1f}")
    
    # Example 3: Inter-facility distances and potential interactions
    print("\n\nExample 3: Facility Separation Distances")
    print("-" * 70)
    
    print(f"\n{'Pair':>20} {'Distance (m)':>20} {'Potential Interaction':>30}")
    print(f"{'-'*20} {'-'*20} {'-'*30}")
    
    for i, fac_i in enumerate(facilities):
        for j, fac_j in enumerate(facilities[i+1:], start=i+1):
            dx = fac_j.x - fac_i.x
            dy = fac_j.y - fac_i.y
            dist = np.sqrt(dx**2 + dy**2)
            
            # Simple interaction criterion: plume widths at separation distance
            rise_i = briggs_plume_rise(fac_i.heat_release, 10.0, dist)
            rise_j = briggs_plume_rise(fac_j.heat_release, 10.0, dist)
            
            interaction = "Strong" if dist < 2000 else "Moderate" if dist < 3000 else "Weak"
            
            print(f"{fac_i.name} - {fac_j.name:>8} {dist:>20.0f} {interaction:>30}")
    
    # Example 4: Distance-downwind plume profiles for multiple facilities
    print("\n\nExample 4: Downwind Temperature Profiles")
    print("-" * 70)
    
    wind_direction = 270.0  # From west
    
    for facility in facilities:
        print(f"\n{facility.name} (Wind from {wind_direction}°):")
        print(f"{'Downwind Distance (m)':>25} {'Plume Rise (m)':>20}")
        print(f"{'-'*25} {'-'*20}")
        
        distances = np.array([250.0, 500.0, 1000.0, 2000.0, 5000.0])
        for dist in distances:
            rise = briggs_plume_rise(facility.heat_release, wind_speed, dist)
            print(f"{dist:>25.0f} {rise:>20.1f}")
    
    # Example 5: Regional cumulative effects
    print("\n\nExample 5: Regional Cumulative Heating Estimate")
    print("-" * 70)
    
    # Simple estimate: total heat distributed over typical mixing region
    domain_area = 4000 * 4000  # m²
    mixing_height = 200.0      # m
    mixing_volume = domain_area * mixing_height
    
    # Assume all heat mixes within domain
    total_heat_all = sum(f.heat_release for f in facilities)
    dT_regional = (total_heat_all / (1.225 * 1005.0 * mixing_volume))  # K
    
    print(f"\nEstimated Regional Temperature Rise:")
    print(f"  Total heat release: {total_heat_all / 1.0e6:.1f} MW")
    print(f"  Domain area: {domain_area / 1.0e6:.1f} km²")
    print(f"  Mixing layer height: {mixing_height:.0f} m")
    print(f"  Estimated ΔT (if fully mixed): {dT_regional:.3f} K")
    
    # Example 6: Facility-by-facility contribution
    print("\n\nExample 6: Individual Facility Contributions to Regional Heating")
    print("-" * 70)
    
    print(f"\n{'Facility':>20} {'Heat (MW)':>15} {'% of Total':>15} {'ΔT (K)':>15}")
    print(f"{'-'*20} {'-'*15} {'-'*15} {'-'*15}")
    
    for facility in facilities:
        pct = 100.0 * facility.heat_release / total_heat_all
        dT_indiv = (facility.heat_release / (1.225 * 1005.0 * mixing_volume))
        print(f"{facility.name:>20} {facility.heat_release/1.0e6:>15.1f} "
              f"{pct:>15.1f} {dT_indiv:>15.3f}")
    
    print("\n" + "="*70)
    print("Analysis complete!")
    print("="*70 + "\n")


if __name__ == "__main__":
    example_multi_facility_analysis()
