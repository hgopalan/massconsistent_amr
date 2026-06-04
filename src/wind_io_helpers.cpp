#include "wind_io_helpers.H"
#include "math_constants.H"
#include <AMReX.H>
#include <AMReX_Print.H>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace WindIO {

// Read an X Y Z terrain file (whitespace or comma separated; '#' comments).
void read_terrain_file(const std::string& filename,
                       std::vector<Real>& xd,
                       std::vector<Real>& yd,
                       std::vector<Real>& zd)
{
    std::ifstream f(filename);
    if (!f.is_open())
        amrex::Abort("wind_solver: cannot open terrain file: " + filename);

    std::string line;
    while (std::getline(f, line)) {
        // strip comments
        auto pos = line.find('#');
        if (pos != std::string::npos) line = line.substr(0, pos);
        // replace commas with spaces
        std::replace(line.begin(), line.end(), ',', ' ');
        std::istringstream ss(line);
        Real x, y, z;
        if (ss >> x >> y >> z) {
            xd.push_back(x);
            yd.push_back(y);
            zd.push_back(z);
        }
    }
    if (xd.empty())
        amrex::Abort("wind_solver: no data read from terrain file: " + filename);

    amrex::Print() << "wind_solver: read " << xd.size()
                   << " terrain points from " << filename << "\n";
}

// Read X Y Z U V velocity file (whitespace or comma separated; '#' comments).
// Used for RAWS or synthetic wind data initialization.
void read_velocity_file(const std::string& filename,
                        std::vector<Real>& xd,
                        std::vector<Real>& yd,
                        std::vector<Real>& zd,
                        std::vector<Real>& ux,
                        std::vector<Real>& uy)
{
    std::ifstream f(filename);
    if (!f.is_open())
        amrex::Abort("wind_solver: cannot open velocity file: " + filename);

    std::string line;
    while (std::getline(f, line)) {
        // strip comments
        auto pos = line.find('#');
        if (pos != std::string::npos) line = line.substr(0, pos);
        // replace commas with spaces
        std::replace(line.begin(), line.end(), ',', ' ');
        std::istringstream ss(line);
        Real x, y, z, u_x, u_y;
        if (ss >> x >> y >> z >> u_x >> u_y) {
            xd.push_back(x);
            yd.push_back(y);
            zd.push_back(z);
            ux.push_back(u_x);
            uy.push_back(u_y);
        }
    }
    if (xd.empty())
        amrex::Abort("wind_solver: no data read from velocity file: " + filename);

    amrex::Print() << "wind_solver: read " << xd.size()
                   << " velocity points from " << filename << "\n";
}

// Read X Y Z0 roughness file (whitespace or comma separated; '#' comments).
// Format: X Y Z0
// where Z0 = aerodynamic roughness length [m]
void read_roughness_file(const std::string& filename,
                         std::vector<Real>& xd,
                         std::vector<Real>& yd,
                         std::vector<Real>& z0_d)
{
    std::ifstream f(filename);
    if (!f.is_open())
        amrex::Abort("wind_solver: cannot open roughness file: " + filename);

    std::string line;
    while (std::getline(f, line)) {
        // strip comments
        auto pos = line.find('#');
        if (pos != std::string::npos) line = line.substr(0, pos);
        // replace commas with spaces
        std::replace(line.begin(), line.end(), ',', ' ');
        std::istringstream ss(line);
        Real x, y, z0;
        if (ss >> x >> y >> z0) {
            xd.push_back(x);
            yd.push_back(y);
            z0_d.push_back(z0);
        }
    }
    if (xd.empty())
        amrex::Abort("wind_solver: no data read from roughness file: " + filename);

    amrex::Print() << "wind_solver: read " << xd.size()
                   << " roughness points from " << filename << "\n";
}

// Read X Y Z USTAR Z0 U10 V10 surface data file (whitespace or comma separated; '#' comments).
// Used for HRRR-style surface parameters with per-column friction velocity and roughness.
// Format: X Y Z USTAR Z0 U10 V10
// where USTAR = friction velocity [m/s], Z0 = roughness length [m], U10/V10 = 10m wind [m/s]
void read_surface_data_file(const std::string& filename,
                            std::vector<Real>& xd,
                            std::vector<Real>& yd,
                            std::vector<Real>& zd,
                            std::vector<Real>& ustar_d,
                            std::vector<Real>& z0_d,
                            std::vector<Real>& u10_d,
                            std::vector<Real>& v10_d)
{
    std::ifstream f(filename);
    if (!f.is_open())
        amrex::Abort("wind_solver: cannot open surface data file: " + filename);

    std::string line;
    while (std::getline(f, line)) {
        // strip comments
        auto pos = line.find('#');
        if (pos != std::string::npos) line = line.substr(0, pos);
        // replace commas with spaces
        std::replace(line.begin(), line.end(), ',', ' ');
        std::istringstream ss(line);
        Real x, y, z, ustar, z0, u10, v10;
        if (ss >> x >> y >> z >> ustar >> z0 >> u10 >> v10) {
            xd.push_back(x);
            yd.push_back(y);
            zd.push_back(z);
            ustar_d.push_back(ustar);
            z0_d.push_back(z0);
            u10_d.push_back(u10);
            v10_d.push_back(v10);
        }
    }
    if (xd.empty())
        amrex::Abort("wind_solver: no data read from surface data file: " + filename);

    amrex::Print() << "wind_solver: read " << xd.size()
                   << " surface data points from " << filename << "\n";
}

// Read Z T temperature profile file (whitespace or comma separated; '#' comments).
// Format: Z T
// where Z = height above sea level [m], T = temperature [K]
void read_temperature_file(const std::string& filename,
                           std::vector<Real>& zd,
                           std::vector<Real>& Td)
{
    std::ifstream f(filename);
    if (!f.is_open())
        amrex::Abort("wind_solver: cannot open temperature file: " + filename);

    std::string line;
    while (std::getline(f, line)) {
        // strip comments
        auto pos = line.find('#');
        if (pos != std::string::npos) line = line.substr(0, pos);
        // replace commas with spaces
        std::replace(line.begin(), line.end(), ',', ' ');
        std::istringstream ss(line);
        Real z, T;
        if (ss >> z >> T) {
            zd.push_back(z);
            Td.push_back(T);
        }
    }
    if (zd.empty())
        amrex::Abort("wind_solver: no data read from temperature file: " + filename);

    amrex::Print() << "wind_solver: read " << zd.size()
                   << " temperature profile points from " << filename << "\n";
}

// Read X Y ALPHA_H ALPHA_V file (whitespace or comma separated; '#' comments).
// Format: X Y ALPHA_H ALPHA_V
// where X, Y = coordinates [m], ALPHA_H, ALPHA_V = Lagrange coefficients (dimensionless)
void read_alpha_coefficients_file(const std::string& filename,
                                  std::vector<Real>& xd,
                                  std::vector<Real>& yd,
                                  std::vector<Real>& alpha_h_data,
                                  std::vector<Real>& alpha_v_data)
{
    std::ifstream f(filename);
    if (!f.is_open())
        amrex::Abort("wind_solver: cannot open alpha coefficients file: " + filename);

    std::string line;
    while (std::getline(f, line)) {
        // strip comments
        auto pos = line.find('#');
        if (pos != std::string::npos) line = line.substr(0, pos);
        // replace commas with spaces
        std::replace(line.begin(), line.end(), ',', ' ');
        std::istringstream ss(line);
        Real x, y, ah, av;
        if (ss >> x >> y >> ah >> av) {
            xd.push_back(x);
            yd.push_back(y);
            alpha_h_data.push_back(ah);
            alpha_v_data.push_back(av);
        }
    }
    if (xd.empty())
        amrex::Abort("wind_solver: no data read from alpha coefficients file: " + filename);

    amrex::Print() << "wind_solver: read " << xd.size()
                   << " alpha coefficient points from " << filename << "\n";
}

// Read building file: xmin xmax ymin ymax zmin zmax (whitespace or comma separated; '#' comments).
void read_building_file(const std::string& filename,
                        std::vector<Real>& xmin,
                        std::vector<Real>& xmax,
                        std::vector<Real>& ymin,
                        std::vector<Real>& ymax,
                        std::vector<Real>& zmin,
                        std::vector<Real>& zmax,
                        std::vector<Real>& rotation)
{
    std::ifstream f(filename);
    if (!f.is_open())
        amrex::Abort("wind_solver: cannot open building file: " + filename);

    std::string line;
    while (std::getline(f, line)) {
        // strip comments
        auto pos = line.find('#');
        if (pos != std::string::npos) line = line.substr(0, pos);
        // replace commas with spaces
        std::replace(line.begin(), line.end(), ',', ' ');
        std::istringstream ss(line);
        Real x1, x2, y1, y2, z1, z2, angle = 0.0;
        if (ss >> x1 >> x2 >> y1 >> y2 >> z1 >> z2) {
            // Phase 3 Enhancement: Optional rotation angle (7th column, in degrees)
            // If provided, angle is converted from degrees to radians
            if (ss >> angle) {
                angle = angle * MathConstants::deg_to_rad;
            }
            xmin.push_back(x1);
            xmax.push_back(x2);
            ymin.push_back(y1);
            ymax.push_back(y2);
            zmin.push_back(z1);
            zmax.push_back(z2);
            rotation.push_back(angle);
        }
    }
    if (xmin.empty())
        amrex::Abort("wind_solver: no data read from building file: " + filename);

    amrex::Print() << "wind_solver: read " << xmin.size()
                   << " building(s) from " << filename << "\n";
}

// Read porous building file: xmin xmax ymin ymax zmin zmax porosity [rotation_angle]
// (whitespace or comma separated; '#' comments).
void read_porous_building_file(const std::string& filename,
                               std::vector<Real>& xmin,
                               std::vector<Real>& xmax,
                               std::vector<Real>& ymin,
                               std::vector<Real>& ymax,
                               std::vector<Real>& zmin,
                               std::vector<Real>& zmax,
                               std::vector<Real>& porosity,
                               std::vector<Real>& rotation)
{
    std::ifstream f(filename);
    if (!f.is_open())
        amrex::Abort("wind_solver: cannot open porous building file: " + filename);

    std::string line;
    while (std::getline(f, line)) {
        // strip comments
        auto pos = line.find('#');
        if (pos != std::string::npos) line = line.substr(0, pos);
        // replace commas with spaces
        std::replace(line.begin(), line.end(), ',', ' ');
        std::istringstream ss(line);
        Real x1, x2, y1, y2, z1, z2, por = 0.0, angle = 0.0;
        if (ss >> x1 >> x2 >> y1 >> y2 >> z1 >> z2 >> por) {
            // Optional rotation angle (8th column, in degrees)
            if (ss >> angle) {
                angle = angle * MathConstants::deg_to_rad;
            }
            xmin.push_back(x1);
            xmax.push_back(x2);
            ymin.push_back(y1);
            ymax.push_back(y2);
            zmin.push_back(z1);
            zmax.push_back(z2);
            porosity.push_back(por);
            rotation.push_back(angle);
        }
    }
    if (xmin.empty())
        amrex::Abort("wind_solver: no data read from porous building file: " + filename);

    amrex::Print() << "wind_solver: read " << xmin.size()
                   << " porous building(s) from " << filename << "\n";
}

// Read windbreaks file: x1 y1 x2 y2 height blockage drag_coeff
// (whitespace or comma separated; '#' comments).
void read_windbreaks_file(const std::string& filename,
                          std::vector<Real>& x1,
                          std::vector<Real>& y1,
                          std::vector<Real>& x2,
                          std::vector<Real>& y2,
                          std::vector<Real>& height,
                          std::vector<Real>& blockage,
                          std::vector<Real>& drag_coeff)
{
    std::ifstream f(filename);
    if (!f.is_open())
        amrex::Abort("wind_solver: cannot open windbreaks file: " + filename);

    std::string line;
    while (std::getline(f, line)) {
        // strip comments
        auto pos = line.find('#');
        if (pos != std::string::npos) line = line.substr(0, pos);
        // replace commas with spaces
        std::replace(line.begin(), line.end(), ',', ' ');
        std::istringstream ss(line);
        Real ax, ay, bx, by, h, block, cd;
        if (ss >> ax >> ay >> bx >> by >> h >> block >> cd) {
            x1.push_back(ax);
            y1.push_back(ay);
            x2.push_back(bx);
            y2.push_back(by);
            height.push_back(h);
            blockage.push_back(block);
            drag_coeff.push_back(cd);
        }
    }
    if (x1.empty())
        amrex::Abort("wind_solver: no data read from windbreaks file: " + filename);

    amrex::Print() << "wind_solver: read " << x1.size()
                   << " windbreak segment(s) from " << filename << "\n";
}

// Read time series file: time U_ref V_ref (whitespace or comma separated; '#' comments)
void read_time_series_file(const std::string& filename,
                           std::vector<Real>& times,
                           std::vector<Real>& U_refs,
                           std::vector<Real>& V_refs)
{
    std::ifstream f(filename);
    if (!f.is_open())
        amrex::Abort("wind_solver: cannot open time series file: " + filename);

    std::string line;
    while (std::getline(f, line)) {
        // strip comments
        auto pos = line.find('#');
        if (pos != std::string::npos) line = line.substr(0, pos);
        // replace commas with spaces
        std::replace(line.begin(), line.end(), ',', ' ');
        std::istringstream ss(line);
        Real t, u, v;
        if (ss >> t >> u >> v) {
            times.push_back(t);
            U_refs.push_back(u);
            V_refs.push_back(v);
        }
    }
    if (times.empty())
        amrex::Abort("wind_solver: no data read from time series file: " + filename);

    amrex::Print() << "wind_solver: read " << times.size()
                   << " time points from " << filename << "\n";
}

// Read multi-point vertical profile CSV files of speed/direction.
// Format: X, Y, Z, Speed, Direction (degrees)
void read_vertical_profile_csv(const std::string& filename,
                               std::vector<Real>& xd,
                               std::vector<Real>& yd,
                               std::vector<Real>& zd,
                               std::vector<Real>& ux,
                               std::vector<Real>& uy)
{
    std::ifstream f(filename);
    if (!f.is_open())
        amrex::Abort("wind_solver: cannot open vertical profile file: " + filename);

    std::string line;
    bool is_first = true;
    while (std::getline(f, line)) {
        // strip comments
        auto pos = line.find('#');
        if (pos != std::string::npos) line = line.substr(0, pos);
        if (line.empty()) continue;

        // replace commas with spaces
        std::replace(line.begin(), line.end(), ',', ' ');
        std::istringstream ss(line);
        
        // Skip header if it contains text instead of numbers
        if (is_first) {
            std::string first_token;
            if (ss >> first_token) {
                // If first token is non-numeric, skip the line as header
                try {
                    std::stod(first_token);
                } catch (...) {
                    is_first = false;
                    continue;
                }
            }
            is_first = false;
            // Reset ss
            ss.clear();
            ss.str(line);
        }

        Real x, y, z, speed, direction;
        if (ss >> x >> y >> z >> speed >> direction) {
            xd.push_back(x);
            yd.push_back(y);
            zd.push_back(z);
            
            // Convert speed and direction (degrees) to u and v
            Real dir_rad = direction * MathConstants::deg_to_rad;
            // Meteorological convention: u = -spd * sin(dir), v = -spd * cos(dir)
            Real u_val = -speed * std::sin(dir_rad);
            Real v_val = -speed * std::cos(dir_rad);
            ux.push_back(u_val);
            uy.push_back(v_val);
        }
    }
    if (xd.empty()) {
        amrex::Abort("wind_solver: no data read from vertical profile file: " + filename);
    }
    
    amrex::Print() << "wind_solver: read " << xd.size()
                   << " vertical profile points from " << filename << "\n";
}

// Read multi-point 3D windfield CSV file (whitespace or comma separated; '#' comments).
// Format: X Y Z U V W
void read_windfield_file(const std::string& filename,
                         std::vector<Real>& xd,
                         std::vector<Real>& yd,
                         std::vector<Real>& zd,
                         std::vector<Real>& ux,
                         std::vector<Real>& uy,
                         std::vector<Real>& uz)
{
    std::ifstream f(filename);
    if (!f.is_open())
        amrex::Abort("wind_solver: cannot open windfield file: " + filename);

    std::string line;
    bool is_first = true;
    while (std::getline(f, line)) {
        // strip comments
        auto pos = line.find('#');
        if (pos != std::string::npos) line = line.substr(0, pos);
        if (line.empty()) continue;

        // replace commas with spaces
        std::replace(line.begin(), line.end(), ',', ' ');
        std::istringstream ss(line);
        
        // Skip header if it contains text instead of numbers
        if (is_first) {
            std::string first_token;
            if (ss >> first_token) {
                try {
                    std::stod(first_token);
                } catch (...) {
                    is_first = false;
                    continue;
                }
            }
            is_first = false;
            ss.clear();
            ss.str(line);
        }

        Real x, y, z, u, v, w;
        if (ss >> x >> y >> z >> u >> v >> w) {
            xd.push_back(x);
            yd.push_back(y);
            zd.push_back(z);
            ux.push_back(u);
            uy.push_back(v);
            uz.push_back(w);
        }
    }
    if (xd.empty()) {
        amrex::Abort("wind_solver: no data read from windfield file: " + filename);
    }

    amrex::Print() << "wind_solver: read " << xd.size()
                   << " windfield points from " << filename << "\n";
}

} // namespace WindIO
