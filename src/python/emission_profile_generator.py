#!/usr/bin/env python3
"""
Emission Profile Generator
===========================

Generate time-varying emission profiles for atmospheric dispersion models.
Supports daily cycles, seasonal patterns, episodic events, and custom functions.

Features:
- Pre-built emission profile templates
- Daily cycle generation (traffic, industrial, residential patterns)
- Seasonal modulation
- Episodic event modeling
- Custom function support
- CSV export compatible with puff model
- Visualization of generated profiles

Usage:
    python emission_profile_generator.py --profile traffic --duration 86400 --output emissions.csv
    python emission_profile_generator.py --template industrial --duration 604800 --output weekly.csv
    python emission_profile_generator.py --custom "lambda t: 1.0 + 0.5*sin(2*pi*t/3600)" --duration 3600
"""

import sys
import argparse
import csv
import math
from pathlib import Path
from typing import Callable, List, Tuple, Optional
from dataclasses import dataclass
from enum import Enum


class ProfileType(Enum):
    """Available emission profile templates."""
    CONSTANT = "constant"           # Steady emission
    TRAFFIC = "traffic"             # Rush hour pattern
    INDUSTRIAL = "industrial"       # 24-hour industrial
    RESIDENTIAL = "residential"     # Residential heating/cooking
    EPISODIC = "episodic"          # Accidental/transient release
    DAILY = "daily"                 # Generic daily pattern
    WEEKLY = "weekly"               # Weekly variation
    SEASONAL = "seasonal"           # Seasonal variation


@dataclass
class EmissionPoint:
    """Single emission rate at a time point."""
    time: float                      # [s]
    rate: float                      # [units/s]
    
    def to_csv_row(self) -> List[str]:
        """Convert to CSV row format."""
        return [f"{self.time:.1f}", f"{self.rate:.6e}"]


def generate_constant_profile(
    base_rate: float,
    duration: float,
    timestep: float = 60.0
) -> List[EmissionPoint]:
    """
    Generate constant emission profile.
    
    Parameters
    ----------
    base_rate : float
        Constant emission rate [units/s]
    duration : float
        Profile duration [s]
    timestep : float
        Time step for output [s]
    
    Returns
    -------
    List[EmissionPoint]
        Emission points
    """
    profile = []
    t = 0.0
    
    while t <= duration:
        profile.append(EmissionPoint(time=t, rate=base_rate))
        t += timestep
    
    return profile


def generate_traffic_profile(
    base_rate: float,
    duration: float = 86400.0,
    timestep: float = 300.0,
    peak_factor: float = 2.0,
    morning_rush: Tuple[float, float] = (6.0*3600, 10.0*3600),
    evening_rush: Tuple[float, float] = (16.0*3600, 20.0*3600)
) -> List[EmissionPoint]:
    """
    Generate traffic emission profile with rush hour peaks.
    
    Parameters
    ----------
    base_rate : float
        Off-peak emission rate [units/s]
    duration : float
        Profile duration [s] (default: 24 hours)
    timestep : float
        Time step for output [s]
    peak_factor : float
        Multiplier for rush hour rates
    morning_rush : Tuple[float, float]
        Morning rush hour window [start, end] in seconds
    evening_rush : Tuple[float, float]
        Evening rush hour window [start, end] in seconds
    
    Returns
    -------
    List[EmissionPoint]
        Emission points
    """
    profile = []
    t = 0.0
    
    while t <= duration:
        # Determine if in rush hour window
        time_of_day = t % 86400.0  # Wrap to 24-hour cycle
        
        if morning_rush[0] <= time_of_day <= morning_rush[1]:
            # Morning rush with triangular rise/fall
            frac = (time_of_day - morning_rush[0]) / (morning_rush[1] - morning_rush[0])
            if frac < 0.5:
                rate = base_rate * (1.0 + (peak_factor - 1.0) * 2.0 * frac)
            else:
                rate = base_rate * (1.0 + (peak_factor - 1.0) * 2.0 * (1.0 - frac))
        elif evening_rush[0] <= time_of_day <= evening_rush[1]:
            # Evening rush with triangular rise/fall
            frac = (time_of_day - evening_rush[0]) / (evening_rush[1] - evening_rush[0])
            if frac < 0.5:
                rate = base_rate * (1.0 + (peak_factor - 1.0) * 2.0 * frac)
            else:
                rate = base_rate * (1.0 + (peak_factor - 1.0) * 2.0 * (1.0 - frac))
        else:
            # Off-peak
            rate = base_rate
        
        profile.append(EmissionPoint(time=t, rate=rate))
        t += timestep
    
    return profile


def generate_industrial_profile(
    base_rate: float,
    duration: float = 86400.0,
    timestep: float = 300.0,
    shift1: Tuple[float, float] = (6.0*3600, 14.0*3600),
    shift2: Tuple[float, float] = (14.0*3600, 22.0*3600),
    shift1_factor: float = 1.2,
    shift2_factor: float = 0.8
) -> List[EmissionPoint]:
    """
    Generate industrial 24-hour profile with shift variations.
    
    Parameters
    ----------
    base_rate : float
        Base emission rate [units/s]
    duration : float
        Profile duration [s] (default: 24 hours)
    timestep : float
        Time step for output [s]
    shift1 : Tuple[float, float]
        Day shift window [start, end]
    shift2 : Tuple[float, float]
        Evening shift window [start, end]
    shift1_factor : float
        Multiplier for day shift (typically higher)
    shift2_factor : float
        Multiplier for evening shift (typically lower)
    
    Returns
    -------
    List[EmissionPoint]
        Emission points
    """
    profile = []
    t = 0.0
    
    while t <= duration:
        time_of_day = t % 86400.0
        
        if shift1[0] <= time_of_day <= shift1[1]:
            rate = base_rate * shift1_factor
        elif shift2[0] <= time_of_day <= shift2[1]:
            rate = base_rate * shift2_factor
        else:
            # Night shift (minimal or off)
            rate = base_rate * 0.1
        
        profile.append(EmissionPoint(time=t, rate=rate))
        t += timestep
    
    return profile


def generate_residential_profile(
    base_rate: float,
    duration: float = 86400.0,
    timestep: float = 300.0,
    morning_peak: float = 7.0*3600,
    evening_peak: float = 19.0*3600,
    peak_factor: float = 1.5
) -> List[EmissionPoint]:
    """
    Generate residential emission profile (cooking, heating times).
    
    Parameters
    ----------
    base_rate : float
        Base emission rate [units/s]
    duration : float
        Profile duration [s] (default: 24 hours)
    timestep : float
        Time step for output [s]
    morning_peak : float
        Time of morning peak (typically 7 AM)
    evening_peak : float
        Time of evening peak (typically 7 PM)
    peak_factor : float
        Multiplier during cooking hours
    
    Returns
    -------
    List[EmissionPoint]
        Emission points
    """
    profile = []
    t = 0.0
    peak_width = 2.0 * 3600  # 2-hour window
    
    while t <= duration:
        time_of_day = t % 86400.0
        
        # Morning peak
        if abs(time_of_day - morning_peak) < peak_width:
            frac = abs(time_of_day - morning_peak) / peak_width
            rate = base_rate * (1.0 + (peak_factor - 1.0) * max(0, 1.0 - frac))
        # Evening peak
        elif abs(time_of_day - evening_peak) < peak_width:
            frac = abs(time_of_day - evening_peak) / peak_width
            rate = base_rate * (1.0 + (peak_factor - 1.0) * max(0, 1.0 - frac))
        else:
            rate = base_rate
        
        profile.append(EmissionPoint(time=t, rate=rate))
        t += timestep
    
    return profile


def generate_episodic_profile(
    base_rate: float,
    event_time: float,
    event_duration: float,
    event_rate: float,
    duration: float,
    timestep: float = 60.0
) -> List[EmissionPoint]:
    """
    Generate profile with episodic/accidental release.
    
    Parameters
    ----------
    base_rate : float
        Normal emission rate [units/s]
    event_time : float
        Time of event start [s]
    event_duration : float
        Duration of elevated emissions [s]
    event_rate : float
        Elevated emission rate during event [units/s]
    duration : float
        Total profile duration [s]
    timestep : float
        Time step for output [s]
    
    Returns
    -------
    List[EmissionPoint]
        Emission points
    """
    profile = []
    t = 0.0
    
    while t <= duration:
        if event_time <= t <= event_time + event_duration:
            rate = event_rate
        else:
            rate = base_rate
        
        profile.append(EmissionPoint(time=t, rate=rate))
        t += timestep
    
    return profile


def generate_weekly_profile(
    base_rate: float,
    weekend_factor: float = 0.7,
    duration: float = 7 * 86400.0,
    timestep: float = 3600.0
) -> List[EmissionPoint]:
    """
    Generate weekly emission profile with weekend reduction.
    
    Parameters
    ----------
    base_rate : float
        Weekday base rate [units/s]
    weekend_factor : float
        Multiplier for Saturday/Sunday
    duration : float
        Profile duration [s] (default: 7 days)
    timestep : float
        Time step for output [s]
    
    Returns
    -------
    List[EmissionPoint]
        Emission points
    """
    profile = []
    t = 0.0
    seconds_per_day = 86400.0
    
    while t <= duration:
        day_of_week = (t / seconds_per_day) % 7
        
        # Days 5-6 are Saturday-Sunday (5 < day_of_week < 7)
        if 5 < day_of_week < 7:
            rate = base_rate * weekend_factor
        else:
            rate = base_rate
        
        profile.append(EmissionPoint(time=t, rate=rate))
        t += timestep
    
    return profile


def generate_seasonal_profile(
    base_rate: float,
    summer_factor: float = 0.5,
    duration: float = 365 * 86400.0,
    timestep: float = 3600.0
) -> List[EmissionPoint]:
    """
    Generate seasonal emission profile (heating/cooling season variation).
    
    Parameters
    ----------
    base_rate : float
        Winter base rate [units/s]
    summer_factor : float
        Reduction factor for summer months
    duration : float
        Profile duration [s] (default: 365 days)
    timestep : float
        Time step for output [s]
    
    Returns
    -------
    List[EmissionPoint]
        Emission points
    """
    profile = []
    t = 0.0
    seconds_per_year = 365 * 86400.0
    
    while t <= duration:
        # Day of year (0-365)
        day_of_year = (t / 86400.0) % 365
        
        # Summer months (June, July, August): days 152-244
        if 152 < day_of_year < 244:
            rate = base_rate * summer_factor
        else:
            # Linear transition in spring and fall
            if 100 <= day_of_year <= 152:  # Spring transition
                frac = (day_of_year - 100) / 52
                rate = base_rate * (1.0 - frac * (1.0 - summer_factor))
            elif 244 <= day_of_year <= 300:  # Fall transition
                frac = (day_of_year - 244) / 56
                rate = base_rate * (summer_factor + frac * (1.0 - summer_factor))
            else:
                rate = base_rate
        
        profile.append(EmissionPoint(time=t, rate=rate))
        t += timestep
    
    return profile


def write_emission_profile_csv(
    profile: List[EmissionPoint],
    output_file: str,
    description: str = "Emission profile"
) -> None:
    """
    Write emission profile to CSV file.
    
    Parameters
    ----------
    profile : List[EmissionPoint]
        Emission points
    output_file : str
        Output CSV file path
    description : str
        Optional description
    """
    with open(output_file, 'w', newline='') as f:
        writer = csv.writer(f)
        
        # Write metadata header
        writer.writerow(['# Time-Varying Emission Profile'])
        writer.writerow(['# Description:', description])
        writer.writerow(['# Generated by emission_profile_generator.py'])
        writer.writerow([])
        
        # Write column headers
        writer.writerow(['time [s]', 'emission_rate [units/s]'])
        
        # Write profile points
        for point in profile:
            writer.writerow(point.to_csv_row())
    
    print(f"✓ Wrote emission profile to {output_file}")
    print(f"  {len(profile)} time points")
    print(f"  Duration: {profile[-1].time/3600:.1f} hours")
    print(f"  Min rate: {min(p.rate for p in profile):.3e} units/s")
    print(f"  Max rate: {max(p.rate for p in profile):.3e} units/s")
    print(f"  Avg rate: {sum(p.rate for p in profile)/len(profile):.3e} units/s")


def main():
    """Main entry point."""
    parser = argparse.ArgumentParser(
        description="Generate time-varying emission profiles for dispersion models",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # 24-hour traffic pattern
  python emission_profile_generator.py --profile traffic --duration 86400 --output traffic_day.csv
  
  # 7-day industrial cycle
  python emission_profile_generator.py --profile industrial --duration 604800 --output industrial_week.csv
  
  # 365-day seasonal pattern
  python emission_profile_generator.py --profile seasonal --duration 31536000 --output seasonal_year.csv
  
  # Episodic event (accidental release at 12:00 UTC)
  python emission_profile_generator.py --profile episodic --event-time 43200 --event-duration 3600 \\
    --base-rate 1.0 --event-rate 100.0 --duration 86400
        """
    )
    
    parser.add_argument(
        "--profile",
        choices=[p.value for p in ProfileType],
        required=True,
        help="Emission profile type"
    )
    parser.add_argument(
        "--output",
        required=True,
        help="Output CSV filename"
    )
    parser.add_argument(
        "--base-rate",
        type=float,
        default=1.0,
        help="Base emission rate [units/s]"
    )
    parser.add_argument(
        "--duration",
        type=float,
        default=86400.0,
        help="Profile duration [s] (default: 24 hours)"
    )
    parser.add_argument(
        "--timestep",
        type=float,
        default=300.0,
        help="Output time step [s] (default: 5 minutes)"
    )
    
    # Episodic options
    parser.add_argument(
        "--event-time",
        type=float,
        default=43200.0,
        help="Event start time [s] (for episodic profile)"
    )
    parser.add_argument(
        "--event-duration",
        type=float,
        default=3600.0,
        help="Event duration [s] (for episodic profile)"
    )
    parser.add_argument(
        "--event-rate",
        type=float,
        default=10.0,
        help="Event emission rate [units/s] (for episodic profile)"
    )
    
    # Traffic/Industrial options
    parser.add_argument(
        "--peak-factor",
        type=float,
        default=2.0,
        help="Peak/off-peak ratio (for traffic profile)"
    )
    
    parser.add_argument(
        "--weekend-factor",
        type=float,
        default=0.7,
        help="Weekend/weekday ratio (for weekly profile)"
    )
    
    parser.add_argument(
        "--summer-factor",
        type=float,
        default=0.5,
        help="Summer/winter ratio (for seasonal profile)"
    )
    
    args = parser.parse_args()
    
    # Generate appropriate profile
    if args.profile == "constant":
        profile = generate_constant_profile(args.base_rate, args.duration, args.timestep)
    elif args.profile == "traffic":
        profile = generate_traffic_profile(args.base_rate, args.duration, args.timestep, args.peak_factor)
    elif args.profile == "industrial":
        profile = generate_industrial_profile(args.base_rate, args.duration, args.timestep)
    elif args.profile == "residential":
        profile = generate_residential_profile(args.base_rate, args.duration, args.timestep)
    elif args.profile == "episodic":
        profile = generate_episodic_profile(
            args.base_rate, args.event_time, args.event_duration, args.event_rate, args.duration, args.timestep
        )
    elif args.profile == "weekly":
        profile = generate_weekly_profile(args.base_rate, args.weekend_factor, args.duration, args.timestep)
    elif args.profile == "seasonal":
        profile = generate_seasonal_profile(args.base_rate, args.summer_factor, args.duration, args.timestep)
    else:
        print(f"Unknown profile: {args.profile}")
        sys.exit(1)
    
    write_emission_profile_csv(profile, args.output, description=f"Profile type: {args.profile}")


if __name__ == "__main__":
    main()
