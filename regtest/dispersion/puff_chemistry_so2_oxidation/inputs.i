# SO₂ Oxidation Regression Test
# Simple uniform wind field with SO₂ source
# Validates oxidation to SO₄²⁻ with 24-hour half-life
# Domain: 100km downwind at 5 m/s = 20,000 seconds = 5.56 hours
# Expected at 5.56 hours: SO₂ ≈ 71% of initial (0.78 half-life)

# Enable puff model
enable_puff = true

# Source location and emission
source_x  = 500.0      # Source x-coordinate [m]
source_y  = 500.0      # Source y-coordinate [m]
source_z  = 50.0       # Stack height [m]
emission_rate = 1.0    # Emission rate [kg/s]
emission_duration = 1.0  # Short duration (single puff)

# Diffusivity parameters
K_h = 2.0              # Horizontal diffusivity [m²/s]
K_v = 1.0              # Vertical diffusivity [m²/s]

# Initial puff size
sigma_y0 = 5.0         # Initial lateral width [m]
sigma_z0 = 5.0         # Initial vertical height [m]

# Wind field (uniform, slower for longer transport)
U_wind = 5.0           # x-component of wind [m/s]
V_wind = 0.0           # y-component of wind [m/s]
W_wind = 0.0           # z-component of wind [m/s]

# Domain extent (larger for long-range transport)
xmin = 0.0             # Domain min x [m]
xmax = 100000.0        # Domain max x [m] = 100 km
ymin = -10000.0        # Domain min y [m]
ymax = 10000.0         # Domain max y [m]
zmin = 0.0             # Domain min z [m]
zmax = 200.0           # Domain max z [m]

# Concentration grid resolution
dx = 500.0             # Grid spacing x [m]
dy = 500.0             # Grid spacing y [m]
dz = 20.0              # Grid spacing z [m]

# Time stepping: 20000 seconds = 5.56 hours
# At 5 m/s wind, puff travels 100 km in 20,000 seconds
dt_puff = 200.0        # Time step [s]
n_steps_puff = 100     # Total time = 20,000 s = 5.56 hours
output_freq_puff = 1   # Write every step

# Output file
puff_output = puff_concentration.csv

# Chemical species parameters
puff_chemistry_enabled = true
puff_chemistry_half_life_SO2 = 24.0   # 24-hour half-life
puff_chemistry_enable_products = true  # Track SO₄ production
puff_chemistry_enable_seasonal_adjust = false
puff_chemistry_enable_temp_adjust = false

# Initial chemical concentrations [ppb]
puff_initial_SO2 = 200.0     # 200 ppb SO₂
puff_initial_SO4 = 0.0       # No initial SO₄
