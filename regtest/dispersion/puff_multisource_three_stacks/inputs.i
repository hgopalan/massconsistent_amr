# Phase 5.1 Regression Test: Multi-source dispersion
# Test: Three industrial stacks with different emission rates
# Expected behavior: Superposition of three Gaussian puffs
# Comparable to: CALPUFF multi-stack scenario

puff_model {
    enabled = true
    K_h = 10.0
    K_v = 1.0
    sigma_y0 = 5.0
    sigma_z0 = 5.0
}

# Three emission sources at different locations
# Stack 1: High-level stack (100 m)
puff_model.source_x = 0.0
puff_model.source_y = 0.0
puff_model.source_z = 100.0
puff_model.emission_rate = 1.0
puff_model.emission_duration = 3600.0

# CSV file with three sources
puff_model.sources_file = "sources_three_stacks.csv"

# Time stepping
puff_model.dt_puff = 10.0
puff_model.n_steps_puff = 360  # 3600 seconds
puff_model.output_freq_puff = 36  # Output every 360 seconds

# Receptor grid
puff_model.receptors_file = "receptors_multisource.csv"
puff_model.receptor_output = "receptor_multisource.csv"

# Output specification for Phase 4.2
puff_model.output_enable_wind_components = false
puff_model.output_enable_pressure = false
puff_model.output_enable_terrain = false
