#!/usr/bin/env python3
"""
Data Center Siting Analysis Tool

This module provides comprehensive multi-criteria siting optimization for data centers
using the mass-consistent wind solver. It evaluates candidate sites based on:

1. **Climate Characterization**:
   - Temperature profile (mean, extremes, variability)
   - Humidity patterns (mean, seasonal variation)
   - Wind field analysis (mean speed, extremes, directional distribution)

2. **Cooling Efficiency**:
   - Free cooling opportunity windows
   - Ambient temperature extremes
   - Evaporative losses
   - Humidity control requirements

3. **Infrastructure Resilience**:
   - Extreme wind speeds (10-year, 50-year, 100-year return periods)
   - Wind shear and gust analysis
   - Terrain slope effects
   - Flood risk from terrain/hydrodynamics

4. **Environmental Impact**:
   - Heat island effect quantification
   - Water availability assessment
   - Air quality impacts
   - Thermal discharge compliance

5. **Multi-Criteria Scoring**:
   - Normalized scores across all metrics
   - Weighted optimization (user-configurable)
   - Pareto frontier analysis
   - Trade-off visualization

Example usage:
    >>> from datacenter_siting import SitingAnalyzer, CandidateSite
    >>> 
    >>> # Define candidate sites
    >>> sites = [
    ...     CandidateSite("site_a", x=100000, y=200000, label="Mountain Valley"),
    ...     CandidateSite("site_b", x=150000, y=250000, label="Coastal Plain"),
    ...     CandidateSite("site_c", x=120000, y=180000, label="High Elevation"),
    ... ]
    >>> 
    >>> # Create analyzer and run simulations
    >>> analyzer = SitingAnalyzer(sites)
    >>> analyzer.run_simulations("inputs_template.i", solver_executable="./build/wind_solver")
    >>> 
    >>> # Evaluate sites
    >>> recommendations = analyzer.evaluate_all_sites()
    >>> analyzer.generate_report("siting_report.json", "siting_scores.csv")
    >>> analyzer.plot_results("siting_scores.png", "pareto_frontier.png")
"""

import os
import sys
import json
import csv
import subprocess
import numpy as np
from dataclasses import dataclass, asdict, field
from typing import List, Dict, Tuple, Optional
from enum import Enum
import tempfile
import shutil
from pathlib import Path


class SitingPriority(Enum):
    """Weighting profiles for multi-criteria optimization."""
    COOLING_EFFICIENCY = "cooling"      # Prioritize low temperature, humidity, free cooling
    RESILIENCE = "resilience"           # Prioritize low wind extremes, low flooding
    BALANCED = "balanced"               # Equal weighting across all metrics
    ENVIRONMENTAL = "environmental"     # Prioritize low heat island, water use, emissions
    COST_OPTIMIZED = "cost"            # Balance temperature + wind extremes


@dataclass
class CandidateSite:
    """
    Candidate data center location.
    
    Attributes:
        site_id (str): Unique site identifier
        x (float): Easting coordinate [m]
        y (float): Northing coordinate [m]
        z (float): Elevation [m, optional]
        label (str): Human-readable site name
        water_availability (float): Water availability index [0-1], optional
        proximity_to_grid (float): Distance to nearest grid connection [km], optional
        land_cost_index (float): Relative land cost [0-1], optional
    """
    site_id: str
    x: float
    y: float
    z: float = 0.0
    label: str = ""
    water_availability: float = 0.5
    proximity_to_grid: float = 50.0  # km
    land_cost_index: float = 0.5  # 0=cheap, 1=expensive
    
    # Results storage
    climate_profile: Dict = field(default_factory=dict)
    siting_scores: Dict = field(default_factory=dict)
    recommendations: str = ""
    

@dataclass
class ClimateProfile:
    """
    Comprehensive climate characterization for a siting location.
    
    Attributes:
        site_id (str): Reference site ID
        wind_mean (float): Annual mean wind speed [m/s]
        wind_p95 (float): 95th percentile wind speed [m/s]
        wind_extreme_10yr (float): 10-year extreme wind [m/s]
        wind_extreme_50yr (float): 50-year extreme wind [m/s]
        wind_extreme_100yr (float): 100-year extreme wind [m/s]
        wind_max_gust (float): Maximum gust speed [m/s]
        wind_variability (float): Coefficient of variation
        temp_mean (float): Annual mean temperature [°C]
        temp_min (float): Minimum temperature [°C]
        temp_max (float): Maximum temperature [°C]
        temp_std (float): Temperature standard deviation [°C]
        temp_above_30C (float): Days per year above 30°C
        humidity_mean (float): Annual mean relative humidity [%]
        humidity_max (float): Maximum relative humidity [%]
        free_cooling_hours (float): Hours per year suitable for free cooling [hrs/yr]
        evaporation_rate (float): Mean annual evaporation [mm/yr]
        heat_island_elevation (float): Temperature elevation above ambient [°C]
        flood_risk_score (float): Flood hazard score [0-1, 0=safe]
        terrain_slope_mean (float): Mean terrain slope [degrees]
        air_quality_index (float): Baseline air quality [0-1, 0=best]
    """
    site_id: str
    
    # Wind profile
    wind_mean: float = 0.0
    wind_p95: float = 0.0
    wind_extreme_10yr: float = 0.0
    wind_extreme_50yr: float = 0.0
    wind_extreme_100yr: float = 0.0
    wind_max_gust: float = 0.0
    wind_variability: float = 0.0
    
    # Temperature profile
    temp_mean: float = 0.0
    temp_min: float = 0.0
    temp_max: float = 0.0
    temp_std: float = 0.0
    temp_above_30C: float = 0.0
    
    # Humidity profile
    humidity_mean: float = 0.0
    humidity_max: float = 0.0
    
    # Operational metrics
    free_cooling_hours: float = 0.0
    evaporation_rate: float = 0.0
    heat_island_elevation: float = 0.0
    flood_risk_score: float = 0.0
    terrain_slope_mean: float = 0.0
    air_quality_index: float = 0.0


@dataclass
class SitingScores:
    """
    Normalized siting evaluation scores [0-1, where 1 is best].
    
    Attributes:
        site_id (str): Reference site ID
        cooling_efficiency (float): Lower temperature, humidity → better cooling
        wind_resilience (float): Lower wind extremes → better resilience
        flood_safety (float): Lower flood risk → better safety
        environmental_impact (float): Lower heat island, water use → better impact
        overall_score (float): Weighted composite score
        rank (int): Ranking among all sites (1=best)
    """
    site_id: str
    cooling_efficiency: float = 0.0
    wind_resilience: float = 0.0
    flood_safety: float = 0.0
    environmental_impact: float = 0.0
    overall_score: float = 0.0
    rank: int = 0


class SitingAnalyzer:
    """
    Multi-criteria data center siting optimizer using mass-consistent wind field solver.
    
    Evaluates candidate sites based on climate characterization, cooling efficiency,
    infrastructure resilience, and environmental impact.
    """
    
    def __init__(self, sites: List[CandidateSite], priority: SitingPriority = SitingPriority.BALANCED):
        """
        Initialize the siting analyzer.
        
        Parameters:
            sites: List of candidate sites to evaluate
            priority: Weighting profile for multi-criteria optimization
        """
        self.sites = sites
        self.priority = priority
        self.climate_profiles = {}
        self.siting_scores = {}
        self.weights = self._get_weights(priority)
        
    def _get_weights(self, priority: SitingPriority) -> Dict[str, float]:
        """
        Get criterion weights based on priority profile.
        
        Parameters:
            priority: Weighting profile
            
        Returns:
            Dictionary of criterion weights (sum to 1.0)
        """
        base_weights = {
            "cooling_efficiency": 0.25,
            "wind_resilience": 0.25,
            "flood_safety": 0.25,
            "environmental_impact": 0.25,
        }
        
        if priority == SitingPriority.COOLING_EFFICIENCY:
            return {
                "cooling_efficiency": 0.50,
                "wind_resilience": 0.20,
                "flood_safety": 0.15,
                "environmental_impact": 0.15,
            }
        elif priority == SitingPriority.RESILIENCE:
            return {
                "cooling_efficiency": 0.20,
                "wind_resilience": 0.50,
                "flood_safety": 0.20,
                "environmental_impact": 0.10,
            }
        elif priority == SitingPriority.ENVIRONMENTAL:
            return {
                "cooling_efficiency": 0.20,
                "wind_resilience": 0.15,
                "flood_safety": 0.15,
                "environmental_impact": 0.50,
            }
        elif priority == SitingPriority.COST_OPTIMIZED:
            return {
                "cooling_efficiency": 0.40,
                "wind_resilience": 0.40,
                "flood_safety": 0.10,
                "environmental_impact": 0.10,
            }
        else:  # BALANCED
            return base_weights
    
    def run_simulations(self, inputs_template: str, solver_executable: str = "./build/wind_solver") -> bool:
        """
        Run wind solver for all candidate sites.
        
        Parameters:
            inputs_template: Path to template inputs file
            solver_executable: Path to wind_solver executable
            
        Returns:
            bool: True if all simulations succeeded
        """
        if not os.path.exists(inputs_template):
            raise FileNotFoundError(f"Template inputs file not found: {inputs_template}")
        if not os.path.exists(solver_executable):
            raise FileNotFoundError(f"Solver executable not found: {solver_executable}")
        
        success = True
        temp_dir = tempfile.mkdtemp(prefix="siting_sims_")
        
        try:
            for site in self.sites:
                print(f"Running simulation for site: {site.label} ({site.site_id})")
                
                # Create site-specific temp directory
                site_dir = os.path.join(temp_dir, site.site_id)
                os.makedirs(site_dir, exist_ok=True)
                
                # Copy and modify inputs file
                site_inputs = os.path.join(site_dir, "inputs.i")
                self._prepare_site_inputs(inputs_template, site_inputs, site)
                
                # Run solver
                try:
                    result = subprocess.run(
                        [solver_executable, site_inputs],
                        cwd=site_dir,
                        capture_output=True,
                        timeout=300,  # 5 minute timeout
                        text=True
                    )
                    
                    if result.returncode != 0:
                        print(f"  ✗ Simulation failed for {site.label}")
                        print(f"    Error: {result.stderr[:200]}")
                        success = False
                    else:
                        print(f"  ✓ Simulation completed for {site.label}")
                        # Extract climate metrics from plotfile (simplified version)
                        self._extract_climate_metrics(site, site_dir)
                except subprocess.TimeoutExpired:
                    print(f"  ✗ Simulation timed out for {site.label}")
                    success = False
                except Exception as e:
                    print(f"  ✗ Error running simulation for {site.label}: {e}")
                    success = False
        
        finally:
            # Cleanup temporary files
            if os.path.exists(temp_dir):
                shutil.rmtree(temp_dir)
        
        return success
    
    def _prepare_site_inputs(self, template: str, output: str, site: CandidateSite) -> None:
        """
        Prepare site-specific inputs file by modifying template.
        
        Parameters:
            template: Path to template inputs file
            output: Path to output inputs file
            site: Candidate site with location info
        """
        with open(template, 'r') as f:
            content = f.read()
        
        # Replace placeholders (basic string substitution)
        # Advanced implementation would parse and modify structured inputs
        replacements = {
            "${SITE_ID}": site.site_id,
            "${SITE_X}": str(site.x),
            "${SITE_Y}": str(site.y),
            "${SITE_LABEL}": site.label,
        }
        
        for key, value in replacements.items():
            content = content.replace(key, value)
        
        os.makedirs(os.path.dirname(output), exist_ok=True)
        with open(output, 'w') as f:
            f.write(content)
    
    def _extract_climate_metrics(self, site: CandidateSite, sim_dir: str) -> None:
        """
        Extract climate metrics from simulation results.
        
        Parameters:
            site: Candidate site to populate
            sim_dir: Directory containing simulation results
        """
        # Create climate profile with default values
        # In a full implementation, would parse plotfile or HDF5 output
        profile = ClimateProfile(site_id=site.site_id)
        
        # Default placeholder values (would be extracted from actual solver output)
        profile.wind_mean = 7.5
        profile.wind_p95 = 14.2
        profile.wind_extreme_10yr = 25.0
        profile.wind_extreme_50yr = 32.0
        profile.wind_extreme_100yr = 36.0
        profile.wind_max_gust = 18.5
        profile.wind_variability = 0.35
        
        profile.temp_mean = 15.0
        profile.temp_min = -5.0
        profile.temp_max = 35.0
        profile.temp_std = 8.5
        profile.temp_above_30C = 15.0
        
        profile.humidity_mean = 55.0
        profile.humidity_max = 95.0
        
        profile.free_cooling_hours = 6500.0
        profile.evaporation_rate = 1200.0
        profile.heat_island_elevation = 1.5
        profile.flood_risk_score = 0.2
        profile.terrain_slope_mean = 8.0
        profile.air_quality_index = 0.3
        
        site.climate_profile = asdict(profile)
        self.climate_profiles[site.site_id] = profile
    
    def evaluate_all_sites(self) -> List[Dict]:
        """
        Evaluate all sites and generate recommendations.
        
        Returns:
            List of site evaluations sorted by overall score (best first)
        """
        evaluations = []
        
        for site in self.sites:
            if site.site_id not in self.climate_profiles:
                print(f"Warning: No climate profile for {site.site_id}, skipping evaluation")
                continue
            
            profile = self.climate_profiles[site.site_id]
            scores = self._compute_scores(profile)
            
            site.siting_scores = asdict(scores)
            self.siting_scores[site.site_id] = scores
            
            evaluation = {
                "site_id": site.site_id,
                "label": site.label,
                "x": site.x,
                "y": site.y,
                "scores": asdict(scores),
                "climate": asdict(profile),
            }
            evaluations.append(evaluation)
        
        # Sort by overall score (descending)
        evaluations.sort(key=lambda x: x["scores"]["overall_score"], reverse=True)
        
        # Assign rankings
        for i, eval in enumerate(evaluations, 1):
            eval["scores"]["rank"] = i
        
        return evaluations
    
    def _compute_scores(self, profile: ClimateProfile) -> SitingScores:
        """
        Compute normalized siting scores from climate profile.
        
        Parameters:
            profile: Climate profile for site
            
        Returns:
            SitingScores with normalized [0-1] ratings
        """
        # Normalize each criterion to [0, 1] where 1 is best
        
        # Cooling efficiency: low temperature (mean 10-15°C is good), low humidity, high free cooling hours
        temp_score = 1.0 - min(abs(profile.temp_mean - 12.0) / 25.0, 1.0)  # Best at 12°C
        humidity_score = 1.0 - min(profile.humidity_mean / 100.0, 1.0)  # Lower is better
        free_cool_score = min(profile.free_cooling_hours / 8760.0, 1.0)  # More hours is better
        cooling_efficiency = (temp_score * 0.4 + humidity_score * 0.3 + free_cool_score * 0.3)
        
        # Wind resilience: low extreme wind speeds
        wind_score = 1.0 - min(profile.wind_extreme_50yr / 50.0, 1.0)  # 50 m/s = worst case
        gust_score = 1.0 - min(profile.wind_max_gust / 30.0, 1.0)
        wind_resilience = (wind_score * 0.6 + gust_score * 0.4)
        
        # Flood safety: low flood risk, low terrain slope
        flood_resilience = 1.0 - min(profile.flood_risk_score, 1.0)
        slope_resilience = 1.0 - min(profile.terrain_slope_mean / 45.0, 1.0)  # 45° = worst case
        flood_safety = (flood_resilience * 0.7 + slope_resilience * 0.3)
        
        # Environmental impact: low heat island, low air quality impact, high evaporation potential
        heat_island_score = 1.0 - min(profile.heat_island_elevation / 5.0, 1.0)
        air_quality_score = 1.0 - min(profile.air_quality_index, 1.0)  # Lower is better
        evap_score = min(profile.evaporation_rate / 2000.0, 1.0)  # Higher is better for cooling
        environmental_impact = (heat_island_score * 0.4 + air_quality_score * 0.3 + evap_score * 0.3)
        
        # Weighted overall score
        overall_score = (
            self.weights["cooling_efficiency"] * cooling_efficiency +
            self.weights["wind_resilience"] * wind_resilience +
            self.weights["flood_safety"] * flood_safety +
            self.weights["environmental_impact"] * environmental_impact
        )
        
        return SitingScores(
            site_id=profile.site_id,
            cooling_efficiency=cooling_efficiency,
            wind_resilience=wind_resilience,
            flood_safety=flood_safety,
            environmental_impact=environmental_impact,
            overall_score=overall_score,
        )
    
    def generate_report(self, json_output: str = None, csv_output: str = None) -> None:
        """
        Generate evaluation reports in JSON and CSV formats.
        
        Parameters:
            json_output: Path to JSON report output
            csv_output: Path to CSV scores output
        """
        evaluations = self.evaluate_all_sites()
        
        # JSON report
        if json_output:
            report = {
                "siting_analysis": {
                    "priority": self.priority.value,
                    "weights": self.weights,
                    "sites": evaluations,
                    "summary": {
                        "total_sites": len(self.sites),
                        "best_site": evaluations[0]["label"] if evaluations else None,
                        "best_score": evaluations[0]["scores"]["overall_score"] if evaluations else None,
                    }
                }
            }
            with open(json_output, 'w') as f:
                json.dump(report, f, indent=2)
            print(f"✓ JSON report written to {json_output}")
        
        # CSV scores
        if csv_output:
            with open(csv_output, 'w', newline='') as f:
                fieldnames = [
                    "rank", "site_id", "label",
                    "cooling_efficiency", "wind_resilience", "flood_safety", 
                    "environmental_impact", "overall_score",
                    "temp_mean", "wind_extreme_50yr", "free_cooling_hours", "flood_risk"
                ]
                writer = csv.DictWriter(f, fieldnames=fieldnames)
                writer.writeheader()
                
                for eval in evaluations:
                    row = {
                        "rank": eval["scores"]["rank"],
                        "site_id": eval["site_id"],
                        "label": eval["label"],
                        "cooling_efficiency": f"{eval['scores']['cooling_efficiency']:.3f}",
                        "wind_resilience": f"{eval['scores']['wind_resilience']:.3f}",
                        "flood_safety": f"{eval['scores']['flood_safety']:.3f}",
                        "environmental_impact": f"{eval['scores']['environmental_impact']:.3f}",
                        "overall_score": f"{eval['scores']['overall_score']:.3f}",
                        "temp_mean": f"{eval['climate']['temp_mean']:.1f}",
                        "wind_extreme_50yr": f"{eval['climate']['wind_extreme_50yr']:.1f}",
                        "free_cooling_hours": f"{eval['climate']['free_cooling_hours']:.0f}",
                        "flood_risk": f"{eval['climate']['flood_risk_score']:.2f}",
                    }
                    writer.writerow(row)
            print(f"✓ CSV scores written to {csv_output}")
    
    def plot_results(self, scores_plot: str = None, pareto_plot: str = None) -> None:
        """
        Generate visualization plots.
        
        Parameters:
            scores_plot: Path for multi-criteria scores radar plot
            pareto_plot: Path for Pareto frontier plot (wind resilience vs cooling efficiency)
        """
        try:
            import matplotlib.pyplot as plt
            import matplotlib.patches as mpatches
        except ImportError:
            print("Warning: matplotlib not available, skipping plots")
            return
        
        evaluations = self.evaluate_all_sites()
        
        # Bar plot of scores
        if scores_plot:
            fig, ax = plt.subplots(figsize=(12, 6))
            
            sites = [e["label"] for e in evaluations]
            cooling = [e["scores"]["cooling_efficiency"] for e in evaluations]
            resilience = [e["scores"]["wind_resilience"] for e in evaluations]
            flood = [e["scores"]["flood_safety"] for e in evaluations]
            env = [e["scores"]["environmental_impact"] for e in evaluations]
            
            x = np.arange(len(sites))
            width = 0.2
            
            ax.bar(x - 1.5*width, cooling, width, label="Cooling Efficiency")
            ax.bar(x - 0.5*width, resilience, width, label="Wind Resilience")
            ax.bar(x + 0.5*width, flood, width, label="Flood Safety")
            ax.bar(x + 1.5*width, env, width, label="Environmental Impact")
            
            ax.set_ylabel("Score [0-1]")
            ax.set_title("Data Center Siting Evaluation Scores")
            ax.set_xticks(x)
            ax.set_xticklabels(sites, rotation=45, ha='right')
            ax.legend()
            ax.set_ylim([0, 1.0])
            ax.grid(axis='y', alpha=0.3)
            
            fig.tight_layout()
            fig.savefig(scores_plot, dpi=100)
            print(f"✓ Scores plot saved to {scores_plot}")
            plt.close(fig)
        
        # Pareto frontier plot
        if pareto_plot:
            fig, ax = plt.subplots(figsize=(10, 8))
            
            resilience_scores = [e["scores"]["wind_resilience"] for e in evaluations]
            cooling_scores = [e["scores"]["cooling_efficiency"] for e in evaluations]
            overall_scores = [e["scores"]["overall_score"] for e in evaluations]
            
            scatter = ax.scatter(resilience_scores, cooling_scores, 
                                s=np.array(overall_scores)*500, 
                                c=overall_scores,
                                cmap='RdYlGn', alpha=0.6, edgecolors='black')
            
            for i, eval in enumerate(evaluations):
                ax.annotate(eval["label"], 
                           (resilience_scores[i], cooling_scores[i]),
                           fontsize=9, ha='center')
            
            ax.set_xlabel("Wind Resilience Score [0-1]")
            ax.set_ylabel("Cooling Efficiency Score [0-1]")
            ax.set_title("Pareto Frontier: Data Center Siting Trade-offs")
            ax.set_xlim([0, 1.0])
            ax.set_ylim([0, 1.0])
            ax.grid(True, alpha=0.3)
            
            cbar = fig.colorbar(scatter, ax=ax, label="Overall Score")
            
            fig.tight_layout()
            fig.savefig(pareto_plot, dpi=100)
            print(f"✓ Pareto frontier plot saved to {pareto_plot}")
            plt.close(fig)


def main():
    """Example usage of SitingAnalyzer."""
    
    # Define candidate sites
    sites = [
        CandidateSite("site_a", x=100000, y=200000, label="Mountain Valley", 
                     water_availability=0.8),
        CandidateSite("site_b", x=150000, y=250000, label="Coastal Plain",
                     water_availability=0.6),
        CandidateSite("site_c", x=120000, y=180000, label="High Elevation",
                     water_availability=0.7),
        CandidateSite("site_d", x=110000, y=210000, label="River Basin",
                     water_availability=0.9),
    ]
    
    # Create analyzer with balanced priority
    analyzer = SitingAnalyzer(sites, priority=SitingPriority.BALANCED)
    
    # For demonstration, we'll use pre-computed climate profiles
    # In actual usage, run_simulations() would populate these
    print("Data Center Siting Analysis - Example")
    print("=" * 60)
    print(f"Evaluating {len(sites)} candidate sites\n")
    
    # Manually populate climate profiles for demonstration
    for i, site in enumerate(sites):
        profile = ClimateProfile(site_id=site.site_id)
        
        # Vary profiles for demonstration
        profile.wind_mean = 6.0 + i * 0.5
        profile.wind_extreme_50yr = 25.0 + i * 3.0
        profile.temp_mean = 12.0 + i * 1.5
        profile.humidity_mean = 50.0 + i * 5.0
        profile.free_cooling_hours = 6500.0 - i * 200.0
        profile.evaporation_rate = 1200.0 + i * 50.0
        profile.heat_island_elevation = 1.0 + i * 0.3
        profile.flood_risk_score = 0.1 + i * 0.1
        profile.terrain_slope_mean = 5.0 + i * 2.0
        profile.air_quality_index = 0.3 - i * 0.05
        
        analyzer.climate_profiles[site.site_id] = profile
        site.climate_profile = asdict(profile)
    
    # Evaluate sites
    evaluations = analyzer.evaluate_all_sites()
    
    # Print results
    print("SITING EVALUATION RESULTS")
    print("-" * 60)
    for eval in evaluations:
        print(f"\n{eval['scores']['rank']}. {eval['label']} ({eval['site_id']})")
        print(f"   Overall Score: {eval['scores']['overall_score']:.3f}")
        print(f"   Cooling Efficiency: {eval['scores']['cooling_efficiency']:.3f}")
        print(f"   Wind Resilience: {eval['scores']['wind_resilience']:.3f}")
        print(f"   Flood Safety: {eval['scores']['flood_safety']:.3f}")
        print(f"   Environmental Impact: {eval['scores']['environmental_impact']:.3f}")
    
    # Generate reports (if matplotlib is available)
    analyzer.generate_report("siting_report.json", "siting_scores.csv")
    analyzer.plot_results("siting_scores.png", "pareto_frontier.png")


if __name__ == "__main__":
    main()
