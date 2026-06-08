// ============================================================================
// cell_local_anisotropy.H
// Cell-Local Spatially-Varying Variational Anisotropy
//
// Formulates and computes a fully 3D spatially-varying anisotropic diagonal
// weighting tensor A(x, y, z) where local horizontal-to-vertical adjustment
// coefficients (alpha_h / alpha_v) adapt cell-locally based on local terrain
// slope, local Richardson number (Ri), and local Froude number (Fr).
// ============================================================================

#ifndef CELL_LOCAL_ANISOTROPY_H
#define CELL_LOCAL_ANISOTROPY_H

#include <AMReX_REAL.H>
#include <AMReX_MultiFab.H>
#include <AMReX_GpuQualifiers.H>
#include <cmath>
#include <algorithm>
#include <string>

// Include existing models
#include "richardson_number_models.H"
#include "stability_models.H"
#include "orographic_models.H"

namespace CellLocalAnisotropy {

AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real compute_local_slope(
    int i, int j,
    const amrex::Real* d_terrain,
    int nx, int ny,
    amrex::Real dx,
    amrex::Real dy) noexcept
{
    int im1 = std::max(0, i - 1);
    int ip1 = std::min(nx - 1, i + 1);
    int jm1 = std::max(0, j - 1);
    int jp1 = std::min(ny - 1, j + 1);

    amrex::Real h_w = d_terrain[j * nx + im1];
    amrex::Real h_e = d_terrain[j * nx + ip1];
    amrex::Real h_s = d_terrain[jm1 * nx + i];
    amrex::Real h_n = d_terrain[jp1 * nx + i];

    amrex::Real dh_dx = (h_e - h_w) / (amrex::Real(ip1 - im1) * dx);
    amrex::Real dh_dy = (h_n - h_s) / (amrex::Real(jp1 - jm1) * dy);

    if (ip1 == im1) dh_dx = amrex::Real(0.0);
    if (jp1 == jm1) dh_dy = amrex::Real(0.0);

    return std::sqrt(dh_dx * dh_dx + dh_dy * dh_dy);
}

inline void compute_cell_local_anisotropy_fields(
    amrex::MultiFab& alpha_h_field,
    amrex::MultiFab& alpha_v_field,
    const amrex::MultiFab& vel0,
    const amrex::MultiFab& temp,
    const amrex::Real* d_terrain,
    int nx, int ny, int nz,
    amrex::Real dx, amrex::Real dy, amrex::Real dz,
    amrex::Real alpha_h_base,
    amrex::Real alpha_v_base,
    amrex::Real zs_min,
    bool enable_cell_local_anisotropy,
    const std::string& anisotropy_source,
    amrex::Real anisotropy_slope_scale,
    amrex::Real anisotropy_decay_height,
    amrex::Real anisotropy_ri_gamma,
    amrex::Real anisotropy_ri_beta,
    amrex::Real anisotropy_fr_min)
{
    // If not enabled, set to base values
    if (!enable_cell_local_anisotropy) {
        alpha_h_field.setVal(alpha_h_base);
        alpha_v_field.setVal(alpha_v_base);
        return;
    }

    // Determine numerical source option: 0 = all, 1 = slope, 2 = richardson, 3 = froude
    int source_mode = 0;
    if (anisotropy_source == "slope") {
        source_mode = 1;
    } else if (anisotropy_source == "richardson") {
        source_mode = 2;
    } else if (anisotropy_source == "froude") {
        source_mode = 3;
    }

    for (amrex::MFIter mfi(alpha_h_field); mfi.isValid(); ++mfi) {
        const amrex::Box& bx = mfi.validbox();
        auto ah = alpha_h_field.array(mfi);
        auto av = alpha_v_field.array(mfi);
        auto v0 = vel0.const_array(mfi);
        auto t_arr = temp.const_array(mfi);

        amrex::ParallelFor(bx, [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept {
            // Horizontal factor is held at base value
            ah(i, j, k) = alpha_h_base;

            // Compute height above ground level (AGL)
            amrex::Real z_physical = zs_min + (k + amrex::Real(0.5)) * dz;
            amrex::Real z_terrain = d_terrain[j * nx + i];
            amrex::Real z_agl = z_physical - z_terrain;

            if (z_agl <= amrex::Real(0.0)) {
                av(i, j, k) = alpha_v_base;
                return;
            }

            // 1. Terrain Slope factor
            amrex::Real f_slope = amrex::Real(1.0);
            if (source_mode == 0 || source_mode == 1) {
                // Calculate boundary layer height for this column (i, j) on the fly
                amrex::Real bl_height = amrex::Real(500.0); // default fallback
                {
                    int k_start = 0;
                    while (k_start < nz && (zs_min + (k_start + amrex::Real(0.5)) * dz - z_terrain <= amrex::Real(0.0))) {
                        k_start++;
                    }
                    if (k_start < nz) {
                        amrex::Real theta_s = t_arr(i, j, k_start);
                        bool found = false;
                        amrex::Real richardson_critical = amrex::Real(0.25);
                        for (int kp = k_start + 1; kp < nz; ++kp) {
                            amrex::Real z_agl_kp = zs_min + (kp + amrex::Real(0.5)) * dz - z_terrain;
                            amrex::Real u_kp = v0(i, j, kp, 0);
                            amrex::Real v_kp = v0(i, j, kp, 1);
                            amrex::Real u_mag_kp = std::sqrt(u_kp * u_kp + v_kp * v_kp);
                            amrex::Real ri_b_kp = compute_bulk_richardson_number(theta_s, t_arr(i, j, kp), z_agl_kp, u_mag_kp, theta_s);

                            if (ri_b_kp > richardson_critical) {
                                amrex::Real z_agl_kpm1 = zs_min + (kp - 1 + amrex::Real(0.5)) * dz - z_terrain;
                                amrex::Real u_kpm1 = v0(i, j, kp - 1, 0);
                                amrex::Real v_kpm1 = v0(i, j, kp - 1, 1);
                                amrex::Real u_mag_kpm1 = std::sqrt(u_kpm1 * u_kpm1 + v_kpm1 * v_kpm1);
                                amrex::Real ri_b_kpm1 = compute_bulk_richardson_number(theta_s, t_arr(i, j, kp - 1), z_agl_kpm1, u_mag_kpm1, theta_s);

                                if (std::abs(ri_b_kp - ri_b_kpm1) > amrex::Real(1.0e-10)) {
                                    amrex::Real frac = (richardson_critical - ri_b_kpm1) / (ri_b_kp - ri_b_kpm1);
                                    bl_height = z_agl_kpm1 + frac * (z_agl_kp - z_agl_kpm1);
                                } else {
                                    bl_height = z_agl_kpm1;
                                }
                                found = true;
                                break;
                            }
                        }
                        if (!found) {
                            bl_height = RichardsonNumberConstants::MAX_BL_DEPTH;
                        }
                        bl_height = std::max(RichardsonNumberConstants::MIN_BL_DEPTH,
                                             std::min(bl_height, RichardsonNumberConstants::MAX_BL_DEPTH));
                    }
                }

                amrex::Real slope = compute_local_slope(i, j, d_terrain, nx, ny, dx, dy);
                amrex::Real slope_3d = slope * std::exp(-z_agl / bl_height);
                f_slope = std::exp(-slope_3d / anisotropy_slope_scale);
            }

            // Compute vertical derivatives
            amrex::Real dT_dz = amrex::Real(0.0);
            amrex::Real du_dz = amrex::Real(0.0);
            amrex::Real dv_dz = amrex::Real(0.0);

            if (k == 0) {
                dT_dz = (t_arr(i, j, 1) - t_arr(i, j, 0)) / dz;
                du_dz = (v0(i, j, 1, 0) - v0(i, j, 0, 0)) / dz;
                dv_dz = (v0(i, j, 1, 1) - v0(i, j, 0, 1)) / dz;
            } else if (k == nz - 1) {
                dT_dz = (t_arr(i, j, nz - 1) - t_arr(i, j, nz - 2)) / dz;
                du_dz = (v0(i, j, nz - 1, 0) - v0(i, j, nz - 2, 0)) / dz;
                dv_dz = (v0(i, j, nz - 1, 1) - v0(i, j, nz - 2, 1)) / dz;
            } else {
                dT_dz = (t_arr(i, j, k + 1) - t_arr(i, j, k - 1)) / (amrex::Real(2.0) * dz);
                du_dz = (v0(i, j, k + 1, 0) - v0(i, j, k - 1, 0)) / (amrex::Real(2.0) * dz);
                dv_dz = (v0(i, j, k + 1, 1) - v0(i, j, k - 1, 1)) / (amrex::Real(2.0) * dz);
            }

            amrex::Real shear_sq = du_dz * du_dz + dv_dz * dv_dz;
            amrex::Real T_ref = t_arr(i, j, k);
            if (T_ref < amrex::Real(1.0)) T_ref = amrex::Real(300.0);

            // 2. Richardson factor
            amrex::Real f_ri = amrex::Real(1.0);
            if (source_mode == 0 || source_mode == 2) {
                amrex::Real Ri = compute_richardson_number(T_ref, dT_dz, std::sqrt(shear_sq), amrex::Real(1.0e-3));
                if (Ri > amrex::Real(0.0)) {
                    f_ri = std::exp(-anisotropy_ri_gamma * Ri);
                } else if (Ri < amrex::Real(0.0)) {
                    f_ri = amrex::Real(1.0) + anisotropy_ri_beta * std::min(amrex::Real(10.0), -Ri);
                }
                f_ri = std::max(amrex::Real(0.1), std::min(f_ri, amrex::Real(2.0)));
            }

            // 3. Froude factor
            amrex::Real f_fr = amrex::Real(1.0);
            if (source_mode == 0 || source_mode == 3) {
                amrex::Real N_sq = (amrex::Real(9.81) / T_ref) * dT_dz;
                if (N_sq > amrex::Real(0.0)) {
                    amrex::Real N = std::sqrt(N_sq);
                    amrex::Real U_local = std::sqrt(v0(i, j, k, 0) * v0(i, j, k, 0) +
                                                     v0(i, j, k, 1) * v0(i, j, k, 1));
                    amrex::Real L_scale = std::max(amrex::Real(10.0), z_agl);
                    amrex::Real Fr = compute_orographic_froude_number(U_local, N, L_scale);
                    f_fr = amrex::Real(1.0) - std::exp(-Fr * Fr);
                    f_fr = std::max(anisotropy_fr_min, std::min(f_fr, amrex::Real(1.0)));
                }
            }

            // Combine factors
            amrex::Real val = alpha_v_base * f_slope * f_ri * f_fr;

            // Clamp vertical coefficient to physically reasonable range [0.05 * base, 2.0 * base]
            amrex::Real min_val = amrex::Real(0.05) * alpha_v_base;
            amrex::Real max_val = amrex::Real(2.0) * alpha_v_base;
            av(i, j, k) = std::max(min_val, std::min(val, max_val));
        });
    }
}

} // namespace CellLocalAnisotropy

#endif // CELL_LOCAL_ANISOTROPY_H
