#!/usr/bin/env python3
"""
csv_source_loader.py

Part of the CALPUFF model enhancement suite.
Parses multi-source CSV files, validates parameters, and generates unified source descriptions
for point, line, area, and volume sources compatible with massconsistent_amr's PUFF solver.
"""

import os
import csv


class CSVSourceLoader:
    def __init__(self, sources_file=None):
        self.sources = []
        if sources_file:
            self.load_sources(sources_file)

    def load_sources(self, filepath):
        """
        Loads multiple source specifications from a CSV file.
        Format: source_id,type,x,y,z,emission_rate,duration,sigma_y0,sigma_z0,heat_flux,width,length
        Types supported: point, line, area, volume
        """
        if not os.path.exists(filepath):
            raise FileNotFoundError(f"Sources file not found: {filepath}")

        with open(filepath, 'r') as f:
            reader = csv.DictReader(f)
            for row in reader:
                row = {k.strip(): v.strip() for k, v in row.items()}
                src = {
                    'source_id': int(row['source_id']),
                    'type': row['type'].lower(),
                    'x': float(row['x']),
                    'y': float(row['y']),
                    'z': float(row['z']),
                    'emission_rate': float(row['emission_rate']),
                    'duration': float(row['duration']) if row.get('duration') else 100.0,
                    'sigma_y0': float(row['sigma_y0']) if row.get('sigma_y0') else 1.0,
                    'sigma_z0': float(row['sigma_z0']) if row.get('sigma_z0') else 1.0,
                    'heat_flux': float(row['heat_flux']) if row.get('heat_flux') else 0.0,
                    'width': float(row['width']) if row.get('width') else 0.0,
                    'length': float(row['length']) if row.get('length') else 0.0
                }
                self.validate_source(src)
                self.sources.append(src)

    def validate_source(self, src):
        """
        Validates source parameters. Raises ValueError if invalid.
        """
        valid_types = {'point', 'line', 'area', 'volume'}
        if src['type'] not in valid_types:
            raise ValueError(f"Unknown source type: {src['type']}. Must be one of {valid_types}")
        if src['emission_rate'] < 0.0:
            raise ValueError(f"Emission rate must be non-negative: {src['emission_rate']}")
        if src['sigma_y0'] <= 0.0 or src['sigma_z0'] <= 0.0:
            raise ValueError("Initial spreads (sigma_y0, sigma_z0) must be positive values.")

    def export_to_parmparse(self, output_path):
        """
        Exports the loaded sources to an AMReX ParmParse input format file snippet.
        """
        with open(output_path, 'w') as f:
            f.write("# Auto-generated source list snippet for puff_solver\n")
            f.write(f"num_sources = {len(self.sources)}\n\n")
            for i, src in enumerate(self.sources):
                f.write(f"# Source {src['source_id']} ({src['type']})\n")
                prefix = f"source_{i}"
                f.write(f"{prefix}.type = {src['type']}\n")
                f.write(f"{prefix}.x = {src['x']}\n")
                f.write(f"{prefix}.y = {src['y']}\n")
                f.write(f"{prefix}.z = {src['z']}\n")
                f.write(f"{prefix}.emission_rate = {src['emission_rate']}\n")
                f.write(f"{prefix}.duration = {src['duration']}\n")
                f.write(f"{prefix}.sigma_y0 = {src['sigma_y0']}\n")
                f.write(f"{prefix}.sigma_z0 = {src['sigma_z0']}\n")
                f.write(f"{prefix}.heat_flux = {src['heat_flux']}\n")
                if src['type'] in ('area', 'volume'):
                    f.write(f"{prefix}.width = {src['width']}\n")
                    f.write(f"{prefix}.length = {src['length']}\n")
                f.write("\n")


if __name__ == "__main__":
    loader = CSVSourceLoader()
    print("CSVSourceLoader class defined successfully.")
