// pyWindSolver.cpp - Python bindings for massconsistent_amr wind solver
// 
// Provides Python interface to control the mass-consistent wind solver from Python,
// enabling coupled simulations with external fire solvers (e.g., wildfire_levelset) without disk I/O.
//
// Build with: cmake -S . -B build -DMASSCONSISTENT_BUILD_PYTHON_BINDINGS=ON
//             cmake --build build
//
// Usage from Python:
//   import pyWindSolver
//   result = pyWindSolver.initialize("inputs.i")
//   pyWindSolver.solve()
//   velocity = pyWindSolver.get_velocity()
//   pyWindSolver.finalize()

#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>

#include <AMReX.H>
#include <AMReX_Print.H>

// Include the wind solver API
#include "wind_solver_api.H"

namespace py = pybind11;

// ============================================================================
// Wrapper functions for wind solver API
// ============================================================================

py::dict wind_solver_init_py(const std::string& inputs_file) {
    bool success = wind_solver_initialize(inputs_file);
    
    py::dict result;
    result["success"] = success;
    
    if (success) {
        int nx, ny, nz;
        double xmin, xmax, ymin, ymax, zmin, zmax, dx, dy, dz;
        wind_solver_get_geometry(nx, ny, nz, xmin, xmax, ymin, ymax, zmin, zmax, dx, dy, dz);
        
        result["nx"] = nx;
        result["ny"] = ny;
        result["nz"] = nz;
        result["xmin"] = xmin;
        result["xmax"] = xmax;
        result["ymin"] = ymin;
        result["ymax"] = ymax;
        result["zmin"] = zmin;
        result["zmax"] = zmax;
        result["dx"] = dx;
        result["dy"] = dy;
        result["dz"] = dz;
    }
    
    return result;
}

py::dict wind_solver_solve_py() {
    bool success = wind_solver_solve();
    
    bool solved;
    int iters;
    double residual;
    wind_solver_get_status(solved, iters, residual);
    
    py::dict result;
    result["success"] = success;
    result["solved"] = solved;
    result["iters"] = iters;
    result["residual"] = residual;
    
    return result;
}

py::dict wind_solver_get_status_py() {
    bool solved;
    int iters;
    double residual;
    wind_solver_get_status(solved, iters, residual);
    
    py::dict result;
    result["solved"] = solved;
    result["iters"] = iters;
    result["residual"] = residual;
    
    return result;
}

py::dict wind_solver_get_geometry_py() {
    int nx, ny, nz;
    double xmin, xmax, ymin, ymax, zmin, zmax, dx, dy, dz;
    wind_solver_get_geometry(nx, ny, nz, xmin, xmax, ymin, ymax, zmin, zmax, dx, dy, dz);
    
    py::dict result;
    result["nx"] = nx;
    result["ny"] = ny;
    result["nz"] = nz;
    result["xmin"] = xmin;
    result["xmax"] = xmax;
    result["ymin"] = ymin;
    result["ymax"] = ymax;
    result["zmin"] = zmin;
    result["zmax"] = zmax;
    result["dx"] = dx;
    result["dy"] = dy;
    result["dz"] = dz;
    
    return result;
}

py::dict wind_solver_get_terrain_bounds_py() {
    double zs_min, zs_max;
    wind_solver_get_terrain_bounds(zs_min, zs_max);
    
    py::dict result;
    result["zs_min"] = zs_min;
    result["zs_max"] = zs_max;
    
    return result;
}

py::dict wind_solver_get_velocity_py() {
    if (!wind_solver_is_initialized()) {
        throw std::runtime_error("Wind solver not initialized");
    }
    
    int nx, ny, nz;
    double xmin, xmax, ymin, ymax, zmin, zmax, dx, dy, dz;
    wind_solver_get_geometry(nx, ny, nz, xmin, xmax, ymin, ymax, zmin, zmax, dx, dy, dz);
    
    std::vector<double> u_vec, v_vec, w_vec;
    wind_solver_get_velocity(u_vec, v_vec, w_vec);
    
    // Convert to numpy arrays (shape: nz, ny, nx in Fortran order)
    // Data is already in Fortran (column-major) order from the C++ API
    auto u_np = py::array_t<double>({nz, ny, nx});
    auto v_np = py::array_t<double>({nz, ny, nx});
    auto w_np = py::array_t<double>({nz, ny, nx});
    
    auto u_buf = u_np.mutable_unchecked<3>();
    auto v_buf = v_np.mutable_unchecked<3>();
    auto w_buf = w_np.mutable_unchecked<3>();
    
    for (int k = 0; k < nz; ++k) {
        for (int j = 0; j < ny; ++j) {
            for (int i = 0; i < nx; ++i) {
                int idx = i + nx * (j + ny * k);  // Fortran order
                u_buf(k, j, i) = u_vec[idx];
                v_buf(k, j, i) = v_vec[idx];
                w_buf(k, j, i) = w_vec[idx];
            }
        }
    }
    
    py::dict result;
    result["u"] = u_np;
    result["v"] = v_np;
    result["w"] = w_np;
    
    return result;
}

py::dict wind_solver_get_velocity0_py() {
    if (!wind_solver_is_initialized()) {
        throw std::runtime_error("Wind solver not initialized");
    }
    
    int nx, ny, nz;
    double xmin, xmax, ymin, ymax, zmin, zmax, dx, dy, dz;
    wind_solver_get_geometry(nx, ny, nz, xmin, xmax, ymin, ymax, zmin, zmax, dx, dy, dz);
    
    std::vector<double> u_vec, v_vec, w_vec;
    wind_solver_get_velocity0(u_vec, v_vec, w_vec);
    
    // Convert to numpy arrays
    auto u_np = py::array_t<double>({nz, ny, nx});
    auto v_np = py::array_t<double>({nz, ny, nx});
    auto w_np = py::array_t<double>({nz, ny, nx});
    
    auto u_buf = u_np.mutable_unchecked<3>();
    auto v_buf = v_np.mutable_unchecked<3>();
    auto w_buf = w_np.mutable_unchecked<3>();
    
    for (int k = 0; k < nz; ++k) {
        for (int j = 0; j < ny; ++j) {
            for (int i = 0; i < nx; ++i) {
                int idx = i + nx * (j + ny * k);  // Fortran order
                u_buf(k, j, i) = u_vec[idx];
                v_buf(k, j, i) = v_vec[idx];
                w_buf(k, j, i) = w_vec[idx];
            }
        }
    }
    
    py::dict result;
    result["u"] = u_np;
    result["v"] = v_np;
    result["w"] = w_np;
    
    return result;
}

py::array_t<double> wind_solver_get_lambda_py() {
    if (!wind_solver_is_initialized()) {
        throw std::runtime_error("Wind solver not initialized");
    }
    
    int nx, ny, nz;
    double xmin, xmax, ymin, ymax, zmin, zmax, dx, dy, dz;
    wind_solver_get_geometry(nx, ny, nz, xmin, xmax, ymin, ymax, zmin, zmax, dx, dy, dz);
    
    std::vector<double> lambda_vec = wind_solver_get_lambda();
    
    auto lambda_np = py::array_t<double>({nz, ny, nx});
    auto lambda_buf = lambda_np.mutable_unchecked<3>();
    
    for (int k = 0; k < nz; ++k) {
        for (int j = 0; j < ny; ++j) {
            for (int i = 0; i < nx; ++i) {
                int idx = i + nx * (j + ny * k);  // Fortran order
                lambda_buf(k, j, i) = lambda_vec[idx];
            }
        }
    }
    
    return lambda_np;
}

py::array_t<double> wind_solver_get_div0_py() {
    if (!wind_solver_is_initialized()) {
        throw std::runtime_error("Wind solver not initialized");
    }
    
    int nx, ny, nz;
    double xmin, xmax, ymin, ymax, zmin, zmax, dx, dy, dz;
    wind_solver_get_geometry(nx, ny, nz, xmin, xmax, ymin, ymax, zmin, zmax, dx, dy, dz);
    
    std::vector<double> div0_vec = wind_solver_get_div0();
    
    auto div0_np = py::array_t<double>({nz, ny, nx});
    auto div0_buf = div0_np.mutable_unchecked<3>();
    
    for (int k = 0; k < nz; ++k) {
        for (int j = 0; j < ny; ++j) {
            for (int i = 0; i < nx; ++i) {
                int idx = i + nx * (j + ny * k);  // Fortran order
                div0_buf(k, j, i) = div0_vec[idx];
            }
        }
    }
    
    return div0_np;
}

py::array_t<double> wind_solver_get_terrain_py() {
    if (!wind_solver_is_initialized()) {
        throw std::runtime_error("Wind solver not initialized");
    }
    
    int nx, ny, nz;
    double xmin, xmax, ymin, ymax, zmin, zmax, dx, dy, dz;
    wind_solver_get_geometry(nx, ny, nz, xmin, xmax, ymin, ymax, zmin, zmax, dx, dy, dz);
    
    std::vector<double> terrain_vec = wind_solver_get_terrain();
    
    auto terrain_np = py::array_t<double>({ny, nx});
    auto terrain_buf = terrain_np.mutable_unchecked<2>();
    
    for (int j = 0; j < ny; ++j) {
        for (int i = 0; i < nx; ++i) {
            int idx = i + nx * j;  // Fortran order
            terrain_buf(j, i) = terrain_vec[idx];
        }
    }
    
    return terrain_np;
}

py::dict wind_solver_get_velocity_at_agl_py(double agl_height) {
    if (!wind_solver_is_initialized()) {
        throw std::runtime_error("Wind solver not initialized");
    }
    
    int nx, ny, nz;
    double xmin, xmax, ymin, ymax, zmin, zmax, dx, dy, dz;
    wind_solver_get_geometry(nx, ny, nz, xmin, xmax, ymin, ymax, zmin, zmax, dx, dy, dz);
    
    std::vector<double> u_vec, v_vec, w_vec;
    wind_solver_get_velocity_at_agl(agl_height, u_vec, v_vec, w_vec);
    
    auto u_np = py::array_t<double>({ny, nx});
    auto v_np = py::array_t<double>({ny, nx});
    auto w_np = py::array_t<double>({ny, nx});
    
    auto u_buf = u_np.mutable_unchecked<2>();
    auto v_buf = v_np.mutable_unchecked<2>();
    auto w_buf = w_np.mutable_unchecked<2>();
    
    for (int j = 0; j < ny; ++j) {
        for (int i = 0; i < nx; ++i) {
            int idx = i + nx * j;  // Fortran order
            u_buf(j, i) = u_vec[idx];
            v_buf(j, i) = v_vec[idx];
            w_buf(j, i) = w_vec[idx];
        }
    }
    
    py::dict result;
    result["u"] = u_np;
    result["v"] = v_np;
    result["w"] = w_np;
    result["agl"] = agl_height;
    
    return result;
}

py::dict wind_solver_get_velocity_at_k_py(int k) {
    if (!wind_solver_is_initialized()) {
        throw std::runtime_error("Wind solver not initialized");
    }
    
    int nx, ny, nz;
    double xmin, xmax, ymin, ymax, zmin, zmax, dx, dy, dz;
    wind_solver_get_geometry(nx, ny, nz, xmin, xmax, ymin, ymax, zmin, zmax, dx, dy, dz);
    
    std::vector<double> u_vec, v_vec, w_vec;
    wind_solver_get_velocity_at_k(k, u_vec, v_vec, w_vec);
    
    auto u_np = py::array_t<double>({ny, nx});
    auto v_np = py::array_t<double>({ny, nx});
    auto w_np = py::array_t<double>({ny, nx});
    
    auto u_buf = u_np.mutable_unchecked<2>();
    auto v_buf = v_np.mutable_unchecked<2>();
    auto w_buf = w_np.mutable_unchecked<2>();
    
    for (int j = 0; j < ny; ++j) {
        for (int i = 0; i < nx; ++i) {
            int idx = i + nx * j;  // Fortran order
            u_buf(j, i) = u_vec[idx];
            v_buf(j, i) = v_vec[idx];
            w_buf(j, i) = w_vec[idx];
        }
    }
    
    py::dict result;
    result["u"] = u_np;
    result["v"] = v_np;
    result["w"] = w_np;
    result["k"] = k;
    
    return result;
}

bool wind_solver_update_reference_wind_py(double U_ref, double V_ref) {
    return wind_solver_update_reference_wind(U_ref, V_ref);
}

bool wind_solver_update_parameters_py(double alpha_h, double alpha_v, double tol_rel, int max_iter) {
    return wind_solver_update_parameters(alpha_h, alpha_v, tol_rel, max_iter);
}

bool wind_solver_write_plotfile_py(const std::string& plotfile_name) {
    return wind_solver_write_plotfile(plotfile_name);
}

bool wind_solver_write_extract_py(const std::string& extract_filename, double agl_height) {
    return wind_solver_write_extract(extract_filename, agl_height);
}

void wind_solver_finalize_py() {
    wind_solver_finalize();
}

bool wind_solver_is_initialized_py() {
    return wind_solver_is_initialized();
}

bool wind_solver_add_turbine_py(double x, double y, double hub_height, double rotor_diameter, double default_ct, const std::string& power_curve_file, double yaw, double orientation, double tilt) {
    return wind_solver_add_turbine(x, y, hub_height, rotor_diameter, default_ct, power_curve_file, yaw, orientation, tilt);
}

void wind_solver_clear_turbines_py() {
    wind_solver_clear_turbines();
}

std::vector<double> wind_solver_get_turbine_power_outputs_py() {
    return wind_solver_get_turbine_power_outputs();
}

std::vector<double> wind_solver_get_turbine_inflow_speeds_py() {
    return wind_solver_get_turbine_inflow_speeds();
}

std::vector<double> wind_solver_get_turbine_yaws_py() {
    return wind_solver_get_turbine_yaws();
}

std::vector<double> wind_solver_get_turbine_orientations_py() {
    return wind_solver_get_turbine_orientations();
}

std::vector<double> wind_solver_get_turbine_tilts_py() {
    return wind_solver_get_turbine_tilts();
}

std::vector<double> wind_solver_get_turbine_u_hubs_py() {
    return wind_solver_get_turbine_u_hubs();
}

std::vector<double> wind_solver_get_turbine_v_hubs_py() {
    return wind_solver_get_turbine_v_hubs();
}

std::vector<double> wind_solver_get_turbine_z_terrains_py() {
    return wind_solver_get_turbine_z_terrains();
}

// ============================================================================
// Heat source wrapper functions for fire coupling
// ============================================================================

py::dict wind_solver_add_heat_source_py(
    py::array_t<double> heat_flux,
    double scaling_factor = 1.0)
{
    if (!wind_solver_is_initialized()) {
        throw std::runtime_error("Wind solver not initialized");
    }
    
    // Get numpy array data
    auto buf = heat_flux.request();
    if (buf.ndim != 2) {
        throw std::runtime_error("heat_flux must be a 2D array");
    }
    
    int ny = buf.shape[0];
    int nx = buf.shape[1];
    std::vector<double> heat_flux_vec(static_cast<double*>(buf.ptr), 
                                       static_cast<double*>(buf.ptr) + nx * ny);
    
    bool success = wind_solver_add_heat_source(heat_flux_vec, nx, ny, scaling_factor);
    
    py::dict result;
    result["success"] = success;
    result["nx"] = nx;
    result["ny"] = ny;
    result["scaling_factor"] = scaling_factor;
    
    return result;
}

py::dict wind_solver_get_heat_source_py()
{
    auto [heat_flux_vec, is_active] = wind_solver_get_heat_source();
    
    // Get geometry to determine shape
    int nx, ny, nz;
    double xmin, xmax, ymin, ymax, zmin, zmax, dx, dy, dz;
    wind_solver_get_geometry(nx, ny, nz, xmin, xmax, ymin, ymax, zmin, zmax, dx, dy, dz);
    
    // Convert vector to numpy array
    auto heat_flux_np = py::array_t<double>({ny, nx});
    auto buf = heat_flux_np.mutable_unchecked<2>();
    
    for (int j = 0; j < ny; ++j) {
        for (int i = 0; i < nx; ++i) {
            int idx = j * nx + i;
            buf(j, i) = heat_flux_vec[idx];
        }
    }
    
    py::dict result;
    result["heat_flux"] = heat_flux_np;
    result["is_active"] = is_active;
    
    return result;
}

void wind_solver_clear_heat_source_py()
{
    wind_solver_clear_heat_source();
}

// ============================================================================
// Module definition
// ============================================================================

PYBIND11_MODULE(pyWindSolver, m) {
    m.doc() = "Python bindings for massconsistent_amr: mass-consistent wind solver control";

    // Initialization and solving
    m.def("initialize", &wind_solver_init_py,
          py::arg("inputs_file"),
          R"pbdoc(
        Initialize the mass-consistent wind solver from an inputs file.

        Parameters
        ----------
        inputs_file : str
            Path to the inputs file (e.g., "inputs.i")

        Returns
        -------
        dict
            Dictionary with 'success' boolean and geometry information (nx, ny, nz, bounds, cell sizes)
      )pbdoc");

    m.def("solve", &wind_solver_solve_py,
          R"pbdoc(
        Solve for the mass-consistent wind field.

        Returns
        -------
        dict
            Dictionary with 'success', 'solved', 'iters', and 'residual'
      )pbdoc");

    m.def("get_status", &wind_solver_get_status_py,
          R"pbdoc(
        Get solver status.

        Returns
        -------
        dict
            Dictionary with 'solved' (bool), 'iters' (int), and 'residual' (float)
      )pbdoc");

    // Geometry queries
    m.def("get_geometry", &wind_solver_get_geometry_py,
          R"pbdoc(
        Get grid geometry information.

        Returns
        -------
        dict
            Dictionary with grid dimensions (nx, ny, nz), domain bounds, and cell sizes
      )pbdoc");

    m.def("get_terrain_bounds", &wind_solver_get_terrain_bounds_py,
          R"pbdoc(
        Get terrain elevation bounds.

        Returns
        -------
        dict
            Dictionary with 'zs_min' and 'zs_max' (minimum and maximum terrain elevation)
      )pbdoc");

    // Data extraction
    m.def("get_velocity", &wind_solver_get_velocity_py,
          R"pbdoc(
        Extract the corrected mass-consistent velocity field.

        Returns
        -------
        dict
            Dictionary with 'u', 'v', 'w' numpy arrays (shape: nz, ny, nx)
      )pbdoc");

    m.def("get_velocity0", &wind_solver_get_velocity0_py,
          R"pbdoc(
        Extract the initial (uncorrected) velocity field.

        Returns
        -------
        dict
            Dictionary with 'u', 'v', 'w' numpy arrays (shape: nz, ny, nx)
      )pbdoc");

    m.def("get_lambda", &wind_solver_get_lambda_py,
          R"pbdoc(
        Extract the Lagrange multiplier field.

        Returns
        -------
        numpy.ndarray
            3D array (shape: nz, ny, nx) of Lagrange multiplier values
      )pbdoc");

    m.def("get_div0", &wind_solver_get_div0_py,
          R"pbdoc(
        Extract the divergence of the initial velocity field.

        Returns
        -------
        numpy.ndarray
            3D array (shape: nz, ny, nx) of divergence values
      )pbdoc");

    m.def("get_terrain", &wind_solver_get_terrain_py,
          R"pbdoc(
        Extract the terrain elevation field.

        Returns
        -------
        numpy.ndarray
            2D array (shape: ny, nx) of terrain elevations
      )pbdoc");

    m.def("get_velocity_at_agl", &wind_solver_get_velocity_at_agl_py,
          py::arg("agl_height"),
          R"pbdoc(
        Extract velocity at a specific height above ground level (AGL).

        Parameters
        ----------
        agl_height : float
            Height above ground level in meters

        Returns
        -------
        dict
            Dictionary with 'u', 'v', 'w' numpy arrays (shape: ny, nx) and 'agl'
      )pbdoc");

    m.def("get_velocity_at_k", &wind_solver_get_velocity_at_k_py,
          py::arg("k"),
          R"pbdoc(
        Extract velocity at a specific k-index (vertical level).

        Parameters
        ----------
        k : int
            Vertical level index (0 = lowest level)

        Returns
        -------
        dict
            Dictionary with 'u', 'v', 'w' numpy arrays (shape: ny, nx) and 'k'
      )pbdoc");

    // Parameter updates
    m.def("update_reference_wind", &wind_solver_update_reference_wind_py,
          py::arg("U_ref"), py::arg("V_ref"),
          R"pbdoc(
        Update the reference wind and re-initialize the velocity field.

        Parameters
        ----------
        U_ref : float
            Reference wind x-component (m/s)
        V_ref : float
            Reference wind y-component (m/s)

        Returns
        -------
        bool
            True on success
      )pbdoc");

    m.def("update_parameters", &wind_solver_update_parameters_py,
          py::arg("alpha_h"), py::arg("alpha_v"), py::arg("tol_rel"), py::arg("max_iter"),
          R"pbdoc(
        Update solver parameters (anisotropy factors, tolerances).

        Parameters
        ----------
        alpha_h : float
            Horizontal anisotropy factor
        alpha_v : float
            Vertical anisotropy factor
        tol_rel : float
            Relative tolerance for MLMG solver
        max_iter : int
            Maximum iterations for MLMG solver

        Returns
        -------
        bool
            True on success
      )pbdoc");

    // Output
    m.def("write_plotfile", &wind_solver_write_plotfile_py,
          py::arg("plotfile_name"),
          R"pbdoc(
        Write AMReX plotfile.

        Parameters
        ----------
        plotfile_name : str
            Plotfile name/prefix

        Returns
        -------
        bool
            True on success
      )pbdoc");

    m.def("write_extract", &wind_solver_write_extract_py,
          py::arg("extract_filename"), py::arg("agl_height"),
          R"pbdoc(
        Write terrain-aligned CSV extract at specified AGL height.

        Parameters
        ----------
        extract_filename : str
            Output CSV filename
        agl_height : float
            Height above ground level in meters

        Returns
        -------
        bool
            True on success
      )pbdoc");

    // Cleanup
    m.def("finalize", &wind_solver_finalize_py,
          R"pbdoc(
        Clean up and finalize the wind solver.
      )pbdoc");

    m.def("is_initialized", &wind_solver_is_initialized_py,
          R"pbdoc(
        Check if the wind solver is initialized.

        Returns
        -------
        bool
            True if initialized, False otherwise
      )pbdoc");

    // Turbine API
    m.def("add_turbine", &wind_solver_add_turbine_py,
          py::arg("x"), py::arg("y"), py::arg("hub_height"), py::arg("rotor_diameter"),
          py::arg("default_ct") = 0.8, py::arg("power_curve_file") = "",
          py::arg("yaw") = 0.0, py::arg("orientation") = 0.0, py::arg("tilt") = 0.0,
          R"pbdoc(
        Add a wind turbine to the solver.
      )pbdoc");

    m.def("clear_turbines", &wind_solver_clear_turbines_py,
          R"pbdoc(
        Clear all wind turbines.
      )pbdoc");

    m.def("get_turbine_power_outputs", &wind_solver_get_turbine_power_outputs_py,
          R"pbdoc(
        Get power output of all turbines.
      )pbdoc");

    m.def("get_turbine_inflow_speeds", &wind_solver_get_turbine_inflow_speeds_py,
          R"pbdoc(
        Get inflow wind speed at all turbines.
      )pbdoc");

    m.def("get_turbine_yaws", &wind_solver_get_turbine_yaws_py,
          R"pbdoc(
        Get yaw angle of all turbines.
      )pbdoc");

    m.def("get_turbine_orientations", &wind_solver_get_turbine_orientations_py,
          R"pbdoc(
        Get orientation of all turbines.
      )pbdoc");

    m.def("get_turbine_tilts", &wind_solver_get_turbine_tilts_py,
          R"pbdoc(
        Get tilt angle of all turbines.
      )pbdoc");

    m.def("get_turbine_u_hubs", &wind_solver_get_turbine_u_hubs_py,
          R"pbdoc(
        Get turbine u_hub components.
      )pbdoc");

    m.def("get_turbine_v_hubs", &wind_solver_get_turbine_v_hubs_py,
          R"pbdoc(
        Get turbine v_hub components.
      )pbdoc");

    m.def("get_turbine_z_terrains", &wind_solver_get_turbine_z_terrains_py,
          R"pbdoc(
        Get turbine terrain elevations.
      )pbdoc");

    // Heat source API for fire coupling
    m.def("add_heat_source", &wind_solver_add_heat_source_py,
          py::arg("heat_flux"), py::arg("scaling_factor") = 1.0,
          R"pbdoc(
        Add a 2D heat source (surface heat flux from fire solver) for two-way coupling.
        
        Parameters
        ----------
        heat_flux : np.ndarray
            2D array of surface heat flux with shape (ny, nx) in W/m².
        scaling_factor : float, optional
            Multiplier for unit conversion (default: 1.0)
        
        Returns
        -------
        dict
            Dictionary with 'success', 'nx', 'ny', 'scaling_factor'
      )pbdoc");

    m.def("get_heat_source", &wind_solver_get_heat_source_py,
          R"pbdoc(
        Get the currently stored heat source (if any).
        
        Returns
        -------
        dict
            Dictionary with:
            - 'heat_flux': 2D array of heat flux or empty array if not set
            - 'is_active': Boolean indicating if heat source is active
      )pbdoc");

    m.def("clear_heat_source", &wind_solver_clear_heat_source_py,
          R"pbdoc(
        Clear any stored heat source.
      )pbdoc");
}
