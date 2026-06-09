import math
import sys

# Generate steep Gaussian hill terrain
# 11x11 grid over 200x200 m domain, peak elevation 100 m at center (steep!)
nx, ny = 11, 11
xmin, xmax = 0.0, 200.0
ymin, ymax = 0.0, 200.0

# Center of domain
xc, yc = 100.0, 100.0

# Steep Gaussian hill: z = 100 * exp(-((x-xc)^2 + (y-yc)^2) / (2 * sigma^2))
# Using small sigma (20 m) to create steep slopes
sigma = 20.0
z_peak = 100.0

print("# X Y Z")
print("# Steep Gaussian hill terrain for terrain-following coords test")
print("# 11x11 grid, 200x200 m domain, 100 m peak height, sigma=20 m (steep!)")

for j in range(ny):
    y = ymin + j * (ymax - ymin) / (ny - 1)
    for i in range(nx):
        x = xmin + i * (xmax - xmin) / (nx - 1)
        r2 = (x - xc)**2 + (y - yc)**2
        z = z_peak * math.exp(-r2 / (2 * sigma**2))
        print(f"{x:.6f} {y:.6f} {z:.6f}")

sys.stderr.write(f"Generated steep Gaussian hill terrain: {nx}x{ny} points\n")
sys.stderr.write(f"Domain: [{xmin}, {xmax}] x [{ymin}, {ymax}]\n")
sys.stderr.write(f"Peak elevation: {z_peak} m at ({xc}, {yc})\n")
sys.stderr.write(f"Sigma: {sigma} m (creates steep slopes)\n")
