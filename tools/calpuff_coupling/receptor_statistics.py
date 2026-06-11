#!/usr/bin/env python3
"""
receptor_statistics.py

Part of the CALPUFF model enhancement suite.
Calculates statistical indices, percentiles, averages, and regulatory standard exceedance
from puff solver receptor concentration outputs.
"""

import os
import csv
import numpy as np


class ReceptorStatistics:
    def __init__(self, receptor_output_file=None):
        self.headers = []
        self.data = {}
        if receptor_output_file:
            self.load_receptor_data(receptor_output_file)

    def load_receptor_data(self, filepath):
        """
        Loads receptor concentrations over time from a CSV file.
        Expected format: step,time,receptor_name,C_total,species1_conc,species2_conc...
        """
        if not os.path.exists(filepath):
            raise FileNotFoundError(f"Receptor file not found: {filepath}")

        with open(filepath, 'r') as f:
            reader = csv.reader(f)
            # Find the header row
            self.headers = next(reader)
            # Remove whitespace
            self.headers = [h.strip() for h in self.headers]

            # Populate data dict per receptor
            for row in reader:
                if not row:
                    continue
                step = int(row[0])
                time = float(row[1])
                rec_name = row[2].strip()

                if rec_name not in self.data:
                    self.data[rec_name] = []

                entry = {'step': step, 'time': time}
                for i in range(3, len(self.headers)):
                    col_name = self.headers[i]
                    entry[col_name] = float(row[i])
                self.data[rec_name].append(entry)

    def compute_percentiles(self, receptor_name, species, percentiles=[50, 90, 95, 99]):
        """
        Computes requested percentiles for a specific species at a specific receptor.
        """
        if receptor_name not in self.data:
            raise ValueError(f"Receptor '{receptor_name}' not found.")

        series = [entry[species] for entry in self.data[receptor_name] if species in entry]
        if not series:
            return {}

        arr = np.array(series)
        results = {}
        for p in percentiles:
            results[p] = float(np.percentile(arr, p))
        return results

    def compute_average(self, receptor_name, species):
        """
        Computes the arithmetic mean concentration.
        """
        if receptor_name not in self.data:
            raise ValueError(f"Receptor '{receptor_name}' not found.")

        series = [entry[species] for entry in self.data[receptor_name] if species in entry]
        if not series:
            return 0.0
        return float(np.mean(series))

    def detect_exceedances(self, receptor_name, species, threshold):
        """
        Finds time periods where concentration exceeds a given regulatory threshold.
        Returns a list of matching entries (timestamp and value).
        """
        if receptor_name not in self.data:
            raise ValueError(f"Receptor '{receptor_name}' not found.")

        exceedances = []
        for entry in self.data[receptor_name]:
            if species in entry and entry[species] > threshold:
                exceedances.append({
                    'time': entry['time'],
                    'value': entry[species]
                })
        return exceedances

    def verify_naaqs_compliance(self, receptor_name, species, standard_limit):
        """
        Verifies compliance against a NAAQS standard (compares average concentration
        and extreme percentiles to standard_limit).
        """
        avg = self.compute_average(receptor_name, species)
        p99 = self.compute_percentiles(receptor_name, species, percentiles=[99])[99]

        compliance = {
            'average': avg,
            'p99': p99,
            'limit': standard_limit,
            'compliant_on_average': avg < standard_limit,
            'compliant_on_peak': p99 < standard_limit
        }
        return compliance


if __name__ == "__main__":
    stats = ReceptorStatistics()
    print("ReceptorStatistics class defined successfully.")
