# Gaussian hill test case for OpenFAST export validation
# This case tests the wind field export to TurbSim/OpenFAST format over complex terrain

terrain_file = regtest_terrain.csv
init_mode = log_law
U_ref = 10.0
V_ref = 0.0
z_ref = 10.0
z0 = 0.1
dx = 25.0
dy = 25.0
dz = 20.0
domain_height = 300.0
alpha_h = 1.0
alpha_v = 1.0
mlmg_verbose = 0
tol_rel = 1.e-8
max_iters = 100
