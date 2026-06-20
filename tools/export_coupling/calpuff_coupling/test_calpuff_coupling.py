#!/usr/bin/env python3
"""
test_calpuff_coupling.py

Unit test suite to validate the functionality of the new CALPUFF coupling toolset.
Tests chemistry loading, multi-source loading, resistance-based deposition, meteorology adaptation,
and receptor analytics.
"""

import os
import unittest
import numpy as np

from csv_chemistry_loader import CSVChemistryLoader
from csv_source_loader import CSVSourceLoader
from csv_landuse_deposition import CSVLanduseDeposition
from csv_meteorology_adapter import CSVMeteorologyAdapter
from receptor_statistics import ReceptorStatistics
from diagnostic_validator import DiagnosticValidator


class TestCALPUFFCoupling(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        # Paths to example CSV files
        cls.script_dir = os.path.dirname(os.path.abspath(__file__))
        cls.examples_dir = os.path.join(cls.script_dir, "examples")
        cls.species_file = os.path.join(cls.examples_dir, "species.csv")
        cls.reactions_file = os.path.join(cls.examples_dir, "reactions.csv")
        cls.sources_file = os.path.join(cls.examples_dir, "sources.csv")
        cls.landuse_file = os.path.join(cls.examples_dir, "landuse_deposition.csv")
        cls.met_file = os.path.join(cls.examples_dir, "meteorology.csv")

    def test_chemistry_loader(self):
        loader = CSVChemistryLoader(self.species_file, self.reactions_file)
        self.assertEqual(len(loader.species), 5)
        self.assertEqual(len(loader.reactions), 3)

        # Test species info
        self.assertEqual(loader.species[1]['name'], 'SO2')
        self.assertEqual(loader.species[1]['molar_mass'], 64.0)

        # Test rates calculation
        concs = {1: 10.0, 2: 0.0, 3: 5.0, 4: 0.0, 5: 0.0}
        rates = loader.compute_reaction_rates(concs, temp_k=298.15)
        # SO2 reactant in reaction 1 (rate_const = 0.001) -> rate = 0.001 * [SO2] = 0.01
        self.assertLess(rates[1], 0.0)
        self.assertGreater(rates[2], 0.0)

        # Integration check
        final_concs = loader.integrate_reactions(concs, temp_k=298.15, dt=10.0, steps=10)
        self.assertLess(final_concs[1], concs[1])
        self.assertGreater(final_concs[2], concs[2])

        # Generate C++ header snippet
        header_path = os.path.join(self.script_dir, "auto_chemistry.H")
        loader.generate_cpp_header(header_path)
        self.assertTrue(os.path.exists(header_path))
        os.remove(header_path)

    def test_sources_loader(self):
        loader = CSVSourceLoader(self.sources_file)
        self.assertEqual(len(loader.sources), 3)
        self.assertEqual(loader.sources[0]['type'], 'point')
        self.assertEqual(loader.sources[1]['type'], 'line')
        self.assertEqual(loader.sources[2]['type'], 'area')

        # Test validation bounds
        invalid_src = {
            'source_id': 99, 'type': 'invalid_type', 'x': 0.0, 'y': 0.0, 'z': 0.0,
            'emission_rate': 1.0, 'sigma_y0': 1.0, 'sigma_z0': 1.0
        }
        with self.assertRaises(ValueError):
            loader.validate_source(invalid_src)

        # Export to ParmParse
        parm_path = os.path.join(self.script_dir, "sources_snippet.i")
        loader.export_to_parmparse(parm_path)
        self.assertTrue(os.path.exists(parm_path))
        os.remove(parm_path)

    def test_landuse_deposition(self):
        loader = CSVLanduseDeposition(self.landuse_file)
        self.assertEqual(len(loader.landuse_table), 3)
        self.assertEqual(loader.landuse_table[1]['description'], 'water')

        # Vd computation: water (r_a = 10, r_b = 20, r_s = 1000) -> total resistance = 1030
        # Vd = 1 / 1030 = ~0.00097 m/s
        vd_water_summer = loader.compute_deposition_velocity(1, season='summer')
        self.assertAlmostEqual(vd_water_summer, 1.0 / 1030.0, places=5)

        # Winter season check
        vd_grass_winter = loader.compute_deposition_velocity(2, season='winter')
        self.assertAlmostEqual(vd_grass_winter, 1.0 / (50.0 + 80.0 + 400.0), places=5)

    def test_meteorology_adapter(self):
        adapter = CSVMeteorologyAdapter(self.met_file)
        self.assertEqual(len(adapter.time_series), 3)

        # Interpolation test at mid-point (1800.0s)
        met_1800 = adapter.interpolate_met_at_time(1800.0)
        self.assertEqual(met_1800['time'], 1800.0)
        # Temp should be average of 293.15 and 295.15 -> 294.15
        self.assertAlmostEqual(met_1800['temp'], 294.15, places=2)

        # Stability class lookup check
        stability = adapter.estimate_stability_class(wind_speed=1.5, solar_radiation=800.0)
        self.assertEqual(stability, 0) # Class A

    def test_receptor_statistics_and_compliance(self):
        # Create a dummy receptor output CSV file
        dummy_rec_path = os.path.join(self.script_dir, "dummy_receptor_output.csv")
        with open(dummy_rec_path, 'w') as f:
            f.write("step, time, receptor_name, C_total, SO2, Sulfate\n")
            f.write("0, 0.0, receptor_1, 10.0, 8.0, 2.0\n")
            f.write("1, 10.0, receptor_1, 15.0, 12.0, 3.0\n")
            f.write("2, 20.0, receptor_1, 5.0, 4.0, 1.0\n")

        stats = ReceptorStatistics(dummy_rec_path)
        self.assertIn('receptor_1', stats.data)

        # Test percentile calculation
        percentiles = stats.compute_percentiles('receptor_1', 'SO2', percentiles=[50, 90])
        self.assertEqual(percentiles[50], 8.0)
        self.assertGreater(percentiles[90], 8.0)

        # Test average calculation
        avg = stats.compute_average('receptor_1', 'SO2')
        self.assertAlmostEqual(avg, 8.0, places=2)

        # Test NAAQS compliance check
        compliance = stats.verify_naaqs_compliance('receptor_1', 'SO2', standard_limit=10.0)
        self.assertTrue(compliance['compliant_on_average'])
        self.assertFalse(compliance['compliant_on_peak']) # p99 is near 12.0 > 10.0

        os.remove(dummy_rec_path)

    def test_diagnostic_validator(self):
        emissions = [
            {'time': 0.0, 'rate': 1.0},
            {'time': 10.0, 'rate': 1.0}
        ]
        puffs = [{'mass': 4.0}, {'mass': 3.0}]
        # Total emitted mass over 10s at 1.0 units/s = 10.0
        # Airborne = 7.0, Deposited = 2.5 -> tracked = 9.5
        metrics = DiagnosticValidator.compute_mass_balance(emissions, puffs, deposited_mass=2.5)
        self.assertAlmostEqual(metrics['total_emitted'], 10.0)
        self.assertAlmostEqual(metrics['closure_fraction'], 0.95)

        # Test validation metrics against observations
        predictions = [1.2, 2.5, 3.1, 4.0]
        observations = [1.0, 2.4, 3.0, 4.5]
        stats = DiagnosticValidator.calculate_validation_metrics(predictions, observations)
        self.assertGreater(stats['correlation'], 0.9)
        self.assertLess(stats['mean_bias'], 0.5)


if __name__ == "__main__":
    unittest.main()
