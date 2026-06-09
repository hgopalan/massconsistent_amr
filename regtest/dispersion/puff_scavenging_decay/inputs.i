# Puff Wet Deposition and Dynamic Decay Test Case 
# Tests: Wet deposition (precipitation scavenging) and dynamic chemical decay
# Domain: 300m x 300m x 100m
# Source: center of domain at 15m height
# Wind: 5 m/s from west

enable_puff = true

# Source location and emission
source_x  = 150.0
source_y  = 150.0
source_z  = 15.0
emission_rate = 1.0
emission_duration = 60.0

# Diffusivity parameters
K_h = 1.5
K_v = 0.8

# Initial puff size
sigma_y0 = 1.5
sigma_z0 = 1.5

# Enable first-order chemical decay and wet deposition
enable_decay = true
decay_constant = 0.005 # 5e-3 /s

# Wet deposition / scavenging parameters
enable_wet_deposition = true
scavenging_coeff_base = 2.0e-4
precipitation_rate = 2.5
scavenging_exponent = 0.75

# Dynamic decay parameters
enable_dynamic_decay = true
temp_ref = 298.15
temp_coeff = 0.05
rh_ref = 50.0
rh_coeff = 0.01
solar_ref = 500.0
solar_coeff = 1.5
ambient_temp = 303.15  # 5 degrees warmer -> faster decay
ambient_rh = 60.0      # 10% more humid -> faster decay
ambient_solar = 800.0   # stronger UV -> faster decay

# Wind field (uniform for this test)
U_wind = 5.0
V_wind = 0.0
W_wind = 0.0

# Domain extent
xmin = 0.0
xmax = 300.0
ymin = 0.0
ymax = 300.0
zmin = 0.0
zmax = 100.0

# Concentration grid resolution
dx = 10.0
dy = 10.0
dz = 10.0

# Time stepping
dt_puff = 1.0
n_steps_puff = 120
output_freq_puff = 10

# Output file
puff_output = puff_scavenging_decay.csv
