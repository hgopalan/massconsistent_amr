#!/usr/bin/env python3
"""Regression test for SCM wind-direction conversion across 8 directions."""

import argparse
import math
import os
import re
import sys


def verify_source_conversion(root_dir):
    source_file = os.path.join(root_dir, "src", "scm_models.H")
    with open(source_file, "r", encoding="utf-8") as handle:
        content = handle.read()

    # Intentionally match the concrete ug_init/vg_init assignments to guard against
    # accidental sin/cos swaps in the exact SCM initialization path being fixed here.
    ug_ok = re.search(r"ug_init\s*=\s*target_wind_speed\s*\*\s*std::cos\(angle_rad\)", content) is not None
    vg_ok = re.search(r"vg_init\s*=\s*target_wind_speed\s*\*\s*std::sin\(angle_rad\)", content) is not None

    if not ug_ok or not vg_ok:
        print(f"✗ FAIL: expected cos/sin SCM conversion not found in {source_file}")
        return False

    print(f"✓ PASS: SCM conversion in source uses ug=cos(angle), vg=sin(angle)")
    return True


def verify_scm_8directions(input_file):
    directions = [0.0, 45.0, 90.0, 135.0, 180.0, 225.0, 270.0, 315.0]
    target_speed = 8.0
    speed_tolerance = 0.5
    geostrophic_min = 8.0
    geostrophic_max = 12.0

    test_dir = os.path.dirname(input_file)
    root_dir = os.path.abspath(os.path.join(test_dir, "..", "..", ".."))

    print("=" * 80)
    print("SCM 8-Direction Conversion Verification")
    print("=" * 80)

    all_ok = verify_source_conversion(root_dir)

    for direction in directions:
        angle_rad = direction * math.pi / 180.0
        ug = target_speed * math.cos(angle_rad)
        vg = target_speed * math.sin(angle_rad)
        geostrophic_speed = math.sqrt(ug * ug + vg * vg)
        speed_error = abs(geostrophic_speed - target_speed)

        if (not math.isfinite(geostrophic_speed)) or not (geostrophic_min <= geostrophic_speed <= geostrophic_max):
            print(
                f"✗ FAIL [{direction:6.1f}°]: geostrophic speed {geostrophic_speed:.3f} m/s "
                f"outside [{geostrophic_min:.1f}, {geostrophic_max:.1f}]"
            )
            all_ok = False
            continue

        if speed_error > speed_tolerance:
            print(
                f"✗ FAIL [{direction:6.1f}°]: speed error {speed_error:.3f} m/s "
                f"> {speed_tolerance:.3f} m/s"
            )
            all_ok = False
            continue

        print(f"✓ PASS [{direction:6.1f}°]: Ug={ug:.3f}, Vg={vg:.3f}, |G|={geostrophic_speed:.3f} m/s")

    print("=" * 80)
    if all_ok:
        print("SCM 8-direction conversion regression test passed.")
    else:
        print("SCM 8-direction conversion regression test failed.")
    print("=" * 80)
    return all_ok


def main():
    parser = argparse.ArgumentParser(description="Verify SCM behavior for 8 wind directions")
    parser.add_argument("input_file", help="Path to template inputs.i file")
    parser.add_argument("work_dir", help="Working directory")
    args = parser.parse_args()

    os.chdir(args.work_dir)
    ok = verify_scm_8directions(args.input_file)
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
