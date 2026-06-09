#include "wind_io_helpers.H"
#include "solver_math_constants.H"
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

// Read X Y H FAI canopy file (whitespace or comma separated; '#' comments).
// Format: X Y canopy_height frontal_area_index
void read_canopy_file(const std::string& filename,
                      std::vector<Real>& xd,
                      std::vector<Real>& yd,
                      std::vector<Real>& canopy_h_d,
                      std::vector<Real>& frontal_area_index_d)
{
    std::ifstream f(filename);
    if (!f.is_open())
        amrex::Abort("wind_solver: cannot open canopy file: " + filename);

    std::string line;
    while (std::getline(f, line)) {
        // strip comments
        auto pos = line.find('#');
        if (pos != std::string::npos) line = line.substr(0, pos);
        // replace commas with spaces
        std::replace(line.begin(), line.end(), ',', ' ');
        std::istringstream ss(line);
        Real x, y, h, fai;
        if (ss >> x >> y >> h >> fai) {
            xd.push_back(x);
            yd.push_back(y);
            canopy_h_d.push_back(h);
            frontal_area_index_d.push_back(fai);
        }
    }
    if (xd.empty())
        amrex::Abort("wind_solver: no data read from canopy file: " + filename);

    amrex::Print() << "wind_solver: read " << xd.size()
                   << " canopy points from " << filename << "\n";
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
                        std::vector<Real>& rotation,
                        std::vector<int>& shape,
                        std::vector<Real>& pitch_or_radius,
                        std::vector<Real>& pitch_direction)
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
        Real x1, x2, y1, y2, z1, z2;
        if (ss >> x1 >> x2 >> y1 >> y2 >> z1 >> z2) {
            Real angle = 0.0;
            int shp = 0; // 0 = RECTANGULAR, 1 = CYLINDRICAL, 2 = PITCHED_ROOF
            Real p_or_r = 0.0;
            Real p_dir = 0.0;
            if (ss >> angle) {
                angle = angle * MathConstants::deg_to_rad;
                std::string shape_str;
                if (ss >> shape_str) {
                    for (char &c : shape_str) {
                        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    }
                    try {
                        shp = std::stoi(shape_str);
                    } catch (const std::invalid_argument&) {
                        if (shape_str == "cylindrical" || shape_str == "cylinder") {
                            shp = 1;
                        } else if (shape_str == "pitched_roof" || shape_str == "pitched") {
                            shp = 2;
                        } else {
                            shp = 0;
                        }
                    } catch (const std::out_of_range&) {
                        shp = 0;
                    }
                    if (ss >> p_or_r) {
                        if (shp == 2) { // PITCHED_ROOF
                            p_or_r = p_or_r * MathConstants::deg_to_rad;
                        }
                        if (ss >> p_dir) {
                            p_dir = p_dir * MathConstants::deg_to_rad;
                        }
                    }
                }
            }
            xmin.push_back(x1);
            xmax.push_back(x2);
            ymin.push_back(y1);
            ymax.push_back(y2);
            zmin.push_back(z1);
            zmax.push_back(z2);
            rotation.push_back(angle);
            shape.push_back(shp);
            pitch_or_radius.push_back(p_or_r);
            pitch_direction.push_back(p_dir);
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

// Read precipitation file: time precip_rate (whitespace or comma separated; '#' comments)
void read_precipitation_file(const std::string& filename,
                             std::vector<Real>& times,
                             std::vector<Real>& rates)
{
    std::ifstream f(filename);
    if (!f.is_open())
        amrex::Abort("wind_solver: cannot open precipitation file: " + filename);

    std::string line;
    while (std::getline(f, line)) {
        // strip comments
        auto pos = line.find('#');
        if (pos != std::string::npos) line = line.substr(0, pos);
        // replace commas with spaces
        std::replace(line.begin(), line.end(), ',', ' ');
        std::istringstream ss(line);
        Real t, rate;
        if (ss >> t >> rate) {
            times.push_back(t);
            rates.push_back(rate);
        }
    }
    if (times.empty())
        amrex::Abort("wind_solver: no data read from precipitation file: " + filename);

    amrex::Print() << "wind_solver: read " << times.size()
                   << " precipitation data points from " << filename << "\n";
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

void read_fsl_sounding(const std::string& filename,
                       std::vector<Real>& z,
                       std::vector<Real>& u,
                       std::vector<Real>& v,
                       bool wind_in_knots)
{
    std::ifstream f(filename);
    if (!f.is_open()) {
        amrex::Abort("wind_solver: cannot open FSL sounding file: " + filename);
    }
    std::string line;
    while (std::getline(f, line)) {
        // strip comments
        auto pos = line.find('#');
        if (pos != std::string::npos) line = line.substr(0, pos);
        if (line.empty()) continue;

        std::istringstream ss(line);
        int level_type;
        if (!(ss >> level_type)) continue;

        // FSL level types: 1 = mandatory level, 2 = significant level, 3 = wind level
        if (level_type == 1 || level_type == 2 || level_type == 3) {
            Real pressure, hght, temp, dew, dir, spd;
            if (ss >> pressure >> hght >> temp >> dew >> dir >> spd) {
                if (hght >= 99999.0 || dir >= 9999.0 || spd >= 9999.0 || dir < 0.0 || spd < 0.0) {
                    continue; // Skip missing data
                }
                z.push_back(hght);
                Real speed_ms = spd;
                if (wind_in_knots) {
                    speed_ms *= 0.514444; // 1 knot = 0.514444 m/s
                }
                Real dir_rad = dir * MathConstants::deg_to_rad;
                Real u_val = -speed_ms * std::sin(dir_rad);
                Real v_val = -speed_ms * std::cos(dir_rad);
                u.push_back(u_val);
                v.push_back(v_val);
            }
        }
    }
}

void read_sounding_file(const std::string& filename,
                        std::vector<Real>& z,
                        std::vector<Real>& u,
                        std::vector<Real>& v,
                        bool wind_in_knots)
{
    std::ifstream f(filename);
    if (!f.is_open()) {
        amrex::Abort("wind_solver: cannot open sounding file: " + filename);
    }

    std::string line;
    bool is_fsl = false;
    int line_count = 0;
    while (std::getline(f, line) && line_count < 10) {
        auto pos = line.find('#');
        if (pos != std::string::npos) line = line.substr(0, pos);
        std::istringstream ss(line);
        int first_val;
        if (ss >> first_val) {
            // FSL format record types: 254 is standard FSL header, 9 is station ID,
            // 4 is station coordinates/elevation, and 1 is mandatory level data.
            if (first_val == 254 || first_val == 9 || first_val == 4 || first_val == 1) {
                is_fsl = true;
                break;
            }
        }
        line_count++;
    }

    f.close();

    if (is_fsl) {
        amrex::Print() << "wind_solver: reading " << filename << " as FSL format\n";
        read_fsl_sounding(filename, z, u, v, wind_in_knots);
    } else {
        amrex::Print() << "wind_solver: reading " << filename << " as UP.DAT / custom format\n";
        std::ifstream f2(filename);
        while (std::getline(f2, line)) {
            auto pos = line.find('#');
            if (pos != std::string::npos) line = line.substr(0, pos);
            std::replace(line.begin(), line.end(), ',', ' ');
            std::istringstream ss(line);
            std::vector<double> vals;
            double val;
            while (ss >> val) {
                vals.push_back(val);
            }
            if (vals.size() == 5) {
                // pressure height temp direction speed (UP.DAT format)
                Real h = vals[1];
                Real temp = vals[2];
                Real dir = vals[3];
                Real spd = vals[4];
                if (h >= 99999.0 || dir >= 9999.0 || spd >= 9999.0 || dir < 0.0 || spd < 0.0) continue;
                z.push_back(h);
                Real dir_rad = dir * MathConstants::deg_to_rad;
                u.push_back(-spd * std::sin(dir_rad));
                v.push_back(-spd * std::cos(dir_rad));
            } else if (vals.size() == 3 || vals.size() == 4) {
                // height speed direction [temp]
                Real h = vals[0];
                Real spd = vals[1];
                Real dir = vals[2];
                if (h >= 99999.0 || dir >= 9999.0 || spd >= 9999.0 || dir < 0.0 || spd < 0.0) continue;
                z.push_back(h);
                Real dir_rad = dir * MathConstants::deg_to_rad;
                u.push_back(-spd * std::sin(dir_rad));
                v.push_back(-spd * std::cos(dir_rad));
            }
        }
    }

    if (z.empty()) {
        amrex::Abort("wind_solver: no valid vertical profiles read from sounding file: " + filename);
    }

    // Sort by height in ascending order and remove duplicate heights
    struct SoundingPoint {
        Real z;
        Real u;
        Real v;
        bool operator<(const SoundingPoint& other) const {
            return z < other.z;
        }
    };
    std::vector<SoundingPoint> points(z.size());
    for (std::size_t i = 0; i < z.size(); ++i) {
        points[i] = {z[i], u[i], v[i]};
    }
    std::sort(points.begin(), points.end());

    z.clear();
    u.clear();
    v.clear();
    constexpr Real HEIGHT_DUPLICATE_TOLERANCE = 1e-3; // 1mm vertical tolerance for duplicate levels
    for (const auto& pt : points) {
        if (!z.empty() && std::abs(pt.z - z.back()) < HEIGHT_DUPLICATE_TOLERANCE) {
            continue;
        }
        z.push_back(pt.z);
        u.push_back(pt.u);
        v.push_back(pt.v);
    }
    
    amrex::Print() << "wind_solver: loaded " << z.size() << " sorted vertical levels from sounding " << filename << "\n";
}

} // namespace WindIO
