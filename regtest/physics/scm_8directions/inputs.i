# Regression test template for SCM wind-direction conversion

geometry.prob_lo = 0.0 0.0 0.0
geometry.prob_hi = 1000.0 1000.0 1024.0
geometry.is_periodic = 1 1 0

amr.n_cell = 16 16 256
amr.max_level = 0

init_mode = scm

scm_wind_speed = 8.0
scm_wind_direction = 0.0
scm_ref_height = 80.0
scm_ref_temperature = 288.15
scm_lapse_rate = 0.0065
scm_domain_height = 1024.0
scm_dz = 4.0

z0 = 0.1
latitude = 45.0

enable_3d_scalars = true
enable_temperature_transport = true

mlmg.verbose = 0
mlmg.max_iter = 100
mlmg.tol_rel = 1.0e-6
mlmg.tol_abs = 1.0e-8

plot_file = plt_scm_8directions
extract_agl = 80.0
