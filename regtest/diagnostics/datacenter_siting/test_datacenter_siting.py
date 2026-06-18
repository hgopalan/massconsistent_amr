#!/usr/bin/env python3
"""
Regression Test: Data Center Siting Analysis

Tests the SitingAnalyzer class for correct:
- Climate profile scoring
- Multi-criteria weighting
- Site ranking
- Report generation
"""

import sys
import os
import unittest
import json
import csv
import tempfile

# Add src/python to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', '..', 'src', 'python'))

from datacenter_siting import (
    SitingAnalyzer, CandidateSite, ClimateProfile, SitingPriority, SitingScores
)


class TestDataCenterSiting(unittest.TestCase):
    """Test suite for data center siting analysis."""
    
    def setUp(self):
        """Set up test fixtures."""
        self.sites = [
            CandidateSite("site_a", 100000, 200000, label="Site A"),
            CandidateSite("site_b", 150000, 250000, label="Site B"),
            CandidateSite("site_c", 120000, 180000, label="Site C"),
        ]
    
    def test_siting_analyzer_initialization(self):
        """Test SitingAnalyzer initialization."""
        analyzer = SitingAnalyzer(self.sites, priority=SitingPriority.BALANCED)
        self.assertEqual(len(analyzer.sites), 3)
        self.assertEqual(analyzer.priority, SitingPriority.BALANCED)
        self.assertEqual(sum(analyzer.weights.values()), 1.0)
    
    def test_weights_for_different_priorities(self):
        """Test that different priorities produce different weights."""
        priorities = [
            SitingPriority.BALANCED,
            SitingPriority.COOLING_EFFICIENCY,
            SitingPriority.RESILIENCE,
            SitingPriority.ENVIRONMENTAL,
            SitingPriority.COST_OPTIMIZED,
        ]
        
        for priority in priorities:
            analyzer = SitingAnalyzer(self.sites, priority=priority)
            # Check that weights sum to 1.0
            self.assertAlmostEqual(sum(analyzer.weights.values()), 1.0, places=5)
            # Check that all weights are positive
            for w in analyzer.weights.values():
                self.assertGreater(w, 0.0)
    
    def test_cooling_efficiency_priority_weights(self):
        """Test that cooling efficiency priority heavily weights cooling."""
        analyzer = SitingAnalyzer(self.sites, priority=SitingPriority.COOLING_EFFICIENCY)
        self.assertGreater(
            analyzer.weights["cooling_efficiency"],
            analyzer.weights["wind_resilience"]
        )
    
    def test_climate_profile_creation(self):
        """Test ClimateProfile dataclass."""
        profile = ClimateProfile(
            site_id="test",
            wind_mean=7.5,
            temp_mean=15.0,
            humidity_mean=55.0,
        )
        self.assertEqual(profile.site_id, "test")
        self.assertEqual(profile.wind_mean, 7.5)
        self.assertEqual(profile.temp_mean, 15.0)
    
    def test_score_computation_best_case(self):
        """Test score computation with ideal climate profile."""
        analyzer = SitingAnalyzer(self.sites, priority=SitingPriority.BALANCED)
        
        # Create ideal profile: cool, low wind, no flooding
        ideal_profile = ClimateProfile(
            site_id="ideal",
            wind_mean=5.0,
            wind_p95=10.0,
            wind_extreme_10yr=18.0,
            wind_extreme_50yr=20.0,
            wind_extreme_100yr=25.0,
            wind_max_gust=12.0,
            wind_variability=0.25,
            temp_mean=12.0,  # Ideal for cooling
            temp_min=0.0,
            temp_max=25.0,
            temp_std=5.0,
            temp_above_30C=0.0,
            humidity_mean=40.0,  # Low is better
            humidity_max=70.0,
            free_cooling_hours=8000.0,  # Many hours
            evaporation_rate=1500.0,
            heat_island_elevation=0.0,  # No heat island
            flood_risk_score=0.0,  # No flooding
            terrain_slope_mean=2.0,  # Flat
            air_quality_index=0.0,  # Perfect air
        )
        
        scores = analyzer._compute_scores(ideal_profile)
        
        # Ideal profile should have high scores
        self.assertGreater(scores.cooling_efficiency, 0.65)
        self.assertGreater(scores.wind_resilience, 0.55)
        self.assertGreater(scores.flood_safety, 0.85)
        self.assertGreater(scores.environmental_impact, 0.75)
        self.assertGreater(scores.overall_score, 0.70)
    
    def test_score_computation_worst_case(self):
        """Test score computation with poor climate profile."""
        analyzer = SitingAnalyzer(self.sites, priority=SitingPriority.BALANCED)
        
        # Create poor profile: hot, windy, flooding
        poor_profile = ClimateProfile(
            site_id="poor",
            wind_mean=15.0,
            wind_p95=25.0,
            wind_extreme_10yr=40.0,
            wind_extreme_50yr=50.0,
            wind_extreme_100yr=55.0,
            wind_max_gust=30.0,
            wind_variability=0.50,
            temp_mean=35.0,  # Very hot
            temp_min=20.0,
            temp_max=45.0,
            temp_std=10.0,
            temp_above_30C=200.0,  # Many hot days
            humidity_mean=80.0,  # High
            humidity_max=100.0,
            free_cooling_hours=2000.0,  # Few hours
            evaporation_rate=500.0,
            heat_island_elevation=5.0,  # High heat island
            flood_risk_score=0.95,  # Very high flood risk
            terrain_slope_mean=40.0,  # Very steep
            air_quality_index=0.95,  # Poor air
        )
        
        scores = analyzer._compute_scores(poor_profile)
        
        # Poor profile should have low scores
        self.assertLess(scores.cooling_efficiency, 0.3)
        self.assertLess(scores.wind_resilience, 0.3)
        self.assertLess(scores.flood_safety, 0.2)
        self.assertLess(scores.environmental_impact, 0.2)
        self.assertLess(scores.overall_score, 0.3)
    
    def test_site_evaluation_and_ranking(self):
        """Test complete site evaluation and ranking."""
        analyzer = SitingAnalyzer(self.sites, priority=SitingPriority.BALANCED)
        
        # Create profiles with different quality
        profiles = [
            ClimateProfile(
                site_id="site_a",
                wind_mean=7.5, wind_extreme_50yr=32.0,
                temp_mean=12.0, humidity_mean=50.0,
                free_cooling_hours=6500.0, flood_risk_score=0.2,
                terrain_slope_mean=10.0,
            ),
            ClimateProfile(
                site_id="site_b",
                wind_mean=8.5, wind_extreme_50yr=35.0,
                temp_mean=15.0, humidity_mean=60.0,
                free_cooling_hours=5500.0, flood_risk_score=0.4,
                terrain_slope_mean=5.0,
            ),
            ClimateProfile(
                site_id="site_c",
                wind_mean=6.5, wind_extreme_50yr=28.0,
                temp_mean=10.0, humidity_mean=45.0,
                free_cooling_hours=7200.0, flood_risk_score=0.1,
                terrain_slope_mean=15.0,
            ),
        ]
        
        for profile in profiles:
            analyzer.climate_profiles[profile.site_id] = profile
        
        evaluations = analyzer.evaluate_all_sites()
        
        # Should have 3 evaluations
        self.assertEqual(len(evaluations), 3)
        
        # Should be ranked 1, 2, 3
        ranks = [e["scores"]["rank"] for e in evaluations]
        self.assertEqual(ranks, [1, 2, 3])
        
        # Best should be site_c (coolest, lowest wind, best flood safety)
        self.assertEqual(evaluations[0]["site_id"], "site_c")
    
    def test_report_generation_json(self):
        """Test JSON report generation."""
        analyzer = SitingAnalyzer(self.sites, priority=SitingPriority.BALANCED)
        
        # Add sample profiles
        for i, site in enumerate(self.sites):
            profile = ClimateProfile(
                site_id=site.site_id,
                wind_mean=7.0 + i,
                wind_extreme_50yr=30.0 + i*2,
                temp_mean=12.0 + i,
                humidity_mean=50.0 + i*3,
                free_cooling_hours=6500.0 - i*100,
                flood_risk_score=0.1 + i*0.1,
                terrain_slope_mean=8.0 + i*2,
            )
            analyzer.climate_profiles[site.site_id] = profile
        
        # Generate JSON report
        with tempfile.NamedTemporaryFile(mode='w', suffix='.json', delete=False) as f:
            json_file = f.name
        
        try:
            analyzer.generate_report(json_output=json_file)
            
            # Verify JSON was created and is valid
            self.assertTrue(os.path.exists(json_file))
            with open(json_file, 'r') as f:
                report = json.load(f)
            
            # Check report structure
            self.assertIn("siting_analysis", report)
            self.assertIn("sites", report["siting_analysis"])
            self.assertIn("summary", report["siting_analysis"])
            self.assertEqual(len(report["siting_analysis"]["sites"]), 3)
        finally:
            if os.path.exists(json_file):
                os.remove(json_file)
    
    def test_report_generation_csv(self):
        """Test CSV report generation."""
        analyzer = SitingAnalyzer(self.sites, priority=SitingPriority.BALANCED)
        
        # Add sample profiles
        for i, site in enumerate(self.sites):
            profile = ClimateProfile(
                site_id=site.site_id,
                wind_mean=7.0 + i,
                wind_extreme_50yr=30.0 + i*2,
                temp_mean=12.0 + i,
                humidity_mean=50.0 + i*3,
                free_cooling_hours=6500.0 - i*100,
                flood_risk_score=0.1 + i*0.1,
                terrain_slope_mean=8.0 + i*2,
            )
            analyzer.climate_profiles[site.site_id] = profile
        
        # Generate CSV report
        with tempfile.NamedTemporaryFile(mode='w', suffix='.csv', delete=False) as f:
            csv_file = f.name
        
        try:
            analyzer.generate_report(csv_output=csv_file)
            
            # Verify CSV was created
            self.assertTrue(os.path.exists(csv_file))
            
            # Read and verify CSV structure
            with open(csv_file, 'r') as f:
                reader = csv.DictReader(f)
                rows = list(reader)
            
            # Should have 3 data rows
            self.assertEqual(len(rows), 3)
            
            # Check columns
            self.assertIn("rank", rows[0])
            self.assertIn("site_id", rows[0])
            self.assertIn("overall_score", rows[0])
        finally:
            if os.path.exists(csv_file):
                os.remove(csv_file)
    
    def test_different_priority_rankings(self):
        """Test that different priorities produce different rankings."""
        # Create diverse profiles
        profiles_dict = {
            "site_cool_windy": ClimateProfile(
                site_id="site_cool_windy",
                wind_mean=10.0, wind_extreme_50yr=40.0,  # Windy
                temp_mean=10.0, humidity_mean=40.0,      # Cool
                free_cooling_hours=7500.0,
                flood_risk_score=0.1,
                terrain_slope_mean=5.0,
            ),
            "site_warm_calm": ClimateProfile(
                site_id="site_warm_calm",
                wind_mean=5.0, wind_extreme_50yr=22.0,   # Calm
                temp_mean=18.0, humidity_mean=65.0,      # Warm
                free_cooling_hours=5000.0,
                flood_risk_score=0.2,
                terrain_slope_mean=3.0,
            ),
        }
        
        # Create sites from profiles
        sites_test = [
            CandidateSite("site_cool_windy", 100000, 200000, label="Cool & Windy"),
            CandidateSite("site_warm_calm", 150000, 250000, label="Warm & Calm"),
        ]
        
        # Test both priorities
        rankings = {}
        for priority in [SitingPriority.COOLING_EFFICIENCY, SitingPriority.RESILIENCE]:
            analyzer = SitingAnalyzer(sites_test, priority=priority)
            analyzer.climate_profiles = profiles_dict.copy()
            evaluations = analyzer.evaluate_all_sites()
            rankings[priority.value] = evaluations[0]["site_id"]
        
        # Different priorities should prefer different sites
        # Cooling efficiency should prefer cool, resilience should prefer calm winds
        self.assertEqual(rankings["cooling"], "site_cool_windy")
        self.assertEqual(rankings["resilience"], "site_warm_calm")


class TestDataCenterSitingIntegration(unittest.TestCase):
    """Integration tests for complete siting analysis workflows."""
    
    def test_complete_analysis_workflow(self):
        """Test complete workflow: initialization -> evaluation -> reporting."""
        
        sites = [
            CandidateSite("s1", 100000, 200000, label="Site 1"),
            CandidateSite("s2", 150000, 250000, label="Site 2"),
        ]
        
        analyzer = SitingAnalyzer(sites, priority=SitingPriority.BALANCED)
        
        # Add profiles
        for i, site in enumerate(sites):
            profile = ClimateProfile(
                site_id=site.site_id,
                wind_mean=7.0, wind_extreme_50yr=30.0,
                temp_mean=12.0, humidity_mean=50.0 + i*5,
                free_cooling_hours=6500.0, flood_risk_score=0.1,
                terrain_slope_mean=8.0,
            )
            analyzer.climate_profiles[site.site_id] = profile
        
        # Evaluate
        evaluations = analyzer.evaluate_all_sites()
        self.assertEqual(len(evaluations), 2)
        
        # Both should have scores
        for eval in evaluations:
            self.assertGreater(eval["scores"]["overall_score"], 0.0)
            self.assertLess(eval["scores"]["overall_score"], 1.0)


def run_tests():
    """Run all tests."""
    loader = unittest.TestLoader()
    suite = unittest.TestSuite()
    
    suite.addTests(loader.loadTestsFromTestCase(TestDataCenterSiting))
    suite.addTests(loader.loadTestsFromTestCase(TestDataCenterSitingIntegration))
    
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    sys.exit(run_tests())
