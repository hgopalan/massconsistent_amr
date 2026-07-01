// ============================================================================
// surface_boundary_conditions.H
// Monin-Obukhov Similarity Theory (MOST) surface boundary conditions
//
// Applies terrain-aware surface boundary conditions using MOST profiles
// at the first cell above ground/terrain surface. Supports spatial variability
// in roughness (z0), Obukhov length (L), and sensible heat flux.
//
// References:
// - Monin, A. S., & Obukhov, A. M. (1954). Basic laws of turbulent mixing
//   in the ground layer of the atmosphere. Tr. Akad. Nauk SSSR Geofiz. Inst., 24, 163-187.
// - Businger, J. A., et al. (1971). Flux-Profile Relationships in the
//   Atmospheric Surface Layer. J. Atmos. Sci., 28(2), 181-189.
// - Paulson, C. A. (1970). The Mathematical Representation of Wind Speed
//   and Temperature Profiles in the Unstable Atmospheric Surface Layer.
//   J. Appl. Meteor., 9(6), 857-861.
// ============================================================================

#ifndef SURFACE_BOUNDARY_CONDITIONS_H
#define SURFACE_BOUNDARY_CONDITIONS_H

#include <AMReX_REAL.H>
#include <AMReX_GpuQualifiers.H>
#include <AMReX_Array4.H>
#include "solver_math_constants.H"
#include "stability_models.H"
#include <cmath>

// ============================================================================
// Apply MOST wind profile correction at first cell above terrain
//
// This function corrects the wind velocity field at the first cell above
// terrain using Monin-Obukhov Similarity Theory. It respects spatial
// variability in roughness (z0), Obukhov length (L), and friction velocity.
//
// Parameters:
//   vel_arr       - velocity array [i,j,k,component] where component: 0=u, 1=v, 2=w
//   terrain       - terrain elevation array [j*nx + i]
//   z0_field      - roughness length array [j*nx + i] [m]
//   L_obukhov_field - Obukhov length array [j*nx + i] [m]
//   u_profile     - reference wind speed at reference height [m/s]
//   v_profile     - reference wind speed at reference height [m/s]
//   z_ref         - reference height where (u_profile, v_profile) are specified [m]
//   ustar         - friction velocity [m/s] (scalar, same everywhere)
//   kappa         - von Karman constant (typically 0.41) [-]
//   z_lo          - lowest z coordinate in domain [m]
//   dz            - grid spacing in vertical direction [m]
//   nx, ny        - horizontal domain dimensions
//   ilo, ihi      - x-index range
//   jlo, jhi      - y-index range
//   klo, khi      - z-index range
//
// References:
//   - Denotes log-law wind profile with stability correction
//   - First cell above terrain is overwritten with MOST profile
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void apply_most_wind_correction_at_first_cell(
    amrex::Array4<amrex::Real> const& vel_arr,
    amrex::Real const* terrain,
    amrex::Real const* z0_field,
    amrex::Real const* L_obukhov_field,
    amrex::Real u_profile,
    amrex::Real v_profile,
    amrex::Real z_ref,
    amrex::Real ustar,
    amrex::Real kappa,
    amrex::Real z_lo,
    amrex::Real dz,
    int nx,
    int ilo, int ihi,
    int jlo, int jhi,
    int klo, int khi) noexcept
{
    // Reference wind speed magnitude
    amrex::Real speed_ref = std::sqrt(u_profile * u_profile + v_profile * v_profile);
    if (speed_ref < 1.0e-6) {
        speed_ref = 1.0e-6; // Avoid division by zero
    }
    amrex::Real u_norm = u_profile / speed_ref;
    amrex::Real v_norm = v_profile / speed_ref;

    // Loop over horizontal grid points
    for (int j = jlo; j <= jhi; ++j) {
        for (int i = ilo; i <= ihi; ++i) {
            // Get surface properties at this location
            int idx_2d = j * nx + i;
            amrex::Real z0_local = z0_field[idx_2d];
            amrex::Real L_local = L_obukhov_field[idx_2d];
            amrex::Real terrain_elev = terrain[idx_2d];

            // Find first cell above terrain
            int k_start = klo;
            while (k_start <= khi) {
                amrex::Real z_cell = z_lo + (amrex::Real(k_start) + amrex::Real(0.5)) * dz;
                amrex::Real z_agl = z_cell - terrain_elev;
                if (z_agl > amrex::Real(0.0)) {
                    break;
                }
                k_start++;
            }

            // Apply MOST profile correction at first cell
            if (k_start <= khi) {
                amrex::Real z_cell = z_lo + (amrex::Real(k_start) + amrex::Real(0.5)) * dz;
                amrex::Real z_agl = z_cell - terrain_elev;

                // Ensure we don't have numerical issues with very small z_agl
                if (z_agl < z0_local) {
                    z_agl = z0_local;
                }

                // Compute wind speed using MOST log-law with stability correction
                // u(z) = (u*/kappa) * [ln((z+z0)/z0) - psi_m(z/L) + psi_m(z0/L)]
                // For z0 << z, psi_m(z0/L) ≈ 0
                amrex::Real z_ratio = (z_agl + z0_local) / z0_local;
                amrex::Real zeta = z_agl / L_local;
                amrex::Real psi_m_val = psi_m(zeta, false); // Use Businger-Dyer
                
                amrex::Real log_profile = std::log(z_ratio) - psi_m_val;
                amrex::Real speed_most = (ustar / kappa) * log_profile;

                // Normalize wind components
                vel_arr(i, j, k_start, 0) = speed_most * u_norm;
                vel_arr(i, j, k_start, 1) = speed_most * v_norm;
                // Note: w (vertical component) is typically set by divergence constraint,
                // not directly by surface layer theory. Leave unchanged here.
            }
        }
    }
}

// ============================================================================
// Apply MOST temperature correction at first cell above terrain
//
// This function corrects the temperature field at the first cell above terrain
// using Monin-Obukhov Similarity Theory with a prescribed sensible heat flux.
//
// Parameters:
//   temp_arr      - temperature array [i,j,k]
//   terrain       - terrain elevation array [j*nx + i]
//   z0_field      - roughness length array [j*nx + i] [m]
//   L_obukhov_field - Obukhov length array [j*nx + i] [m]
//   sensible_heat_field - sensible heat flux array [j*nx + i] [W/m^2]
//   ustar_field   - friction velocity array [j*nx + i] [m/s]
//   T_ref         - reference temperature at reference height [K]
//   z_ref         - reference height where T_ref is specified [m]
//   kappa         - von Karman constant (typically 0.41) [-]
//   z_lo          - lowest z coordinate in domain [m]
//   dz            - grid spacing in vertical direction [m]
//   rho_air       - air density [kg/m^3]
//   cp_air        - specific heat capacity of air [J/(kg*K)]
//   nx, ny        - horizontal domain dimensions
//   ilo, ihi      - x-index range
//   jlo, jhi      - y-index range
//   klo, khi      - z-index range
//
// References:
//   - Temperature scale: theta_star = -Q0 / (rho * cp * u*)
//   - Temperature profile: T(z) = T_ref + (theta_star/kappa) * [ln(z/z0) - psi_h(z/L)]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void apply_most_temperature_correction_at_first_cell(
    amrex::Array4<amrex::Real> const& temp_arr,
    amrex::Real const* terrain,
    amrex::Real const* z0_field,
    amrex::Real const* L_obukhov_field,
    amrex::Real const* sensible_heat_field,
    amrex::Real const* ustar_field,
    amrex::Real T_ref,
    amrex::Real z_ref,
    amrex::Real kappa,
    amrex::Real z_lo,
    amrex::Real dz,
    amrex::Real rho_air,
    amrex::Real cp_air,
    int nx,
    int ilo, int ihi,
    int jlo, int jhi,
    int klo, int khi) noexcept
{
    // von Karman constant
    const amrex::Real kappa_val = kappa;
    const amrex::Real rho_cp_inv = amrex::Real(1.0) / (rho_air * cp_air);

    // Loop over horizontal grid points
    for (int j = jlo; j <= jhi; ++j) {
        for (int i = ilo; i <= ihi; ++i) {
            // Get surface properties at this location
            int idx_2d = j * nx + i;
            amrex::Real z0_local = z0_field[idx_2d];
            amrex::Real L_local = L_obukhov_field[idx_2d];
            amrex::Real terrain_elev = terrain[idx_2d];
            amrex::Real Q0 = sensible_heat_field[idx_2d];  // [W/m^2]
            amrex::Real ustar_local = ustar_field[idx_2d]; // [m/s]

            // Avoid division by zero
            if (std::abs(ustar_local) < 1.0e-6) {
                ustar_local = 1.0e-6;
            }

            // Find first cell above terrain
            int k_start = klo;
            while (k_start <= khi) {
                amrex::Real z_cell = z_lo + (amrex::Real(k_start) + amrex::Real(0.5)) * dz;
                amrex::Real z_agl = z_cell - terrain_elev;
                if (z_agl > amrex::Real(0.0)) {
                    break;
                }
                k_start++;
            }

            // Apply MOST temperature profile correction at first cell
            if (k_start <= khi) {
                amrex::Real z_cell = z_lo + (amrex::Real(k_start) + amrex::Real(0.5)) * dz;
                amrex::Real z_agl = z_cell - terrain_elev;

                // Ensure we don't have numerical issues
                if (z_agl < z0_local) {
                    z_agl = z0_local;
                }

                // Compute temperature scale (theta_star) from sensible heat flux
                // theta_star = -Q0 / (rho * cp * u*)
                // Note: Q0 sign convention: positive means upward heat flux (warming)
                amrex::Real theta_star = -Q0 * rho_cp_inv / ustar_local;

                // Temperature profile with stability correction
                // T(z) = T_ref + (theta_star/kappa) * [ln(z/z0) - psi_h(z/L)]
                amrex::Real zeta = z_agl / L_local;
                // For temperature, use psi_h which is similar to psi_m
                // Simple approximation: psi_h ≈ psi_m (Paulson 1970, Dyer 1974)
                amrex::Real psi_h_val = psi_m(zeta, false);

                amrex::Real log_profile_temp = std::log(z_agl / z0_local) - psi_h_val;
                amrex::Real T_surface_layer = T_ref + (theta_star / kappa_val) * log_profile_temp;

                temp_arr(i, j, k_start) = T_surface_layer;
            }
        }
    }
}

// ============================================================================
// Compute friction velocity (u*) from wind profile and surface properties
//
// This is a diagnostic function to compute u* from a given wind speed at
// a reference height, using MOST profile inversion.
//
// Parameters:
//   u_ref, v_ref  - wind speed components at reference height [m/s]
//   z_ref         - reference height [m]
//   z0            - roughness length [m]
//   L             - Obukhov length [m]
//   kappa         - von Karman constant
//
// Returns:
//   friction velocity magnitude [m/s]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real compute_friction_velocity_from_profile(
    amrex::Real u_ref,
    amrex::Real v_ref,
    amrex::Real z_ref,
    amrex::Real z0,
    amrex::Real L,
    amrex::Real kappa) noexcept
{
    // Wind speed magnitude at reference height
    amrex::Real speed_ref = std::sqrt(u_ref * u_ref + v_ref * v_ref);
    if (speed_ref < 1.0e-6) {
        return 1.0e-6;  // Minimal friction velocity for calm conditions
    }

    // Invert MOST log-law to get u*
    // u_ref = (u*/kappa) * [ln((z_ref+z0)/z0) - psi_m(z_ref/L)]
    // u* = u_ref * kappa / [ln((z_ref+z0)/z0) - psi_m(z_ref/L)]
    
    amrex::Real zeta_ref = z_ref / L;
    amrex::Real psi_m_ref = psi_m(zeta_ref, false);
    amrex::Real log_profile = std::log((z_ref + z0) / z0) - psi_m_ref;
    
    if (log_profile < 1.0e-6) {
        log_profile = 1.0e-6;  // Prevent division by zero
    }

    amrex::Real ustar = speed_ref * kappa / log_profile;
    return ustar;
}

#endif // SURFACE_BOUNDARY_CONDITIONS_H
