# Phase 5.1 Regression Test: Reactive chemistry
# Test: SO2 -> Sulfate transformation
# Expected behavior: SO2 decays, Sulfate accumulates
# Comparable to: CALPUFF reactive chemistry scenario

puff_model {
    enabled = true
    source_x = 0.0
    source_y = 0.0
    source_z = 50.0
    emission_rate = 1.0
    emission_duration = 3600.0
    K_h = 10.0
    K_v = 1.0
    sigma_y0 = 5.0
    sigma_z0 = 5.0
}

# Phase 3.2: Reactive chemistry
puff_model.enable_reactive_chemistry = true
puff_model.chemistry_file = "chemistry_sox.csv"
puff_model.chemistry_timestep = 60.0

# Phase 4.2: Output specification with chemistry
puff_model.enable_chemistry = true
puff_model.output_enable_wind_components = false
puff_model.output_enable_pressure = false
puff_model.output_enable_terrain = false

# Time stepping
puff_model.dt_puff = 10.0
puff_model.n_steps_puff = 360  # 3600 seconds
puff_model.output_freq_puff = 36  # Output every 360 seconds

# Receptor grid
puff_model.receptors_file = "receptors_chemistry.csv"
puff_model.receptor_output = "receptor_chemistry.csv"
