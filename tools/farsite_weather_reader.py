#!/usr/bin/env python3
"""
farsite_weather_reader.py  -  Parse FARSITE .wtr weather-station files and
write wind CSV files for the mass-consistent wind solver.

A FARSITE weather (.wtr) file contains one record per observation:

    YEAR  MONTH  DAY  TIME  TEMP  RH  PRECIP  WIND_SPEED  WIND_DIR  CLOUD

Fields
------
YEAR        : 4-digit year
MONTH       : 1-12
DAY         : 1-31
TIME        : HHMM (0000-2350; 2400 = midnight of next day)
TEMP        : temperature [°F]  - or °C when --metric is given
RH          : relative humidity [%]
PRECIP      : precipitation amount [in or mm]
WIND_SPEED  : 20-ft wind speed [miles/hr]  - or [m/s] with --metric
WIND_DIR    : wind direction [degrees from north, meteorological convention]
CLOUD       : cloud cover [%]  (optional column; ignored if absent)

Comments (lines starting with #) are ignored.
Whitespace-delimited; extra columns after CLOUD are silently discarded.

Output
------
For each observation the tool writes (or appends) one CSV wind file suitable
for use as ``velocity_file`` in the wind solver.  Each CSV has columns:

    X  Y  Z  Ux  Uy

where X/Y/Z are spatial coordinates [m] on a regular grid and Ux/Uy are the
horizontal wind components [m/s].  The grid is defined by --domain and
--resolution, and the wind speed is uniform over the domain.

Usage examples
--------------
  # Write one wind CSV for the first record
  python3 farsite_weather_reader.py --wtr fire.wtr --wind wind.csv

  # Write time-dependent wind files for a 2000 * 2000 m domain at 100 m resolution
  python3 farsite_weather_reader.py \\
      --wtr fire.wtr \\
      --wind wind.csv \\
      --time-dependent \\
      --domain 2000 2000 \\
      --resolution 100

  # Metric-unit .wtr file (°C, m/s)
  python3 farsite_weather_reader.py --wtr fire.wtr --wind wind.csv --metric

Options
-------
  --wtr FILE           Input FARSITE .wtr file (required).
  --wind FILE          Base name for output wind CSV file (default: wind.csv).
  --time-dependent     Write numbered wind snapshots (wind.csv, wind_1.csv, …).
  --time-index N       Write only the N-th record (0-based; default: 0).
  --start DATETIME     Earliest record to include (ISO-8601 or free form).
  --end DATETIME       Latest record to include.
  --domain W H         Domain width and height in metres (default: 1000 1000).
  --resolution R       Grid spacing in metres (default: 50).
  --metric             Input data are in SI units (°C, m/s) rather than US
                       customary (°F, mph).

References
----------
  Finney, M.A. (2004). FARSITE: Fire Area Simulator - Model Development and
    Evaluation. USDA Forest Service Research Paper RMRS-RP-4.
"""

from __future__ import annotations

import argparse
import math
import os
import sys
from datetime import datetime, timedelta
from typing import List, Optional, Tuple


# ---------------------------------------------------------------------------
# Data structures
# ---------------------------------------------------------------------------

class WeatherRecord:
    """One parsed FARSITE weather observation."""
    __slots__ = ("timestamp", "temp_c", "rh_pct", "precip_mm",
                 "wind_speed_ms", "wind_dir_deg", "cloud_pct")

    def __init__(self, timestamp: datetime, temp_c: float, rh_pct: float,
                 precip_mm: float, wind_speed_ms: float, wind_dir_deg: float,
                 cloud_pct: float = 0.0):
        self.timestamp      = timestamp
        self.temp_c         = temp_c
        self.rh_pct         = rh_pct
        self.precip_mm      = precip_mm
        self.wind_speed_ms  = wind_speed_ms
        self.wind_dir_deg   = wind_dir_deg
        self.cloud_pct      = cloud_pct


# ---------------------------------------------------------------------------
# Parsing
# ---------------------------------------------------------------------------

def _parse_time(time_field: str) -> Tuple[int, int]:
    """Return (hour, minute) from a HHMM string (handles 2400 → 00:00)."""
    t = int(time_field)
    if t == 2400:
        return 0, 0
    return t // 100, t % 100


def parse_wtr(path: str, metric: bool = False) -> List[WeatherRecord]:
    """
    Parse a FARSITE .wtr file and return a list of WeatherRecord objects.

    Parameters
    ----------
    path   : Path to the .wtr file.
    metric : When True the input units are SI (°C, m/s); otherwise US
             customary (°F, mph at 20 ft).
    """
    records: List[WeatherRecord] = []
    extra_day = False  # whether last TIME field was 2400 (midnight next day)

    with open(path, "r") as fh:
        for raw in fh:
            line = raw.strip()
            if not line or line.startswith("#") or line.startswith("!"):
                continue
            parts = line.split()
            if len(parts) < 9:
                continue  # skip malformed lines

            year, month, day = int(parts[0]), int(parts[1]), int(parts[2])
            hour, minute = _parse_time(parts[3])
            time_overflow = (int(parts[3]) == 2400)
            dt = datetime(year, month, day, hour, minute)
            if time_overflow:
                dt += timedelta(days=1)

            temp_raw   = float(parts[4])
            rh_pct     = float(parts[5])
            precip_raw = float(parts[6])
            speed_raw  = float(parts[7])
            wind_dir   = float(parts[8])
            cloud_pct  = float(parts[9]) if len(parts) > 9 else 0.0

            # Unit conversion if not already metric
            if metric:
                temp_c        = temp_raw
                precip_mm     = precip_raw
                wind_speed_ms = speed_raw
            else:
                temp_c        = (temp_raw - 32.0) / 1.8         # °F → °C
                precip_mm     = precip_raw * 25.4                # in → mm
                wind_speed_ms = speed_raw * 0.44704             # mph → m/s

            records.append(WeatherRecord(
                timestamp     = dt,
                temp_c        = temp_c,
                rh_pct        = min(100.0, max(0.0, rh_pct)),
                precip_mm     = max(0.0, precip_mm),
                wind_speed_ms = max(0.0, wind_speed_ms),
                wind_dir_deg  = wind_dir % 360.0,
                cloud_pct     = min(100.0, max(0.0, cloud_pct)),
            ))

    return records


# ---------------------------------------------------------------------------
# Wind vector computation
# ---------------------------------------------------------------------------

def wind_components(speed_ms: float, dir_deg: float) -> Tuple[float, float]:
    """
    Convert meteorological wind speed + direction to (Ux, Uy) components.

    Meteorological convention: direction is where the wind *comes from*,
    measured clockwise from north.  The solver uses East=+x, North=+y.

    Parameters
    ----------
    speed_ms : Wind speed [m/s].
    dir_deg  : Meteorological wind direction [degrees from north].

    Returns
    -------
    (Ux, Uy) : Wind vector [m/s] in (East, North) frame.
    """
    # Convert meteorological direction to math angle (CCW from East)
    math_angle = math.radians(270.0 - dir_deg)
    ux = speed_ms * math.cos(math_angle)
    uy = speed_ms * math.sin(math_angle)
    return ux, uy


# ---------------------------------------------------------------------------
# CSV grid writing
# ---------------------------------------------------------------------------

def write_wind_csv(path: str, ux: float, uy: float,
                   domain_w: float, domain_h: float,
                   resolution: float) -> None:
    """
    Write a uniform wind field CSV file for the solver.

    The file contains five columns: X Y Z Ux Uy on a regular grid.

    Parameters
    ----------
    path       : Output file path.
    ux, uy     : Uniform wind components [m/s].
    domain_w   : Domain width  [m].
    domain_h   : Domain height [m].
    resolution : Grid spacing  [m].
    """
    nx = max(2, int(round(domain_w / resolution)) + 1)
    ny = max(2, int(round(domain_h / resolution)) + 1)
    xs = [i * resolution for i in range(nx)]
    ys = [j * resolution for j in range(ny)]

    with open(path, "w") as fh:
        fh.write("# X Y Z Ux Uy  (generated by farsite_weather_reader.py)\n")
        for y in ys:
            for x in xs:
                fh.write(f"{x:.2f} {y:.2f} 0.00 {ux:.4f} {uy:.4f}\n")


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def _output_name(base: str, idx: int) -> str:
    """Return wind_N.csv (or wind.csv for idx=0) from base name wind.csv."""
    root, ext = os.path.splitext(base)
    return base if idx == 0 else f"{root}_{idx}{ext}"


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Parse a FARSITE .wtr weather file and write wind CSV files.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("--wtr",           required=True, metavar="FILE",
                        help="Input FARSITE .wtr file")
    parser.add_argument("--wind",          default="wind.csv", metavar="FILE",
                        help="Base output wind CSV file (default: wind.csv)")
    parser.add_argument("--time-dependent", action="store_true",
                        help="Write numbered time-dependent wind snapshots")
    parser.add_argument("--time-index",    type=int, default=0, metavar="N",
                        help="Index of single record to write (default: 0)")
    parser.add_argument("--start",         default=None, metavar="DATETIME",
                        help="Earliest record timestamp to include")
    parser.add_argument("--end",           default=None, metavar="DATETIME",
                        help="Latest record timestamp to include")
    parser.add_argument("--domain",        type=float, nargs=2, default=[1000.0, 1000.0],
                        metavar=("W", "H"),
                        help="Domain width and height [m] (default: 1000 1000)")
    parser.add_argument("--resolution",    type=float, default=50.0, metavar="R",
                        help="Grid spacing [m] (default: 50)")
    parser.add_argument("--metric",        action="store_true",
                        help="Input data are in SI units (°C, m/s)")

    args = parser.parse_args(argv)

    # Parse weather file
    records = parse_wtr(args.wtr, metric=args.metric)
    if not records:
        print("ERROR: no records parsed from", args.wtr, file=sys.stderr)
        sys.exit(1)
    print(f"Parsed {len(records)} records from {args.wtr}")

    # Filter by date range
    t_start = datetime.fromisoformat(args.start) if args.start else None
    t_end   = datetime.fromisoformat(args.end)   if args.end   else None
    if t_start or t_end:
        records = [r for r in records
                   if (t_start is None or r.timestamp >= t_start) and
                      (t_end   is None or r.timestamp <= t_end)]
        print(f"After date filter: {len(records)} records")

    if not records:
        print("ERROR: no records remain after date filtering", file=sys.stderr)
        sys.exit(1)

    domain_w, domain_h = args.domain

    # Write wind files
    if args.time_dependent:
        for idx, rec in enumerate(records):
            ux, uy = wind_components(rec.wind_speed_ms, rec.wind_dir_deg)
            out = _output_name(args.wind, idx)
            write_wind_csv(out, ux, uy, domain_w, domain_h, args.resolution)
            print(f"  {rec.timestamp}  speed={rec.wind_speed_ms:.2f} m/s  dir={rec.wind_dir_deg:.0f}°"
                  f"  → Ux={ux:.3f} Uy={uy:.3f}  → {out}")
        print(f"Written {len(records)} wind files.")
    else:
        idx = min(args.time_index, len(records) - 1)
        rec = records[idx]
        ux, uy = wind_components(rec.wind_speed_ms, rec.wind_dir_deg)
        write_wind_csv(args.wind, ux, uy, domain_w, domain_h, args.resolution)
        print(f"Record {idx}: {rec.timestamp}  speed={rec.wind_speed_ms:.2f} m/s  "
              f"dir={rec.wind_dir_deg:.0f}°  → Ux={ux:.3f} Uy={uy:.3f}  → {args.wind}")


if __name__ == "__main__":
    main()
