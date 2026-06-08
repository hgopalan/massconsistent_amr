#!/usr/bin/env python3
import numpy as np
import os

def generate_data():
    # Set seed for reproducibility
    np.random.seed(42)

    # 1. Generate Randomized Hill Terrain
    nx, ny = 21, 21
    domain_x, domain_y = 1000.0, 1000.0
    dx = domain_x / (nx - 1)
    dy = domain_y / (ny - 1)

    xc, yc = 500.0, 500.0
    peak_height = 100.0
    sigma = 150.0

    terrain_data = []
    for j in range(ny):
        y = j * dy
        for i in range(nx):
            x = i * dx
            
            # Base Gaussian hill
            r_squared = (x - xc)**2 + (y - yc)**2
            base_z = peak_height * np.exp(-r_squared / (2.0 * sigma**2))
            
            # Taper factor to ensure edges are flat at z = 0
            dist_x = min(x, domain_x - x) / 200.0
            dist_y = min(y, domain_y - y) / 200.0
            taper = min(1.0, max(0.0, dist_x)) * min(1.0, max(0.0, dist_y))
            
            # Add randomized variations
            noise = np.random.uniform(-5.0, 5.0) * taper
            z = max(0.0, base_z + noise)
            
            terrain_data.append((x, y, z))

    # Write terrain.csv
    terrain_file = "terrain.csv"
    with open(terrain_file, "w") as f:
        f.write("# Randomized hill terrain  X[m]  Y[m]  Z[m]\n")
        f.write(f"# Domain: 0-{domain_x} x 0-{domain_y} m, base peak={peak_height}m, sigma={sigma}m\n")
        for x, y, z in terrain_data:
            f.write(f"{x:.6f} {y:.6f} {z:.6f}\n")
    print(f"✓ Generated {terrain_file}")

    # 2. Generate 20 Random Turbines
    # Let's seed separately for turbine placement to keep it distinct
    np.random.seed(101)
    tx = np.random.uniform(150.0, 850.0, 20)
    ty = np.random.uniform(150.0, 850.0, 20)
    
    hub_height = 90.0
    rotor_diameter = 126.0
    default_ct = 0.8
    power_curve = "nrel_5mw.csv"

    turbines_file = "turbines.csv"
    with open(turbines_file, "w") as f:
        f.write("# x, y, hub_height, rotor_diameter, default_ct, power_curve_file\n")
        for x, y in zip(tx, ty):
            f.write(f"{x:.2f}, {y:.2f}, {hub_height:.1f}, {rotor_diameter:.1f}, {default_ct:.1f}, {power_curve}\n")
    print(f"✓ Generated {turbines_file}")

    # 3. Generate Time-Varying Wind Time Series
    # Starts from West (270 deg) and rotates clockwise through all 16 cardinal directions
    cardinal_dirs = [
        "W", "WNW", "NW", "NNW", "N", "NNE", "NE", "ENE",
        "E", "ESE", "SE", "SSE", "S", "SSW", "SW", "WSW"
    ]
    # Corresponding angles in meteorological degrees (0 is North, clockwise)
    # Starts at 270 (West)
    start_angle = 270.0
    angles = [(start_angle + i * 22.5) % 360.0 for i in range(16)]
    # Append the final West point to complete the 3600 second rotation
    angles.append(start_angle)

    time_series_file = "time_series.csv"
    wind_speed = 10.0 # 10 m/s incoming wind speed

    with open(time_series_file, "w") as f:
        f.write("# Time-varying wind boundary conditions\n")
        f.write("# Format: Time(s) U_ref(m/s) V_ref(m/s) Direction(deg) Wind_Speed(m/s)\n")
        for i, angle in enumerate(angles):
            t = i * 225.0
            # Calculate U_ref and V_ref (blowing to direction = angle + 180)
            angle_rad = np.radians(angle)
            u_ref = -wind_speed * np.sin(angle_rad)
            v_ref = -wind_speed * np.cos(angle_rad)
            f.write(f"{t:.1f} {u_ref:.6f} {v_ref:.6f} {angle:.1f} {wind_speed:.1f}\n")
    print(f"✓ Generated {time_series_file}")

if __name__ == "__main__":
    generate_data()
