# Phase 5.1 Regression Test: Time-varying emissions
# Test: Single source with time-varying emission rate
# Expected behavior: Puff mass changes over time
# Comparable to: CALPUFF episodic emission scenario

puff_model {
    enabled = true
    source_x = 0.0
    source_y = 0.0
    source_z = 50.0
    emission_rate = 1.0  # Initial rate, overridden by time series
    emission_duration = 3600.0
    K_h = 10.0
    K_v = 1.0
    sigma_y0 = 5.0
    sigma_z0 = 5.0
}

# Time-varying emissions file
puff_model.emissions_timeseries_file = "emissions_timevary.csv"

# Time stepping
puff_model.dt_puff = 10.0
puff_model.n_steps_puff = 360  # 3600 seconds
puff_model.output_freq_puff = 36  # Output every 360 seconds

# Receptor grid
puff_model.receptors_file = "receptors_timevary.csv"
puff_model.receptor_output = "receptor_timevary.csv"

# Output specification
puff_model.output_enable_wind_components = false
puff_model.output_enable_pressure = false
puff_model.output_enable_terrain = false
