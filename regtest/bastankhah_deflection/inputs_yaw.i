# Standalone Bastankhah deflection / veer test case
terrain_file = terrain.csv
enable_turbine_wake = true
turbine_file = turbines_test.csv
turbine_wake_model_type = bastankhah_gaussian
turbine_wake_superposition = quadratic
wake_added_turbulence_model = none
enable_jimenez_deflection = false
enable_bastankhah_deflection = true
turbopark_c1 = 0.38
ambient_ti = 0.075

# Veer settings if enabled
enable_ekman_veer = false
ekman_veer_total = 30.0
ekman_veer_height = 50.0

U_ref = 10.0
V_ref = 0.0
z_ref = 10.0
z0 = 0.1
dx = 10.0
dy = 10.0
dz = 10.0
domain_height = 100.0
alpha_h = 1.0
alpha_v = 1.0
mlmg_verbose = 0
max_grid_size = 32
plot_file = plt_test
