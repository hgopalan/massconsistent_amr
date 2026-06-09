#ifndef WIND_IO_HELPERS_H
#define WIND_IO_HELPERS_H

#include <AMReX_REAL.H>
#include <string>
#include <vector>

namespace WindIO {

using amrex::Real;

void read_terrain_file(const std::string& filename,
                       std::vector<Real>& xd,
                       std::vector<Real>& yd,
                       std::vector<Real>& zd);

void read_velocity_file(const std::string& filename,
                        std::vector<Real>& xd,
                        std::vector<Real>& yd,
                        std::vector<Real>& zd,
                        std::vector<Real>& ux,
                        std::vector<Real>& uy);

void read_vertical_profile_csv(const std::string& filename,
                               std::vector<Real>& xd,
                               std::vector<Real>& yd,
                               std::vector<Real>& zd,
                               std::vector<Real>& ux,
                               std::vector<Real>& uy);

void read_roughness_file(const std::string& filename,
                         std::vector<Real>& xd,
                         std::vector<Real>& yd,
                         std::vector<Real>& z0_d);

void read_canopy_file(const std::string& filename,
                      std::vector<Real>& xd,
                      std::vector<Real>& yd,
                      std::vector<Real>& canopy_h_d,
                      std::vector<Real>& frontal_area_index_d);

void read_surface_data_file(const std::string& filename,
                            std::vector<Real>& xd,
                            std::vector<Real>& yd,
                            std::vector<Real>& zd,
                            std::vector<Real>& ustar_d,
                            std::vector<Real>& z0_d,
                            std::vector<Real>& u10_d,
                            std::vector<Real>& v10_d);

void read_temperature_file(const std::string& filename,
                           std::vector<Real>& zd,
                           std::vector<Real>& Td);

void read_alpha_coefficients_file(const std::string& filename,
                                  std::vector<Real>& xd,
                                  std::vector<Real>& yd,
                                  std::vector<Real>& alpha_h_data,
                                  std::vector<Real>& alpha_v_data);

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
                        std::vector<Real>& pitch_direction);

void read_porous_building_file(const std::string& filename,
                               std::vector<Real>& xmin,
                               std::vector<Real>& xmax,
                               std::vector<Real>& ymin,
                               std::vector<Real>& ymax,
                               std::vector<Real>& zmin,
                               std::vector<Real>& zmax,
                               std::vector<Real>& porosity,
                               std::vector<Real>& rotation);

void read_windbreaks_file(const std::string& filename,
                          std::vector<Real>& x1,
                          std::vector<Real>& y1,
                          std::vector<Real>& x2,
                          std::vector<Real>& y2,
                          std::vector<Real>& height,
                          std::vector<Real>& blockage,
                          std::vector<Real>& drag_coeff);

void read_time_series_file(const std::string& filename,
                           std::vector<Real>& times,
                           std::vector<Real>& U_refs,
                           std::vector<Real>& V_refs);

void read_precipitation_file(const std::string& filename,
                             std::vector<Real>& times,
                             std::vector<Real>& rates);

void read_windfield_file(const std::string& filename,
                         std::vector<Real>& xd,
                         std::vector<Real>& yd,
                         std::vector<Real>& zd,
                         std::vector<Real>& ux,
                         std::vector<Real>& uy,
                         std::vector<Real>& uz);

void read_sounding_file(const std::string& filename,
                        std::vector<Real>& z,
                        std::vector<Real>& u,
                        std::vector<Real>& v,
                        bool wind_in_knots = true);

} // namespace WindIO

#endif // WIND_IO_HELPERS_H
