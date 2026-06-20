#!/usr/bin/env python3
"""
Test suite for IEC 61400-1 wind input models.

Validates:
- Model initialization
- Wind profile generation
- Turbulence intensity calculations
- Gust profile generation
- Wind shear profiles
- Coherent gust with direction change
- Parameter lookup tables
- Factory function
"""

import sys
import os
import numpy as np

# Add src/python to path for importing iec61400_models
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', '..', 'src', 'python'))

from iec61400_models import (
    NormalTurbulenceModel,
    ExtremeTurbulenceModel,
    ExtremeOperatingGust,
    ExtremeWindShear,
    ExtremeCoherentGust,
    WindTurbineClass,
    create_iec_model,
    TERRAIN_ROUGHNESS,
    TERRAIN_SHEAR_EXPONENT,
    IEC_CLASS_PARAMETERS,
)


def test_model_initialization():
    """Test initialization of all models."""
    print("\n" + "="*70)
    print("Test 1: Model Initialization")
    print("="*70)
    
    models = [
        (NormalTurbulenceModel, "NTM"),
        (ExtremeTurbulenceModel, "ETM"),
        (ExtremeOperatingGust, "EOG"),
        (ExtremeWindShear, "EWS"),
        (ExtremeCoherentGust, "ECG"),
    ]
    
    for ModelClass, name in models:
        try:
            model = ModelClass(turbine_class="II", terrain_category=1, z_hub=90.0)
            assert model.turbine_class == WindTurbineClass.CLASS_II, f"{name}: Wrong turbine class"
            assert model.terrain_category == 1, f"{name}: Wrong terrain category"
            assert model.z_hub == 90.0, f"{name}: Wrong hub height"
            
            # Use parameterized values from lookup table
            expected_params = IEC_CLASS_PARAMETERS[WindTurbineClass.CLASS_II]
            assert model.vref == expected_params["vref"], f"{name}: Wrong vref"
            assert model.iref == expected_params["iref"], f"{name}: Wrong iref"
            assert model.z0 == TERRAIN_ROUGHNESS[1], f"{name}: Wrong roughness"
            print(f"✓ {name:4s} initialization OK")
        except Exception as e:
            print(f"✗ {name:4s} initialization FAILED: {e}")
            return False
    
    return True


def test_turbine_classes():
    """Test all wind turbine classes."""
    print("\n" + "="*70)
    print("Test 2: Wind Turbine Classes")
    print("="*70)
    
    classes = ["I", "II", "III", "IV"]
    
    for class_str in classes:
        try:
            model = NormalTurbulenceModel(turbine_class=class_str, terrain_category=1)
            assert model.turbine_class == WindTurbineClass(class_str), f"Class {class_str}: Wrong enum"
            
            # Check parameters match lookup table
            expected_params = IEC_CLASS_PARAMETERS[WindTurbineClass(class_str)]
            assert model.vref == expected_params["vref"], f"Class {class_str}: Wrong vref"
            assert model.vavg == expected_params["vavg"], f"Class {class_str}: Wrong vavg"
            assert model.iref == expected_params["iref"], f"Class {class_str}: Wrong iref"
            
            print(f"✓ Class {class_str}: vref={model.vref} m/s, iref={model.iref:.2%}")
        except Exception as e:
            print(f"✗ Class {class_str} FAILED: {e}")
            return False
    
    return True


def test_terrain_categories():
    """Test all terrain categories."""
    print("\n" + "="*70)
    print("Test 3: Terrain Categories")
    print("="*70)
    
    for tc in range(5):
        try:
            model = NormalTurbulenceModel(turbine_class="II", terrain_category=tc)
            expected_z0 = TERRAIN_ROUGHNESS[tc]
            expected_shear = TERRAIN_SHEAR_EXPONENT[tc]
            
            assert model.z0 == expected_z0, f"TC {tc}: Wrong roughness"
            assert model.shear_exponent == expected_shear, f"TC {tc}: Wrong shear"
            
            print(f"✓ Terrain Category {tc}: z₀={model.z0:.4f} m, α={model.shear_exponent:.3f}")
        except Exception as e:
            print(f"✗ Terrain Category {tc} FAILED: {e}")
            return False
    
    return True


def test_ntm_wind_profile():
    """Test NTM wind profile generation."""
    print("\n" + "="*70)
    print("Test 4: NTM Wind Profile Generation")
    print("="*70)
    
    try:
        ntm = NormalTurbulenceModel("II", terrain_category=1)
        
        # Test power-law profile
        heights = np.array([10, 40, 90, 120])
        wind_speeds = ntm.power_law_profile(heights, reference_speed=8.5, reference_height=10.0)
        
        # Verify monotonicity (wind speed increases with height)
        assert np.all(np.diff(wind_speeds) > 0), "Wind speeds not monotonically increasing"
        
        # Verify at reference height
        ref_speed = ntm.power_law_profile(
            np.array([10.0]), reference_speed=8.5, reference_height=10.0
        )
        assert np.isclose(ref_speed[0], 8.5, atol=0.01), f"Reference speed mismatch: {ref_speed[0]}"
        
        print(f"✓ Power-law profile: speeds range {wind_speeds.min():.2f}-{wind_speeds.max():.2f} m/s")
        
        # Test log-law profile
        wind_speeds_log = ntm.log_law_profile(heights, reference_speed=8.5, reference_height=10.0)
        assert np.all(np.diff(wind_speeds_log) > 0), "Log-law speeds not monotonically increasing"
        print(f"✓ Log-law profile: speeds range {wind_speeds_log.min():.2f}-{wind_speeds_log.max():.2f} m/s")
        
        # Test full profile generation
        profile = ntm.generate_wind_profile(heights, mean_speed=8.5)
        assert "heights" in profile, "Missing 'heights' in profile"
        assert "mean_wind_speed" in profile, "Missing 'mean_wind_speed' in profile"
        assert "turbulence_intensity" in profile, "Missing 'turbulence_intensity' in profile"
        assert profile["model_type"] == "NTM", "Wrong model type"
        
        print(f"✓ Generated wind profile with {len(profile['heights'])} heights")
        
        return True
    except Exception as e:
        print(f"✗ NTM wind profile FAILED: {e}")
        return False


def test_ntm_turbulence_intensity():
    """Test NTM turbulence intensity."""
    print("\n" + "="*70)
    print("Test 5: NTM Turbulence Intensity")
    print("="*70)
    
    try:
        ntm = NormalTurbulenceModel("II", terrain_category=1)
        
        # Test at multiple heights
        heights = np.array([10, 20, 40, 60, 90, 120, 150])
        ti_values = np.array([ntm.turbulence_intensity(h) for h in heights])
        
        # TI should decrease with height
        assert np.all(np.diff(ti_values) < 0), "TI not decreasing with height"
        
        # All TI values should be positive and less than 1
        assert np.all(ti_values > 0), "TI values not all positive"
        assert np.all(ti_values < 1.0), "TI values exceed 100%"
        
        print(f"✓ Turbulence intensity decreases with height")
        print(f"  Height (m): {list(heights)}")
        print(f"  TI values : {[f'{x*100:.2f}%' for x in ti_values]}")
        
        return True
    except Exception as e:
        print(f"✗ NTM turbulence intensity FAILED: {e}")
        return False


def test_etm_vs_ntm():
    """Test that ETM turbulence is higher than NTM."""
    print("\n" + "="*70)
    print("Test 6: ETM vs NTM Comparison")
    print("="*70)
    
    try:
        ntm = NormalTurbulenceModel("II", terrain_category=1)
        etm = ExtremeTurbulenceModel("II", terrain_category=1)
        
        heights = np.array([10, 40, 90, 120])
        
        for h in heights:
            ti_ntm = ntm.turbulence_intensity(h)
            ti_etm = etm.turbulence_intensity(h)
            
            # ETM should be roughly 1.4 times NTM at hub height
            ratio = ti_etm / ti_ntm if ti_ntm > 0 else 0
            assert ratio > 1.0, f"ETM not greater than NTM at {h}m"
        
        print(f"✓ ETM turbulence intensity is higher than NTM across all heights")
        
        return True
    except Exception as e:
        print(f"✗ ETM vs NTM comparison FAILED: {e}")
        return False


def test_eog_gust_generation():
    """Test EOG gust profile generation."""
    print("\n" + "="*70)
    print("Test 7: EOG Gust Generation")
    print("="*70)
    
    try:
        eog = ExtremeOperatingGust("II", terrain_category=1)
        
        # Generate gust
        gust = eog.generate_gust_profile(
            duration=10.0,
            time_to_peak=5.0,
            mean_speed=12.0,
            sampling_rate=10.0
        )
        
        # Check keys
        required_keys = ["time", "gust_profile", "peak_gust", "mean_speed", "model_type"]
        for key in required_keys:
            assert key in gust, f"Missing key: {key}"
        
        # Check array shapes
        assert len(gust["time"]) > 0, "Empty time array"
        assert len(gust["gust_profile"]) == len(gust["time"]), "Mismatched array lengths"
        
        # Check gust shape (should ramp up then decay)
        peak_idx = np.argmax(gust["gust_profile"])
        time_to_peak_actual = gust["time"][peak_idx]
        assert abs(time_to_peak_actual - 5.0) < 1.0, f"Peak not at expected time: {time_to_peak_actual}"
        
        # Gust should start and end near zero
        assert gust["gust_profile"][0] < gust["peak_gust"] * 0.1, "Gust doesn't start near zero"
        assert gust["gust_profile"][-1] < gust["peak_gust"] * 0.1, "Gust doesn't decay near zero"
        
        print(f"✓ EOG gust generated successfully")
        print(f"  Duration: {gust['time'][-1]:.1f} s")
        print(f"  Peak gust: {gust['peak_gust']:.2f} m/s at t={gust['time'][peak_idx]:.1f} s")
        print(f"  Mean speed: {gust['mean_speed']:.1f} m/s")
        
        return True
    except Exception as e:
        print(f"✗ EOG gust generation FAILED: {e}")
        return False


def test_ews_shear_profile():
    """Test EWS wind shear profile."""
    print("\n" + "="*70)
    print("Test 8: EWS Wind Shear Profile")
    print("="*70)
    
    try:
        ews = ExtremeWindShear("II", terrain_category=1, z_hub=90.0)
        
        # Generate shear profile
        heights = np.linspace(10, 150, 30)
        profile = ews.generate_shear_profile(heights, reference_speed=10.0)
        
        # Check keys
        required_keys = ["heights", "wind_speed", "shear_exponent", "model_type"]
        for key in required_keys:
            assert key in profile, f"Missing key: {key}"
        
        # Wind speeds should increase with height
        assert np.all(np.diff(profile["wind_speed"]) > 0), "Speeds not monotonically increasing"
        
        # Shear exponent should be positive and reasonable
        assert 0.0 < profile["shear_exponent"] < 1.0, f"Unreasonable shear exponent: {profile['shear_exponent']}"
        
        # EWS should have higher shear than normal
        normal_shear = ews.shear_exponent
        assert profile["shear_exponent"] > normal_shear, "EWS shear not enhanced"
        
        print(f"✓ EWS shear profile generated successfully")
        print(f"  Shear exponent: {profile['shear_exponent']:.3f} (vs normal {normal_shear:.3f})")
        print(f"  Wind speed range: {profile['wind_speed'].min():.2f}-{profile['wind_speed'].max():.2f} m/s")
        
        return True
    except Exception as e:
        print(f"✗ EWS shear profile FAILED: {e}")
        return False


def test_ecg_direction_change():
    """Test ECG gust with direction change."""
    print("\n" + "="*70)
    print("Test 9: ECG Direction Change")
    print("="*70)
    
    try:
        ecg = ExtremeCoherentGust("II", terrain_category=1)
        
        # Generate gust with direction change
        gust = ecg.generate_gust_with_direction_change(
            duration=10.0,
            time_to_peak=5.0,
            mean_speed=10.0,
            direction_change=180.0,
            sampling_rate=10.0
        )
        
        # Check keys
        required_keys = [
            "time", "gust_speed", "direction_change", "peak_gust",
            "total_direction_change", "mean_speed", "model_type"
        ]
        for key in required_keys:
            assert key in gust, f"Missing key: {key}"
        
        # Check array shapes
        assert len(gust["time"]) > 0, "Empty time array"
        assert len(gust["gust_speed"]) == len(gust["time"]), "Mismatched gust arrays"
        assert len(gust["direction_change"]) == len(gust["time"]), "Mismatched direction arrays"
        
        # Direction change should reach specified value
        assert gust["total_direction_change"] == 180.0, "Wrong total direction change"
        
        # Direction should increase monotonically to peak, then stay constant
        direction_peak_idx = np.argmax(gust["direction_change"])
        assert gust["direction_change"][-1] == gust["direction_change"][direction_peak_idx], \
            "Direction doesn't maintain constant value after peak"
        
        print(f"✓ ECG direction change generated successfully")
        print(f"  Peak gust: {gust['peak_gust']:.2f} m/s")
        print(f"  Direction change: {gust['total_direction_change']:.1f}°")
        print(f"  Duration: {gust['time'][-1]:.1f} s")
        
        return True
    except Exception as e:
        print(f"✗ ECG direction change FAILED: {e}")
        return False


def test_factory_function():
    """Test factory function for model creation."""
    print("\n" + "="*70)
    print("Test 10: Factory Function")
    print("="*70)
    
    try:
        model_types = ["NTM", "ETM", "EOG", "EWS", "ECG"]
        
        for model_type in model_types:
            model = create_iec_model(
                model_type=model_type,
                turbine_class="II",
                terrain_category=1,
                z_hub=90.0
            )
            
            # Verify model type
            type_names = {
                "NTM": NormalTurbulenceModel,
                "ETM": ExtremeTurbulenceModel,
                "EOG": ExtremeOperatingGust,
                "EWS": ExtremeWindShear,
                "ECG": ExtremeCoherentGust,
            }
            assert isinstance(model, type_names[model_type]), f"Wrong model type for {model_type}"
            
            print(f"✓ Created {model_type}: {type(model).__name__}")
        
        # Test error on invalid model type
        try:
            create_iec_model("INVALID", "II", 1)
            print(f"✗ Should have raised ValueError for invalid model type")
            return False
        except ValueError:
            print(f"✓ Correctly rejects invalid model type")
        
        return True
    except Exception as e:
        print(f"✗ Factory function FAILED: {e}")
        return False


def test_invalid_parameters():
    """Test error handling for invalid parameters."""
    print("\n" + "="*70)
    print("Test 11: Error Handling")
    print("="*70)
    
    try:
        # Test invalid turbine class
        try:
            NormalTurbulenceModel(turbine_class="INVALID")
            print(f"✗ Should have raised ValueError for invalid turbine class")
            return False
        except ValueError:
            print(f"✓ Correctly rejects invalid turbine class")
        
        # Test invalid terrain category (above maximum of 4)
        try:
            NormalTurbulenceModel("II", terrain_category=5)
            print(f"✗ Should have raised ValueError for terrain category=5 (> 4)")
            return False
        except ValueError:
            print(f"✓ Correctly rejects terrain_category=5 (upper boundary violation)")
        
        # Test invalid terrain category (negative)
        try:
            NormalTurbulenceModel("II", terrain_category=-1)
            print(f"✗ Should have raised ValueError for negative terrain category")
            return False
        except ValueError:
            print(f"✓ Correctly rejects negative terrain category")
        
        # Test valid upper boundary terrain category (should not raise)
        try:
            NormalTurbulenceModel("II", terrain_category=4)
            print(f"✓ Correctly accepts terrain_category=4 (upper boundary)")
        except ValueError:
            print(f"✗ Should not raise ValueError for terrain_category=4")
            return False
        
        return True
    except Exception as e:
        print(f"✗ Error handling test FAILED: {e}")
        return False


def run_all_tests():
    """Run all test cases."""
    print("\n")
    print("╔" + "="*68 + "╗")
    print("║" + " "*68 + "║")
    print("║" + "  IEC 61400-1 Models - Test Suite".center(68) + "║")
    print("║" + " "*68 + "║")
    print("╚" + "="*68 + "╝")
    
    tests = [
        test_model_initialization,
        test_turbine_classes,
        test_terrain_categories,
        test_ntm_wind_profile,
        test_ntm_turbulence_intensity,
        test_etm_vs_ntm,
        test_eog_gust_generation,
        test_ews_shear_profile,
        test_ecg_direction_change,
        test_factory_function,
        test_invalid_parameters,
    ]
    
    passed = 0
    failed = 0
    
    for test_func in tests:
        try:
            if test_func():
                passed += 1
            else:
                failed += 1
        except Exception as e:
            print(f"\n✗ {test_func.__name__} raised exception: {e}")
            failed += 1
    
    # Summary
    print("\n" + "="*70)
    print("Test Summary")
    print("="*70)
    print(f"Passed: {passed}/{len(tests)}")
    print(f"Failed: {failed}/{len(tests)}")
    
    if failed == 0:
        print("\n✓ All tests passed!")
        return 0
    else:
        print(f"\n✗ {failed} test(s) failed")
        return 1


if __name__ == "__main__":
    exit_code = run_all_tests()
    sys.exit(exit_code)
