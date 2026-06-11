# Solver Enhancement Regression Test: EB Solid Cell Masking
#
# This test validates Embedded Boundary (EB) solid cell masking capability in the wind solver.
# It places a box geometry inside the domain using AMReX EB2 and verifies that velocity 
# components and RHS are correctly zeroed inside the solid cells, and the solver projects 
# the flow correctly around the solid obstacle.

# Domain and grid
nx = 20
ny = 20
nz = 10
x_lo = 0.0
x_hi = 200.0
y_lo = 0.0
y_hi = 200.0
z_lo = 0.0
z_hi = 100.0

# Grid spacing
dx = 10.0
dy = 10.0
dz = 10.0

# Wind initialization (uniform)
init_mode = uniform
uniform_U = 10.0
uniform_V = 0.0
z_ref = 10.0
z0 = 0.1

# Terrain (flat)
terrain_file = terrain.csv

# Solver options
alpha_h = 1.0
alpha_v = 1.0
mlmg_verbose = 1
tol_rel = 1.0e-8
max_grid_size = 32

# EB parameters: let's build a box geometry
enable_eb = true
eb_threshold = 0.5

eb2.geom_type = box
eb2.box_lo = 60.0 60.0 0.0
eb2.box_hi = 140.0 140.0 40.0
eb2.box_has_fluid_inside = false

# Extraction inside the EB box (at 20m height, which is inside the box)
extract_agl = 20.0
extract_file = eb_extract.csv

# Output
plot_file = plt_eb_solid
