#!/usr/bin/env python3
"""
Example demonstrating IEC 61400-1 wind input models.

This script shows how to use the IEC 61400-1 model classes to generate
wind profiles for wind turbine certification and design studies.

Includes:
- Normal Turbulence Model (NTM)
- Extreme Turbulence Model (ETM)
- Extreme Operating Gust (EOG)
- Extreme Wind Shear (EWS)
- Extreme Coherent Gust (ECG)
"""

import numpy as np
# Optional visualization (not required for example)
# import matplotlib.pyplot as plt
from iec61400_models import (
    NormalTurbulenceModel,
    ExtremeTurbulenceModel,
    ExtremeOperatingGust,
    ExtremeWindShear,
    ExtremeCoherentGust,
    WindTurbineClass,
    create_iec_model,
)


def example_ntm_profile():
    """Example: Generate Normal Turbulence Model profile."""
    print("\n" + "="*70)
    print("Example 1: Normal Turbulence Model (NTM)")
    print("="*70)
    
    # Create NTM for IEC Class II turbine, terrain category 1
    ntm = NormalTurbulenceModel(
        turbine_class="II",
        terrain_category=1,
        z_hub=90.0
    )
    
    print(f"\nTurbine Class: {ntm.turbine_class.value}")
    print(f"Terrain Category: {ntm.terrain_category}")
    print(f"Hub Height: {ntm.z_hub} m")
    print(f"Reference Wind Speed (Vref): {ntm.vref} m/s")
    print(f"Reference Turbulence Intensity: {ntm.iref:.2%}")
    
    # Generate profile at several heights
    heights = np.array([10, 20, 40, 60, 90, 120])
    profile = ntm.generate_wind_profile(heights, mean_speed=8.5)
    
    print("\nWind Profile at Various Heights:")
    print("-" * 50)
    print("Height (m) | Wind Speed (m/s) | Turbulence Intensity")
    print("-" * 50)
    for h, v, ti in zip(
        profile["heights"],
        profile["mean_wind_speed"],
        profile["turbulence_intensity"]
    ):
        print(f"  {h:6.1f}   |     {v:6.3f}      |     {ti:6.2%}")
    
    return heights, profile


def example_etm_profile():
    """Example: Generate Extreme Turbulence Model profile."""
    print("\n" + "="*70)
    print("Example 2: Extreme Turbulence Model (ETM)")
    print("="*70)
    
    # Create ETM for same conditions
    etm = ExtremeTurbulenceModel(
        turbine_class="II",
        terrain_category=1,
        z_hub=90.0
    )
    
    print(f"\nTurbine Class: {etm.turbine_class.value}")
    print(f"Reference Wind Speed: {etm.vref} m/s")
    
    # Generate profile
    heights = np.array([10, 40, 90])
    profile = etm.generate_wind_profile(heights, mean_speed=8.5)
    
    print("\nExtreme Turbulence Profile:")
    print("-" * 50)
    print("Height (m) | Turbulence Intensity (Extreme)")
    print("-" * 50)
    for h, ti in zip(profile["heights"], profile["turbulence_intensity"]):
        ntm_ti = NormalTurbulenceModel("II", 1).turbulence_intensity(h)
        print(f"  {h:6.1f}   |  ETM: {ti:6.2%}  (vs NTM: {ntm_ti:6.2%})")


def example_eog_gust():
    """Example: Generate Extreme Operating Gust profile."""
    print("\n" + "="*70)
    print("Example 3: Extreme Operating Gust (EOG)")
    print("="*70)
    
    eog = ExtremeOperatingGust(
        turbine_class="II",
        terrain_category=1,
    )
    
    print(f"\nTurbine Class: {eog.turbine_class.value}")
    print(f"Reference Wind Speed: {eog.vref} m/s")
    
    # Generate gust profile
    mean_speed = 12.0
    gust = eog.generate_gust_profile(
        duration=10.0,
        time_to_peak=5.0,
        mean_speed=mean_speed,
        sampling_rate=10.0
    )
    
    print(f"\nGust Parameters:")
    print(f"  Mean Operating Speed: {mean_speed} m/s")
    print(f"  Peak Gust Amplitude: {gust['peak_gust']:.2f} m/s")
    print(f"  Duration: 10 seconds")
    print(f"  Time to Peak: 5 seconds")
    
    # Show gust values at key times
    print("\nGust Speed at Key Times:")
    print("-" * 40)
    key_times = [0, 2.5, 5.0, 7.5, 10.0]
    for t in key_times:
        idx = int(t * 10)  # 10 Hz sampling
        if idx < len(gust["gust_profile"]):
            print(f"  t = {t:5.1f} s: {gust['gust_profile'][idx]:6.2f} m/s")


def example_ews_shear():
    """Example: Generate Extreme Wind Shear profile."""
    print("\n" + "="*70)
    print("Example 4: Extreme Wind Shear (EWS)")
    print("="*70)
    
    ews = ExtremeWindShear(
        turbine_class="II",
        terrain_category=1,
        z_hub=90.0
    )
    
    print(f"\nTurbine Class: {ews.turbine_class.value}")
    print(f"Terrain Category: {ews.terrain_category}")
    
    # Generate shear profile
    heights = np.linspace(10, 150, 15)
    profile = ews.generate_shear_profile(heights, reference_speed=10.0)
    
    print("\nExtreme Wind Shear Profile:")
    print("-" * 50)
    print("Height (m) | Wind Speed (m/s) | Shear Effect")
    print("-" * 50)
    for h, v in zip(profile["heights"], profile["wind_speed"]):
        normal_v = 10.0 * (h / 10.0) ** 0.2  # Normal shear
        shear_increase = ((v - normal_v) / normal_v) * 100 if normal_v > 0 else 0
        print(f"  {h:6.1f}   |     {v:6.3f}      | {shear_increase:+6.1f}%")


def example_ecg_gust():
    """Example: Generate Extreme Coherent Gust with direction change."""
    print("\n" + "="*70)
    print("Example 5: Extreme Coherent Gust (ECG)")
    print("="*70)
    
    ecg = ExtremeCoherentGust(
        turbine_class="II",
        terrain_category=1,
    )
    
    print(f"\nTurbine Class: {ecg.turbine_class.value}")
    
    # Generate gust with direction change
    mean_speed = 10.0
    gust = ecg.generate_gust_with_direction_change(
        duration=10.0,
        time_to_peak=5.0,
        mean_speed=mean_speed,
        direction_change=180.0,  # 180-degree direction change
        sampling_rate=10.0
    )
    
    print(f"\nCoherent Gust Parameters:")
    print(f"  Mean Operating Speed: {mean_speed} m/s")
    print(f"  Peak Gust Speed: {gust['peak_gust']:.2f} m/s")
    print(f"  Direction Change: {gust['total_direction_change']:.1f}°")
    print(f"  Duration: 10 seconds")
    
    # Show gust and direction at key times
    print("\nGust Speed and Direction Change at Key Times:")
    print("-" * 50)
    print("Time (s) | Gust Speed (m/s) | Direction Change (°)")
    print("-" * 50)
    key_times = [0, 2.5, 5.0, 7.5, 10.0]
    for t in key_times:
        idx = int(t * 10)
        if idx < len(gust["gust_speed"]):
            print(
                f"  {t:5.1f}  |      {gust['gust_speed'][idx]:6.2f}     |"
                f"     {gust['direction_change'][idx]:6.1f}"
            )


def example_factory_function():
    """Example: Using factory function to create models."""
    print("\n" + "="*70)
    print("Example 6: Using Factory Function")
    print("="*70)
    
    model_types = ["NTM", "ETM", "EOG", "EWS", "ECG"]
    
    print("\nCreating all model types using factory function:")
    print("-" * 50)
    
    for model_type in model_types:
        model = create_iec_model(
            model_type=model_type,
            turbine_class="II",
            terrain_category=1,
            z_hub=90.0
        )
        print(f"✓ Created {model_type:4s}: {type(model).__name__}")


def example_comparison_ntm_vs_etm():
    """Example: Compare NTM and ETM turbulence intensity."""
    print("\n" + "="*70)
    print("Example 7: NTM vs ETM Comparison")
    print("="*70)
    
    ntm = NormalTurbulenceModel("II", terrain_category=1)
    etm = ExtremeTurbulenceModel("II", terrain_category=1)
    
    heights = np.array([10, 20, 40, 60, 90, 120])
    
    print("\nTurbulence Intensity Comparison:")
    print("-" * 60)
    print("Height (m) | NTM TI (%) | ETM TI (%) | ETM/NTM Ratio")
    print("-" * 60)
    
    for h in heights:
        ntm_ti = ntm.turbulence_intensity(h)
        etm_ti = etm.turbulence_intensity(h)
        ratio = etm_ti / ntm_ti if ntm_ti > 0 else 0
        print(
            f"  {h:6.1f}   |  {ntm_ti:7.2%}   |  {etm_ti:7.2%}   | {ratio:6.2f}"
        )


def main():
    """Run all examples."""
    print("\n")
    print("╔" + "="*68 + "╗")
    print("║" + " "*68 + "║")
    print("║" + "  IEC 61400-1 Wind Input Models - Examples".center(68) + "║")
    print("║" + " "*68 + "║")
    print("╚" + "="*68 + "╝")
    
    # Run all examples
    example_ntm_profile()
    example_etm_profile()
    example_eog_gust()
    example_ews_shear()
    example_ecg_gust()
    example_factory_function()
    example_comparison_ntm_vs_etm()
    
    print("\n" + "="*70)
    print("All examples completed successfully!")
    print("="*70 + "\n")


if __name__ == "__main__":
    main()
