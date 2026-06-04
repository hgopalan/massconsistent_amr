// ============================================================================
// landuse_roughness.H
// Land-use classification and aerodynamic roughness lookup table
//
// This header provides functions for mapping land-use categories to
// aerodynamic roughness lengths (z0) using standard atmospheric science
// classifications.
//
// Land-use categories are based on NLCD (National Land Cover Database)
// and IGBP (International Geosphere-Biosphere Programme) classifications.
//
// Standard z0 values by category (Oke, 1987):
//   - Water (ocean, lakes, rivers): 0.0001 - 0.001 m
//   - Grassland (short grass):      0.01 - 0.05 m
//   - Grassland (tall grass):       0.05 - 0.1 m
//   - Shrubland:                    0.1 - 0.3 m
//   - Cropland:                     0.05 - 0.15 m
//   - Deciduous forest:             0.5 - 1.5 m
//   - Evergreen forest:             0.5 - 2.0 m
//   - Mixed forest:                 0.5 - 1.5 m
//   - Urban/built-up:               1.0 - 2.5 m
//   - Barren/rock:                  0.01 - 0.05 m
//
// References:
// - Oke, T. R. (1987). Boundary Layer Climates (2nd ed.). Routledge.
// - Jackson, P. S., & Hunt, J. C. R. (1975). Turbulent wind flow over
//   a low hill. Quarterly Journal of the Royal Meteorological Society, 101(430).
// ============================================================================

#ifndef LANDUSE_ROUGHNESS_H
#define LANDUSE_ROUGHNESS_H

#include <AMReX_REAL.H>
#include <AMReX_GpuQualifiers.H>

// Land-use classification codes (NLCD-compatible enumeration)
enum class LandUseCategory : int {
    WATER = 11,              // Open water, ocean, lakes
    DEVELOPED_OPEN = 21,     // Developed open space (lawns, parks)
    DEVELOPED_LOW = 22,      // Developed low-density (urban, sparse)
    DEVELOPED_MED = 23,      // Developed medium-density
    DEVELOPED_HIGH = 24,     // Developed high-density (downtown)
    BARREN = 31,             // Barren land (rock, sand, gravel)
    GRASSLAND_HERBACEOUS = 71,  // Herbaceous grassland
    PASTURE_HAY = 81,        // Pasture or hay
    CULTIVATED_CROPS = 82,   // Cultivated crops
    WOODY_WETLAND = 90,      // Woody wetland (swamp)
    HERBACEOUS_WETLAND = 95, // Herbaceous wetland (marsh)
    SHRUBLAND = 52,          // Shrubland (scrub)
    DECIDUOUS_FOREST = 41,   // Deciduous forest
    EVERGREEN_FOREST = 42,   // Evergreen forest
    MIXED_FOREST = 43,       // Mixed forest
    UNCLASSIFIED = -1        // No data / unclassified
};

// ============================================================================
// Get representative aerodynamic roughness for a land-use category
//
// Returns the midpoint z0 value for the given land-use category.
// These values are based on literature reviews (Oke 1987, Jackson & Hunt 1975).
//
// Inputs:
//   category    - Land-use category (LandUseCategory enum value)
//
// Returns:
//   z0          - Aerodynamic roughness length [m]
//                 Returns default 0.1 m if category unrecognized
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real get_z0_from_landuse(int category)
{
    switch (category) {
        case static_cast<int>(LandUseCategory::WATER):
            return amrex::Real(0.0005);  // Smooth water surface
        case static_cast<int>(LandUseCategory::BARREN):
            return amrex::Real(0.02);    // Barren rock/sand
        case static_cast<int>(LandUseCategory::GRASSLAND_HERBACEOUS):
            return amrex::Real(0.04);    // Short grass
        case static_cast<int>(LandUseCategory::PASTURE_HAY):
            return amrex::Real(0.08);    // Taller grass
        case static_cast<int>(LandUseCategory::CULTIVATED_CROPS):
            return amrex::Real(0.10);    // Crops
        case static_cast<int>(LandUseCategory::SHRUBLAND):
            return amrex::Real(0.20);    // Shrubs
        case static_cast<int>(LandUseCategory::DEVELOPED_OPEN):
            return amrex::Real(0.15);    // Open developed (parks)
        case static_cast<int>(LandUseCategory::DEVELOPED_LOW):
            return amrex::Real(0.40);    // Low-density urban
        case static_cast<int>(LandUseCategory::DEVELOPED_MED):
            return amrex::Real(0.80);    // Medium-density urban
        case static_cast<int>(LandUseCategory::DEVELOPED_HIGH):
            return amrex::Real(1.50);    // High-density urban
        case static_cast<int>(LandUseCategory::DECIDUOUS_FOREST):
            return amrex::Real(0.80);    // Deciduous trees
        case static_cast<int>(LandUseCategory::EVERGREEN_FOREST):
            return amrex::Real(1.20);    // Evergreen trees
        case static_cast<int>(LandUseCategory::MIXED_FOREST):
            return amrex::Real(1.00);    // Mixed forest
        case static_cast<int>(LandUseCategory::WOODY_WETLAND):
            return amrex::Real(0.50);    // Wetland with trees
        case static_cast<int>(LandUseCategory::HERBACEOUS_WETLAND):
            return amrex::Real(0.10);    // Marsh/herbaceous wetland
        default:
            return amrex::Real(0.10);    // Default grassland
    }
}

// ============================================================================
// Get albedo for a land-use category
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real get_albedo_from_landuse(int category)
{
    switch (category) {
        case static_cast<int>(LandUseCategory::WATER):
            return amrex::Real(0.08);
        case static_cast<int>(LandUseCategory::BARREN):
            return amrex::Real(0.25);
        case static_cast<int>(LandUseCategory::GRASSLAND_HERBACEOUS):
            return amrex::Real(0.20);
        case static_cast<int>(LandUseCategory::PASTURE_HAY):
            return amrex::Real(0.20);
        case static_cast<int>(LandUseCategory::CULTIVATED_CROPS):
            return amrex::Real(0.18);
        case static_cast<int>(LandUseCategory::SHRUBLAND):
            return amrex::Real(0.18);
        case static_cast<int>(LandUseCategory::DEVELOPED_OPEN):
            return amrex::Real(0.15);
        case static_cast<int>(LandUseCategory::DEVELOPED_LOW):
            return amrex::Real(0.15);
        case static_cast<int>(LandUseCategory::DEVELOPED_MED):
            return amrex::Real(0.14);
        case static_cast<int>(LandUseCategory::DEVELOPED_HIGH):
            return amrex::Real(0.12);
        case static_cast<int>(LandUseCategory::DECIDUOUS_FOREST):
            return amrex::Real(0.16);
        case static_cast<int>(LandUseCategory::EVERGREEN_FOREST):
            return amrex::Real(0.12);
        case static_cast<int>(LandUseCategory::MIXED_FOREST):
            return amrex::Real(0.14);
        case static_cast<int>(LandUseCategory::WOODY_WETLAND):
            return amrex::Real(0.14);
        case static_cast<int>(LandUseCategory::HERBACEOUS_WETLAND):
            return amrex::Real(0.14);
        default:
            return amrex::Real(0.15);
    }
}

// ============================================================================
// Get Bowen ratio for a land-use category
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real get_bowen_ratio_from_landuse(int category)
{
    switch (category) {
        case static_cast<int>(LandUseCategory::WATER):
            return amrex::Real(0.10);
        case static_cast<int>(LandUseCategory::BARREN):
            return amrex::Real(4.00);
        case static_cast<int>(LandUseCategory::GRASSLAND_HERBACEOUS):
            return amrex::Real(1.00);
        case static_cast<int>(LandUseCategory::PASTURE_HAY):
            return amrex::Real(0.80);
        case static_cast<int>(LandUseCategory::CULTIVATED_CROPS):
            return amrex::Real(0.50);
        case static_cast<int>(LandUseCategory::SHRUBLAND):
            return amrex::Real(1.50);
        case static_cast<int>(LandUseCategory::DEVELOPED_OPEN):
            return amrex::Real(1.50);
        case static_cast<int>(LandUseCategory::DEVELOPED_LOW):
            return amrex::Real(2.00);
        case static_cast<int>(LandUseCategory::DEVELOPED_MED):
            return amrex::Real(3.00);
        case static_cast<int>(LandUseCategory::DEVELOPED_HIGH):
            return amrex::Real(4.00);
        case static_cast<int>(LandUseCategory::DECIDUOUS_FOREST):
            return amrex::Real(0.30);
        case static_cast<int>(LandUseCategory::EVERGREEN_FOREST):
            return amrex::Real(0.50);
        case static_cast<int>(LandUseCategory::MIXED_FOREST):
            return amrex::Real(0.40);
        case static_cast<int>(LandUseCategory::WOODY_WETLAND):
            return amrex::Real(0.20);
        case static_cast<int>(LandUseCategory::HERBACEOUS_WETLAND):
            return amrex::Real(0.20);
        default:
            return amrex::Real(1.00);
    }
}

// ============================================================================
// Get soil moisture for a land-use category
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real get_soil_moisture_from_landuse(int category)
{
    switch (category) {
        case static_cast<int>(LandUseCategory::WATER):
            return amrex::Real(1.00);
        case static_cast<int>(LandUseCategory::BARREN):
            return amrex::Real(0.05);
        case static_cast<int>(LandUseCategory::GRASSLAND_HERBACEOUS):
            return amrex::Real(0.15);
        case static_cast<int>(LandUseCategory::PASTURE_HAY):
            return amrex::Real(0.20);
        case static_cast<int>(LandUseCategory::CULTIVATED_CROPS):
            return amrex::Real(0.25);
        case static_cast<int>(LandUseCategory::SHRUBLAND):
            return amrex::Real(0.10);
        case static_cast<int>(LandUseCategory::DEVELOPED_OPEN):
            return amrex::Real(0.15);
        case static_cast<int>(LandUseCategory::DEVELOPED_LOW):
            return amrex::Real(0.10);
        case static_cast<int>(LandUseCategory::DEVELOPED_MED):
            return amrex::Real(0.05);
        case static_cast<int>(LandUseCategory::DEVELOPED_HIGH):
            return amrex::Real(0.02);
        case static_cast<int>(LandUseCategory::DECIDUOUS_FOREST):
            return amrex::Real(0.25);
        case static_cast<int>(LandUseCategory::EVERGREEN_FOREST):
            return amrex::Real(0.25);
        case static_cast<int>(LandUseCategory::MIXED_FOREST):
            return amrex::Real(0.25);
        case static_cast<int>(LandUseCategory::WOODY_WETLAND):
            return amrex::Real(0.80);
        case static_cast<int>(LandUseCategory::HERBACEOUS_WETLAND):
            return amrex::Real(0.90);
        default:
            return amrex::Real(0.15);
    }
}

// ============================================================================
// Get emissivity for a land-use category
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real get_emissivity_from_landuse(int category)
{
    switch (category) {
        case static_cast<int>(LandUseCategory::WATER):
            return amrex::Real(0.98);
        case static_cast<int>(LandUseCategory::BARREN):
            return amrex::Real(0.90);
        case static_cast<int>(LandUseCategory::GRASSLAND_HERBACEOUS):
            return amrex::Real(0.95);
        case static_cast<int>(LandUseCategory::PASTURE_HAY):
            return amrex::Real(0.96);
        case static_cast<int>(LandUseCategory::CULTIVATED_CROPS):
            return amrex::Real(0.96);
        case static_cast<int>(LandUseCategory::SHRUBLAND):
            return amrex::Real(0.93);
        case static_cast<int>(LandUseCategory::DEVELOPED_OPEN):
            return amrex::Real(0.95);
        case static_cast<int>(LandUseCategory::DEVELOPED_LOW):
            return amrex::Real(0.93);
        case static_cast<int>(LandUseCategory::DEVELOPED_MED):
            return amrex::Real(0.91);
        case static_cast<int>(LandUseCategory::DEVELOPED_HIGH):
            return amrex::Real(0.90);
        case static_cast<int>(LandUseCategory::DECIDUOUS_FOREST):
            return amrex::Real(0.97);
        case static_cast<int>(LandUseCategory::EVERGREEN_FOREST):
            return amrex::Real(0.97);
        case static_cast<int>(LandUseCategory::MIXED_FOREST):
            return amrex::Real(0.97);
        case static_cast<int>(LandUseCategory::WOODY_WETLAND):
            return amrex::Real(0.98);
        case static_cast<int>(LandUseCategory::HERBACEOUS_WETLAND):
            return amrex::Real(0.98);
        default:
            return amrex::Real(0.95);
    }
}

// ============================================================================
// Get descriptive name for land-use category (for output/debugging)
// Note: Returns C++ string, suitable for host code only (not GPU kernels)
// ============================================================================
inline const char* get_landuse_name(int category)
{
    switch (category) {
        case static_cast<int>(LandUseCategory::WATER):
            return "Water";
        case static_cast<int>(LandUseCategory::BARREN):
            return "Barren";
        case static_cast<int>(LandUseCategory::GRASSLAND_HERBACEOUS):
            return "Grassland";
        case static_cast<int>(LandUseCategory::PASTURE_HAY):
            return "Pasture/Hay";
        case static_cast<int>(LandUseCategory::CULTIVATED_CROPS):
            return "Crops";
        case static_cast<int>(LandUseCategory::SHRUBLAND):
            return "Shrubland";
        case static_cast<int>(LandUseCategory::DEVELOPED_OPEN):
            return "Developed-Open";
        case static_cast<int>(LandUseCategory::DEVELOPED_LOW):
            return "Developed-Low";
        case static_cast<int>(LandUseCategory::DEVELOPED_MED):
            return "Developed-Med";
        case static_cast<int>(LandUseCategory::DEVELOPED_HIGH):
            return "Developed-High";
        case static_cast<int>(LandUseCategory::DECIDUOUS_FOREST):
            return "Deciduous-Forest";
        case static_cast<int>(LandUseCategory::EVERGREEN_FOREST):
            return "Evergreen-Forest";
        case static_cast<int>(LandUseCategory::MIXED_FOREST):
            return "Mixed-Forest";
        case static_cast<int>(LandUseCategory::WOODY_WETLAND):
            return "Woody-Wetland";
        case static_cast<int>(LandUseCategory::HERBACEOUS_WETLAND):
            return "Herbaceous-Wetland";
        default:
            return "Unclassified";
    }
}

#endif  // LANDUSE_ROUGHNESS_H
