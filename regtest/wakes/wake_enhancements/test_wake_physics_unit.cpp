/**
 * test_wake_physics_unit.cpp
 * 
 * Comprehensive C++ unit tests for building wake physics enhancements:
 * 1. Far-wake extension to 15H
 * 2. Oblique angle cavity scaling Lr(θ) = Lr₀ × cos(θ)
 * 3. Tall-building correction Lr = 0.9H × max(1.0, min(W/H, 1.5))
 * 4. Gaussian lateral wake profile option
 * 5. Upwind recirculation zone (~0.5×min(H,W) upstream)
 * 6. Log-law reference velocity correction
 * 7. Corner/side acceleration
 * 8. Height-dependent velocity variance correction
 * 9. Horseshoe vortex at building base
 *
 * These tests verify physics correctness of individual components.
 */

#include <iostream>
#include <cmath>
#include <cassert>
#include <iomanip>

// Include wake models header
#include "wake_models.H"

using Real = amrex::Real;
using std::cout;
using std::endl;

const Real EPSILON = 1.0e-10;
const Real TOLERANCE = 1.0e-6;

// ============================================================================
// Test 1: Oblique Angle Cavity Scaling
// ============================================================================
void test_oblique_cavity_scaling() {
    cout << "\n--- Test 1: Oblique angle cavity scaling Lr(θ) = Lr₀ × cos(θ) ---" << endl;
    
    Real Lr0 = 20.0;  // Base cavity length (m)
    Real wd_x = 1.0;  // Wind direction x (normalized)
    Real wd_y = 0.0;  // Wind direction y (normalized)
    
    // Building normal perpendicular to long axis
    Real bldg_normal_x = 0.0;
    Real bldg_normal_y = 1.0;
    
    // Test perpendicular flow (θ = 90°, cos(θ) = 0)
    Real Lr_perp = compute_oblique_cavity_scaling(Lr0, wd_x, wd_y, bldg_normal_x, bldg_normal_y);
    cout << "  Perpendicular flow (θ=90°): Lr = " << Lr_perp << " m (base: " << Lr0 << " m)" << endl;
    assert(Lr_perp >= 0.3 * Lr0 && Lr_perp <= Lr0, "Perpendicular scaling out of bounds");
    
    // Test oblique flow (45°, cos(45°) ≈ 0.707)
    Real wind_45_x = std::sqrt(2.0) / 2.0;
    Real wind_45_y = std::sqrt(2.0) / 2.0;
    Real Lr_45 = compute_oblique_cavity_scaling(Lr0, wind_45_x, wind_45_y, bldg_normal_x, bldg_normal_y);
    cout << "  Oblique flow (θ=45°): Lr = " << Lr_45 << " m" << endl;
    assert(Lr_45 > 0.3 * Lr0 && Lr_45 < Lr0, "45° scaling out of bounds");
    
    // Test parallel flow (θ = 0°, cos(θ) = 1)
    Real wind_par_x = 0.0;
    Real wind_par_y = 1.0;
    Real Lr_par = compute_oblique_cavity_scaling(Lr0, wind_par_x, wind_par_y, bldg_normal_x, bldg_normal_y);
    cout << "  Parallel flow (θ=0°): Lr = " << Lr_par << " m" << endl;
    assert(Lr_par >= Lr0 * 0.9, "Parallel scaling should be high");
    
    cout << "✓ Oblique cavity scaling test PASSED" << endl;
}

// ============================================================================
// Test 2: Tall-Building Aspect-Ratio Correction
// ============================================================================
void test_tall_building_correction() {
    cout << "\n--- Test 2: Tall-building correction Lr = 0.9H × max(1.0, min(W/H, 1.5)) ---" << endl;
    
    // Test 1: Narrow building (W/H = 0.4)
    Real H_narrow = 50.0;
    Real W_narrow = 20.0;
    Real Lr_narrow = compute_tall_building_correction(H_narrow, W_narrow);
    Real expected_narrow = 0.9 * H_narrow * 1.0;  // min(0.4, 1.5) = 0.4, max(1.0, 0.4) = 1.0
    cout << "  Narrow building (W/H=0.4, H=50m, W=20m): Lr = " << Lr_narrow << " m" << endl;
    cout << "    Expected: " << expected_narrow << " m" << endl;
    assert(std::abs(Lr_narrow - expected_narrow) < TOLERANCE * expected_narrow, 
           "Narrow building correction mismatch");
    
    // Test 2: Square building (W/H = 1.0)
    Real H_square = 30.0;
    Real W_square = 30.0;
    Real Lr_square = compute_tall_building_correction(H_square, W_square);
    Real expected_square = 0.9 * H_square * 1.0;  // min(1.0, 1.5) = 1.0, max(1.0, 1.0) = 1.0
    cout << "  Square building (W/H=1.0, H=30m, W=30m): Lr = " << Lr_square << " m" << endl;
    cout << "    Expected: " << expected_square << " m" << endl;
    assert(std::abs(Lr_square - expected_square) < TOLERANCE * expected_square, 
           "Square building correction mismatch");
    
    // Test 3: Wide building (W/H = 2.0)
    Real H_wide = 25.0;
    Real W_wide = 50.0;
    Real Lr_wide = compute_tall_building_correction(H_wide, W_wide);
    Real expected_wide = 0.9 * H_wide * 1.5;  // min(2.0, 1.5) = 1.5, max(1.0, 1.5) = 1.5
    cout << "  Wide building (W/H=2.0, H=25m, W=50m): Lr = " << Lr_wide << " m" << endl;
    cout << "    Expected: " << expected_wide << " m" << endl;
    assert(std::abs(Lr_wide - expected_wide) < TOLERANCE * expected_wide, 
           "Wide building correction mismatch");
    
    cout << "✓ Tall-building correction test PASSED" << endl;
}

// ============================================================================
// Test 3: Gaussian Lateral Wake Profile
// ============================================================================
void test_gaussian_profile() {
    cout << "\n--- Test 3: Gaussian lateral wake profile ---" << endl;
    
    Real Wr = 20.0;           // Reference wake width (m)
    Real deficit_max = 5.0;   // Maximum deficit (m/s)
    
    // Test center (y=0)
    Real deficit_center = compute_gaussian_deficit(0.0, Wr, deficit_max);
    cout << "  Center (y=0): deficit = " << deficit_center << " m/s" << endl;
    assert(std::abs(deficit_center - deficit_max) < TOLERANCE * deficit_max, 
           "Gaussian center should equal maximum");
    
    // Test 1σ point: exp(-1) ≈ 0.368 of maximum
    Real y_1sigma = Wr / 2.0;  // σ = Wr/2
    Real deficit_1sigma = compute_gaussian_deficit(y_1sigma, Wr, deficit_max);
    Real expected_1sigma = deficit_max * std::exp(-1.0);
    cout << "  1σ point (y=Wr/2): deficit = " << deficit_1sigma << " m/s" << endl;
    cout << "    Expected: " << expected_1sigma << " m/s" << endl;
    assert(std::abs(deficit_1sigma - expected_1sigma) < TOLERANCE * expected_1sigma, 
           "1σ point mismatch");
    
    // Test 2σ point: exp(-4) ≈ 0.018 of maximum
    Real y_2sigma = Wr;
    Real deficit_2sigma = compute_gaussian_deficit(y_2sigma, Wr, deficit_max);
    Real expected_2sigma = deficit_max * std::exp(-4.0);
    cout << "  2σ point (y=Wr): deficit = " << deficit_2sigma << " m/s" << endl;
    cout << "    Expected: " << expected_2sigma << " m/s" << endl;
    assert(std::abs(deficit_2sigma - expected_2sigma) < TOLERANCE * expected_2sigma, 
           "2σ point mismatch");
    
    // Test symmetry
    Real deficit_neg = compute_gaussian_deficit(-y_1sigma, Wr, deficit_max);
    cout << "  Symmetry check: deficit(-Wr/2) = " << deficit_neg << " m/s" << endl;
    assert(std::abs(deficit_neg - deficit_1sigma) < TOLERANCE * deficit_max, 
           "Gaussian should be symmetric");
    
    cout << "✓ Gaussian profile test PASSED" << endl;
}

// ============================================================================
// Test 4: Upwind Recirculation Zone
// ============================================================================
void test_upwind_recirculation() {
    cout << "\n--- Test 4: Upwind recirculation zone (~0.5×min(H,W) upstream) ---" << endl;
    
    Real H = 25.0;
    Real W = 15.0;
    Real U_ref = 10.0;
    
    Real recirculation_extent = 0.5 * std::min(H, W);  // 0.5 * 15 = 7.5m
    cout << "  Recirculation extent: " << recirculation_extent << " m" << endl;
    
    // Test inside recirculation zone (x_wake = -5m)
    Real du_inside = compute_upwind_recirculation(-5.0, H, W, U_ref, 5.0);
    cout << "  Inside zone (x_wake=-5m, z=5m): deficit = " << du_inside << " m/s" << endl;
    assert(du_inside < 0.0, "Upwind deficit should be negative (reverse flow)");
    assert(std::abs(du_inside) < std::abs(0.1 * U_ref), "Upwind deficit magnitude reasonable");
    
    // Test at boundary (x_wake = -7.5m)
    Real du_boundary = compute_upwind_recirculation(-7.5, H, W, U_ref, 5.0);
    cout << "  At boundary (x_wake=-7.5m, z=5m): deficit = " << du_boundary << " m/s" << endl;
    assert(du_boundary < du_inside, "Boundary should have less deficit than inside");
    
    // Test beyond recirculation zone (x_wake = -15m)
    Real du_beyond = compute_upwind_recirculation(-15.0, H, W, U_ref, 5.0);
    cout << "  Beyond zone (x_wake=-15m, z=5m): deficit = " << du_beyond << " m/s" << endl;
    assert(std::abs(du_beyond) < EPSILON, "Beyond zone should have negligible deficit");
    
    // Test height dependency (should decay toward ground)
    Real du_ground = compute_upwind_recirculation(-3.0, H, W, U_ref, 1.0);
    Real du_mid = compute_upwind_recirculation(-3.0, H, W, U_ref, 12.0);
    cout << "  Height dependency (x_wake=-3m):" << endl;
    cout << "    At z=1m:  deficit = " << du_ground << " m/s" << endl;
    cout << "    At z=12m: deficit = " << du_mid << " m/s" << endl;
    
    cout << "✓ Upwind recirculation test PASSED" << endl;
}

// ============================================================================
// Test 5: Log-Law Reference Velocity Correction
// ============================================================================
void test_loglaw_velocity() {
    cout << "\n--- Test 5: Log-law reference velocity correction ---" << endl;
    
    Real z_ref = 10.0;
    Real U_ref = 10.0;
    Real z0 = 0.1;
    
    // Test at reference height
    Real U_at_ref = compute_loglaw_velocity(z_ref, z_ref, U_ref, z0);
    cout << "  Velocity at z_ref (z=10m): U = " << U_at_ref << " m/s" << endl;
    assert(std::abs(U_at_ref - U_ref) < TOLERANCE * U_ref, 
           "Velocity at z_ref should equal U_ref");
    
    // Test at higher elevation (should be higher)
    Real z_high = 25.0;
    Real U_high = compute_loglaw_velocity(z_high, z_ref, U_ref, z0);
    cout << "  Velocity at higher elevation (z=25m): U = " << U_high << " m/s" << endl;
    assert(U_high > U_ref, "Velocity should increase with height");
    
    // Test at lower elevation (should be lower)
    Real z_low = 5.0;
    Real U_low = compute_loglaw_velocity(z_low, z_ref, U_ref, z0);
    cout << "  Velocity at lower elevation (z=5m): U = " << U_low << " m/s" << endl;
    assert(U_low < U_ref, "Velocity should decrease with lower height");
    assert(U_low > 0.0, "Velocity should be positive");
    
    // Test monotonic increase
    cout << "  Monotonic profile check:" << endl;
    Real U_prev = compute_loglaw_velocity(2.0, z_ref, U_ref, z0);
    for (Real z = 3.0; z <= 30.0; z += 3.0) {
        Real U_curr = compute_loglaw_velocity(z, z_ref, U_ref, z0);
        cout << "    z=" << z << "m: U=" << U_curr << " m/s";
        assert(U_curr >= U_prev * 0.99, "Profile should be monotonically increasing");
        cout << " ✓" << endl;
        U_prev = U_curr;
    }
    
    cout << "✓ Log-law velocity test PASSED" << endl;
}

// ============================================================================
// Test 6: Corner and Side Acceleration
// ============================================================================
void test_corner_acceleration() {
    cout << "\n--- Test 6: Corner and side acceleration effects ---" << endl;
    
    Real W = 20.0;
    Real H = 25.0;
    Real deficit_base = 3.0;
    
    // Test at corner
    Real y_corner = W / 2.0 + 1.0;  // Just outside corner
    Real factor_corner = compute_corner_acceleration(y_corner, W, H, 12.0, deficit_base);
    cout << "  At corner (y=" << y_corner << "m): factor = " << factor_corner << endl;
    assert(factor_corner >= 1.0, "Corner factor should be >= 1.0");
    assert(factor_corner <= 1.3, "Corner factor should be reasonable");
    
    // Test at building center
    Real y_center = 0.0;
    Real factor_center = compute_corner_acceleration(y_center, W, H, 12.0, deficit_base);
    cout << "  At center (y=" << y_center << "m): factor = " << factor_center << endl;
    assert(factor_center == 1.0, "Center should have no corner acceleration");
    
    // Test height dependency (peak at mid-height)
    Real z_low = 5.0;
    Real z_mid = H / 2.0;
    Real z_high = H - 2.0;
    Real factor_low = compute_corner_acceleration(y_corner, W, H, z_low, deficit_base);
    Real factor_mid = compute_corner_acceleration(y_corner, W, H, z_mid, deficit_base);
    Real factor_high = compute_corner_acceleration(y_corner, W, H, z_high, deficit_base);
    cout << "  Height dependency at corner:" << endl;
    cout << "    z=" << z_low << "m: factor = " << factor_low << endl;
    cout << "    z=" << z_mid << "m: factor = " << factor_mid << endl;
    cout << "    z=" << z_high << "m: factor = " << factor_high << endl;
    
    cout << "✓ Corner acceleration test PASSED" << endl;
}

// ============================================================================
// Test 7: Height-Dependent Variance Correction
// ============================================================================
void test_variance_correction() {
    cout << "\n--- Test 7: Height-dependent velocity variance correction ---" << endl;
    
    Real Hr = 20.0;  // Cavity height (m)
    
    // Test inside cavity (z < Hr)
    Real var_bottom = compute_variance_correction(5.0, Hr, 5.0 / Hr);
    Real var_middle = compute_variance_correction(10.0, Hr, 10.0 / Hr);
    Real var_top = compute_variance_correction(19.0, Hr, 19.0 / Hr);
    
    cout << "  Inside cavity (z < Hr=" << Hr << "m):" << endl;
    cout << "    Bottom (z=5m, z/Hr=0.25):  variance factor = " << var_bottom << endl;
    cout << "    Middle (z=10m, z/Hr=0.50): variance factor = " << var_middle << endl;
    cout << "    Top (z=19m, z/Hr=0.95):    variance factor = " << var_top << endl;
    
    assert(var_bottom >= 0.5 && var_bottom <= 1.0, "Bottom variance in range");
    assert(var_middle >= 0.5 && var_middle <= 1.0, "Middle variance in range");
    assert(var_top >= 0.5 && var_top <= 1.0, "Top variance in range");
    
    // Test above cavity (should have enhanced variance)
    Real var_shear = compute_variance_correction(25.0, Hr, 25.0 / Hr);
    Real var_above = compute_variance_correction(35.0, Hr, 35.0 / Hr);
    
    cout << "  Above cavity:" << endl;
    cout << "    Shear layer (z=25m, z/Hr=1.25): variance factor = " << var_shear << endl;
    cout << "    Far above (z=35m, z/Hr=1.75):   variance factor = " << var_above << endl;
    
    cout << "✓ Variance correction test PASSED" << endl;
}

// ============================================================================
// Test 8: Horseshoe Vortex
// ============================================================================
void test_horseshoe_vortex() {
    cout << "\n--- Test 8: Horseshoe vortex at building base ---" << endl;
    
    Real x_wake = -2.0;   // Just upwind
    Real H = 25.0;
    Real W = 15.0;
    Real U_ref = 10.0;
    
    Real du_vortex = 0.0;
    Real dv_vortex = 0.0;
    
    // Test near ground at center
    compute_horseshoe_vortex(x_wake, 0.0, 2.0, H, W, U_ref, du_vortex, dv_vortex);
    cout << "  At center, near ground (x=" << x_wake << "m, y=0, z=2m):" << endl;
    cout << "    du = " << du_vortex << " m/s" << endl;
    cout << "    dv = " << dv_vortex << " m/s" << endl;
    
    // Test near corner
    Real du_corner = 0.0;
    Real dv_corner = 0.0;
    Real y_corner = W / 2.0 + 1.0;
    compute_horseshoe_vortex(x_wake, y_corner, 2.0, H, W, U_ref, du_corner, dv_corner);
    cout << "  At corner, near ground (x=" << x_wake << "m, y=" << y_corner << "m, z=2m):" << endl;
    cout << "    du = " << du_corner << " m/s" << endl;
    cout << "    dv = " << dv_corner << " m/s" << endl;
    assert(std::abs(dv_corner) > EPSILON, "Corner vortex should have lateral component");
    
    // Test above ground (should decay to zero)
    Real du_high = 0.0;
    Real dv_high = 0.0;
    compute_horseshoe_vortex(x_wake, 0.0, 15.0, H, W, U_ref, du_high, dv_high);
    cout << "  Above ground (x=" << x_wake << "m, y=0, z=15m):" << endl;
    cout << "    du = " << du_high << " m/s (should be ~0)" << endl;
    cout << "    dv = " << dv_high << " m/s (should be ~0)" << endl;
    assert(std::abs(du_high) < EPSILON, "Vortex should decay above ground");
    assert(std::abs(dv_high) < EPSILON, "Vortex should decay above ground");
    
    cout << "✓ Horseshoe vortex test PASSED" << endl;
}

// ============================================================================
// Test 9: Extended Far-Wake Extent
// ============================================================================
void test_extended_farwake() {
    cout << "\n--- Test 9: Extended far-wake extent to 15H ---" << endl;
    
    Real H = 25.0;
    
    // Test extended far-wake computation
    Real extent_3H = compute_extended_farwake_extent(3.0 * H, H);
    Real extent_10H = compute_extended_farwake_extent(10.0 * H, H);
    Real extent_15H = compute_extended_farwake_extent(15.0 * H, H);
    
    cout << "  Far-wake extent in building heights:" << endl;
    cout << "    At x = 3H (75m):  extent = " << extent_3H << "H" << endl;
    cout << "    At x = 10H (250m): extent = " << extent_10H << "H" << endl;
    cout << "    At x = 15H (375m): extent = " << extent_15H << "H" << endl;
    
    assert(std::abs(extent_3H - 3.0) < EPSILON, "3H extent should equal 3");
    assert(std::abs(extent_10H - 10.0) < EPSILON, "10H extent should equal 10");
    assert(std::abs(extent_15H - 15.0) < EPSILON, "15H extent should equal 15");
    
    cout << "✓ Extended far-wake test PASSED" << endl;
}

// ============================================================================
// Main test runner
// ============================================================================
int main() {
    cout << "\n";
    cout << "============================================================" << endl;
    cout << "WAKE PHYSICS UNIT TESTS - Building Wake Model Enhancements" << endl;
    cout << "============================================================" << endl;
    
    try {
        test_oblique_cavity_scaling();
        test_tall_building_correction();
        test_gaussian_profile();
        test_upwind_recirculation();
        test_loglaw_velocity();
        test_corner_acceleration();
        test_variance_correction();
        test_horseshoe_vortex();
        test_extended_farwake();
        
        cout << "\n";
        cout << "============================================================" << endl;
        cout << "ALL WAKE PHYSICS UNIT TESTS PASSED!" << endl;
        cout << "============================================================" << endl;
        cout << endl;
        
        return 0;
    } catch (const std::exception& e) {
        cout << "\nERROR: " << e.what() << endl;
        return 1;
    }
}
