#!/usr/bin/env python3
"""
csv_meteorology_adapter.py

Part of the CALPUFF model enhancement suite.
Parses meteorological time-series inputs from CSV, performs interpolation, and validates
atmospheric physical states (stability class, boundary layer heights, mixing ratios).
"""

import os
import csv
import numpy as np


class CSVMeteorologyAdapter:
    def __init__(self, met_file=None):
        self.time_series = []
        if met_file:
            self.load_meteorology(met_file)

    def load_meteorology(self, filepath):
        """
        Loads meteorological time series from CSV.
        Expected format: time,u_wind,v_wind,w_wind,temp,rh,z_i,pressure,solar_rad,precip_rate
        """
        if not os.path.exists(filepath):
            raise FileNotFoundError(f"Meteorology file not found: {filepath}")

        with open(filepath, 'r') as f:
            reader = csv.DictReader(f)
            for row in reader:
                row = {k.strip(): v.strip() for k, v in row.items()}
                entry = {
                    'time': float(row['time']),
                    'u_wind': float(row['u_wind']),
                    'v_wind': float(row['v_wind']),
                    'w_wind': float(row['w_wind']),
                    'temp': float(row['temp']),
                    'rh': float(row['rh']),
                    'z_i': float(row['z_i']),
                    'pressure': float(row['pressure']),
                    'solar_rad': float(row['solar_rad']),
                    'precip_rate': float(row['precip_rate'])
                }
                self.validate_met_entry(entry)
                self.time_series.append(entry)

        # Sort by time
        self.time_series.sort(key=lambda x: x['time'])

    def validate_met_entry(self, entry):
        """
        Validates the physical bounds of meteorological values.
        """
        if entry['rh'] < 0.0 or entry['rh'] > 100.0:
            raise ValueError(f"Relative humidity out of bounds: {entry['rh']}%")
        if entry['z_i'] < 0.0:
            raise ValueError(f"Boundary layer height z_i cannot be negative: {entry['z_i']}")
        if entry['pressure'] < 0.0:
            raise ValueError(f"Atmospheric pressure cannot be negative: {entry['pressure']}")
        if entry['precip_rate'] < 0.0:
            raise ValueError(f"Precipitation rate cannot be negative: {entry['precip_rate']}")

    def interpolate_met_at_time(self, query_time):
        """
        Interpolates meteorological state at any query timestamp.
        Uses linear interpolation between the two nearest records.
        """
        if not self.time_series:
            raise ValueError("No meteorological data loaded.")

        times = np.array([e['time'] for e in self.time_series])

        if query_time <= times[0]:
            return self.time_series[0]
        if query_time >= times[-1]:
            return self.time_series[-1]

        # Find interval
        idx = np.searchsorted(times, query_time) - 1
        t0, t1 = times[idx], times[idx + 1]
        w1 = (query_time - t0) / (t1 - t0)
        w0 = 1.0 - w1

        e0 = self.time_series[idx]
        e1 = self.time_series[idx + 1]

        interpolated = {}
        for key in e0.keys():
            interpolated[key] = float(w0 * e0[key] + w1 * e1[key])
        return interpolated

    def estimate_stability_class(self, wind_speed, solar_radiation):
        """
        Estimates Pasquill-Gifford stability class (A-F represented as 0-5)
        based on wind speed and insolation.
        """
        # A simple daytime Pasquill-Gifford table lookup
        if solar_radiation > 700.0:  # Strong solar radiation
            if wind_speed < 2.0: return 0  # A
            elif wind_speed < 3.0: return 0  # A-B -> A
            elif wind_speed < 5.0: return 1  # B
            else: return 2  # C
        elif solar_radiation > 350.0:  # Moderate solar radiation
            if wind_speed < 2.0: return 0  # A-B -> A
            elif wind_speed < 3.0: return 1  # B
            elif wind_speed < 5.0: return 2  # C
            else: return 3  # D
        else:  # Slight solar radiation
            if wind_speed < 2.0: return 1  # B
            elif wind_speed < 3.0: return 2  # C
            elif wind_speed < 5.0: return 3  # D
            else: return 3  # D


if __name__ == "__main__":
    loader = CSVMeteorologyAdapter()
    print("CSVMeteorologyAdapter class defined successfully.")
