# Annotated: Simple Single-Source (Baseline)
# ============================================
# 
# This is the simplest configuration: a single elevated point source
# with steady meteorology. Useful as a starting point before adding features.
# 
# Compare to: inputs.i (in repository root)

xmin = -2000.0
xmax = 10000.0
ymin = -2000.0
ymax = 10000.0
zmin = 0.0
zmax = 1000.0

n_cell_x = 120
n_cell_y = 120
n_cell_z = 25

dx = 100.0
dy = 100.0

# === WIND ===
init_mode = loglaw
U_ref = 10.0
V_ref = 0.0
z_ref = 10.0
z0 = 0.1  # Smooth surface (water/urban)

# === PUFF MODEL ===
puff_model.enabled = true

# MINIMAL CONFIGURATION (Phase 1):
# Single source from inputs.i

# emission parameters
puff_model.initial_emission = 1.0  # units/s
puff_model.emission_duration = 3600.0  # 1 hour

# source location
puff_model.source_x = 5000.0
puff_model.source_y = 5000.0
puff_model.source_z = 100.0  # Stack height (meters)

# stack parameters (for plume rise calculation)
puff_model.stack_diameter = 1.5
puff_model.stack_exit_velocity = 10.0
puff_model.stack_exit_temperature = 350.0

# diffusivity
puff_model.K_h = 10.0
puff_model.K_v = 2.0

# output
puff_model.receptor_output_file = concentrations.csv
puff_model.output_frequency = 300.0  # Every 5 minutes

# === SIMULATION TIMING ===
time_stop = 7200.0  # 2 hours
dt_base = 1.0
puff_model.dt_puff = 10.0
puff_model.n_steps_puff = 720  # 2 hours / 10 second steps

# === QUICK START ===
# 1. Add receptor locations manually in puff_models.H or see inputs_phase4_industrial.i
# 2. Run: ./wind_solver inputs_phase1_simple.i
# 3. View output: head concentrations.csv
# 
# === NEXT STEPS (After this works) ===
# A. Add plume rise: puff_model.enable_plume_rise = true
# B. Add multiple sources: See inputs_phase4_industrial.i
# C. Add chemistry: See inputs_phase4_industrial.i
# D. Add deposition: See inputs_phase4_industrial.i
