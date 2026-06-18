#!/usr/bin/env python3
"""
Example: Data Center Siting Analysis

This example demonstrates how to use the SitingAnalyzer to evaluate
multiple candidate locations for data center deployment.

The analysis considers:
- Climate characterization (wind, temperature, humidity)
- Cooling efficiency (ambient temperatures, free cooling hours)
- Infrastructure resilience (wind extremes, flood risk)
- Environmental impact (heat island, water use)
"""

import sys
import os

# Add parent directory to path for imports
sys.path.insert(0, os.path.dirname(__file__))

from datacenter_siting import (
    SitingAnalyzer, CandidateSite, ClimateProfile, SitingPriority
)
from dataclasses import asdict


def create_sample_sites():
    """Create sample candidate sites for demonstration."""
    sites = [
        CandidateSite(
            site_id="site_mountain",
            x=100000.0,
            y=200000.0,
            z=800.0,
            label="Mountain Valley DC",
            water_availability=0.8,
            proximity_to_grid=45.0,
            land_cost_index=0.3,
        ),
        CandidateSite(
            site_id="site_coastal",
            x=150000.0,
            y=250000.0,
            z=50.0,
            label="Coastal Plain DC",
            water_availability=0.6,
            proximity_to_grid=30.0,
            land_cost_index=0.6,
        ),
        CandidateSite(
            site_id="site_high_elev",
            x=120000.0,
            y=180000.0,
            z=1200.0,
            label="High Elevation DC",
            water_availability=0.7,
            proximity_to_grid=60.0,
            land_cost_index=0.4,
        ),
        CandidateSite(
            site_id="site_river",
            x=110000.0,
            y=210000.0,
            z=300.0,
            label="River Basin DC",
            water_availability=0.9,
            proximity_to_grid=40.0,
            land_cost_index=0.5,
        ),
    ]
    return sites


def create_sample_climate_profiles():
    """Create sample climate profiles for each site."""
    profiles = {
        "site_mountain": ClimateProfile(
            site_id="site_mountain",
            wind_mean=7.5,
            wind_p95=14.2,
            wind_extreme_10yr=25.0,
            wind_extreme_50yr=32.0,
            wind_extreme_100yr=36.0,
            wind_max_gust=18.5,
            wind_variability=0.35,
            temp_mean=10.0,  # Cool at high elevation
            temp_min=-5.0,
            temp_max=28.0,
            temp_std=8.0,
            temp_above_30C=0.0,
            humidity_mean=45.0,  # Lower due to elevation
            humidity_max=85.0,
            free_cooling_hours=7200.0,  # More hours at cool location
            evaporation_rate=1000.0,
            heat_island_elevation=0.5,
            flood_risk_score=0.15,
            terrain_slope_mean=12.0,
            air_quality_index=0.2,
        ),
        "site_coastal": ClimateProfile(
            site_id="site_coastal",
            wind_mean=8.5,  # Higher winds
            wind_p95=16.0,
            wind_extreme_10yr=28.0,
            wind_extreme_50yr=35.0,
            wind_extreme_100yr=39.0,
            wind_max_gust=21.0,
            wind_variability=0.40,
            temp_mean=15.0,  # Moderate coastal temperature
            temp_min=5.0,
            temp_max=32.0,
            temp_std=7.5,
            temp_above_30C=25.0,
            humidity_mean=65.0,  # Higher due to ocean
            humidity_max=95.0,
            free_cooling_hours=5500.0,
            evaporation_rate=1500.0,
            heat_island_elevation=1.2,
            flood_risk_score=0.35,  # Coastal flood risk
            terrain_slope_mean=3.0,
            air_quality_index=0.35,
        ),
        "site_high_elev": ClimateProfile(
            site_id="site_high_elev",
            wind_mean=9.2,  # Highest winds
            wind_p95=17.5,
            wind_extreme_10yr=30.0,
            wind_extreme_50yr=37.0,
            wind_extreme_100yr=41.0,
            wind_max_gust=22.0,
            wind_variability=0.42,
            temp_mean=8.0,  # Very cool
            temp_min=-10.0,
            temp_max=25.0,
            temp_std=9.0,
            temp_above_30C=0.0,
            humidity_mean=40.0,  # Very dry
            humidity_max=80.0,
            free_cooling_hours=7800.0,  # Most cooling hours
            evaporation_rate=900.0,
            heat_island_elevation=0.2,
            flood_risk_score=0.05,
            terrain_slope_mean=18.0,  # Steep
            air_quality_index=0.15,
        ),
        "site_river": ClimateProfile(
            site_id="site_river",
            wind_mean=6.5,  # Lower winds
            wind_p95=12.5,
            wind_extreme_10yr=22.0,
            wind_extreme_50yr=28.0,
            wind_extreme_100yr=32.0,
            wind_max_gust=16.0,
            wind_variability=0.32,
            temp_mean=12.0,
            temp_min=-2.0,
            temp_max=30.0,
            temp_std=8.2,
            temp_above_30C=5.0,
            humidity_mean=60.0,
            humidity_max=92.0,
            free_cooling_hours=6800.0,
            evaporation_rate=1300.0,
            heat_island_elevation=0.8,
            flood_risk_score=0.45,  # River flood risk
            terrain_slope_mean=6.0,
            air_quality_index=0.25,
        ),
    }
    return profiles


def main():
    """Run the data center siting analysis example."""
    
    print("=" * 70)
    print("DATA CENTER SITING ANALYSIS - EXAMPLE")
    print("=" * 70)
    print()
    
    # Create candidate sites
    sites = create_sample_sites()
    print(f"Created {len(sites)} candidate sites:")
    for site in sites:
        print(f"  - {site.label} at ({site.x}, {site.y}, {site.z}m)")
    print()
    
    # Test different priority scenarios
    priorities = [
        SitingPriority.BALANCED,
        SitingPriority.COOLING_EFFICIENCY,
        SitingPriority.RESILIENCE,
        SitingPriority.ENVIRONMENTAL,
    ]
    
    for priority in priorities:
        print("-" * 70)
        print(f"ANALYSIS: {priority.value.upper()}")
        print("-" * 70)
        print()
        
        # Create analyzer
        analyzer = SitingAnalyzer(sites, priority=priority)
        
        # Populate with sample climate profiles
        profiles = create_sample_climate_profiles()
        for site_id, profile in profiles.items():
            analyzer.climate_profiles[site_id] = profile
            for site in sites:
                if site.site_id == site_id:
                    site.climate_profile = asdict(profile)
        
        # Evaluate
        evaluations = analyzer.evaluate_all_sites()
        
        # Print results
        print(f"Priority weights: {analyzer.weights}\n")
        for eval in evaluations:
            print(f"{eval['scores']['rank']}. {eval['label']:30s} "
                  f"Score: {eval['scores']['overall_score']:.3f}")
            print(f"     Cooling: {eval['scores']['cooling_efficiency']:.3f} | "
                  f"Resilience: {eval['scores']['wind_resilience']:.3f} | "
                  f"Flood: {eval['scores']['flood_safety']:.3f} | "
                  f"Environment: {eval['scores']['environmental_impact']:.3f}")
        
        print()
    
    # Generate final reports with balanced priority
    print("-" * 70)
    print("GENERATING FINAL REPORTS (Balanced Priority)")
    print("-" * 70)
    print()
    
    analyzer = SitingAnalyzer(sites, priority=SitingPriority.BALANCED)
    profiles = create_sample_climate_profiles()
    for site_id, profile in profiles.items():
        analyzer.climate_profiles[site_id] = profile
        for site in sites:
            if site.site_id == site_id:
                site.climate_profile = asdict(profile)
    
    # Generate reports
    analyzer.generate_report(
        json_output="example_siting_report.json",
        csv_output="example_siting_scores.csv"
    )
    
    # Generate plots if matplotlib available
    try:
        analyzer.plot_results(
            scores_plot="example_siting_scores.png",
            pareto_plot="example_pareto_frontier.png"
        )
    except Exception as e:
        print(f"Note: Could not generate plots: {e}")
    
    print()
    print("Example completed!")


if __name__ == "__main__":
    main()
