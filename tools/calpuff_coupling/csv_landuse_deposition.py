#!/usr/bin/env python3
"""
csv_landuse_deposition.py

Part of the CALPUFF model enhancement suite.
Calculates deposition velocity profiles via the resistance-based formulation:
Vd = Vs + 1 / (r_a + r_b + r_s)
Supports land-use specific parameters and seasonal variations.
"""

import os
import csv
import numpy as np


class CSVLanduseDeposition:
    def __init__(self, landuse_file=None):
        self.landuse_table = {}
        if landuse_file:
            self.load_landuse_table(landuse_file)

    def load_landuse_table(self, filepath):
        """
        Loads landuse-specific deposition parameters.
        Format: lu_id,description,r_a,r_b,r_s_summer,r_s_winter,canopy_height
        """
        if not os.path.exists(filepath):
            raise FileNotFoundError(f"Land-use file not found: {filepath}")

        with open(filepath, 'r') as f:
            reader = csv.DictReader(f)
            for row in reader:
                row = {k.strip(): v.strip() for k, v in row.items()}
                lu_id = int(row['lu_id'])
                self.landuse_table[lu_id] = {
                    'description': row['description'],
                    'r_a': float(row['r_a']),
                    'r_b': float(row['r_b']),
                    'r_s_summer': float(row['r_s_summer']),
                    'r_s_winter': float(row['r_s_winter']),
                    'canopy_height': float(row['canopy_height']) if row.get('canopy_height') else 0.0
                }

    def compute_deposition_velocity(self, lu_id, season='summer', settling_velocity=0.0):
        """
        Calculates the deposition velocity (V_d) [m/s] using the resistance method:
        V_d = V_s + 1 / (r_a + r_b + r_s)
        """
        if lu_id not in self.landuse_table:
            raise KeyError(f"Land-use ID {lu_id} not found in loaded database.")

        lu = self.landuse_table[lu_id]
        r_a = lu['r_a']
        r_b = lu['r_b']
        
        if season.lower() == 'winter':
            r_s = lu['r_s_winter']
        else:
            r_s = lu['r_s_summer']

        total_resistance = r_a + r_b + r_s
        
        if total_resistance <= 0.0:
            return settling_velocity

        v_dep = settling_velocity + (1.0 / total_resistance)
        return float(v_dep)

    def generate_deposition_map_csv(self, species_list, output_path, season='summer'):
        """
        Generates a summary CSV map of computed deposition velocities across all land-uses.
        """
        fieldnames = ['lu_id', 'description', 'canopy_height'] + [f'Vd_{sp}' for sp in species_list]
        with open(output_path, 'w', newline='') as f:
            writer = csv.DictWriter(f, fieldnames=fieldnames)
            writer.writeheader()
            for lu_id, info in self.landuse_table.items():
                row = {
                    'lu_id': lu_id,
                    'description': info['description'],
                    'canopy_height': info['canopy_height']
                }
                for sp in species_list:
                    # Optional species-specific settling velocity differences can be added here
                    row[f'Vd_{sp}'] = self.compute_deposition_velocity(lu_id, season=season)
                writer.writerow(row)


if __name__ == "__main__":
    loader = CSVLanduseDeposition()
    print("CSVLanduseDeposition class defined successfully.")
