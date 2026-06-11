// ============================================================================
// OutputSpecification.H
// Dynamic output field configuration for Phase 4.2
//
// This header provides a flexible output specification system that allows
// CSV output to include conditional fields based on enabled capabilities:
//   - Chemistry species (SO2, Sulfate, NOx, HNO3, Nitrate, etc.)
//   - Visibility metrics (b_ext, visual_range, deciview)
//   - Deposition flux (particle_deposition_flux)
//
// The specification is configured in inputs.i and serialized to output
// metadata headers for reproducibility.
//
// Usage:
//   OutputSpecification spec;
//   spec.enable_chemistry = true;
//   spec.chemistry_species = {"SO2", "Sulfate", "NOx", "HNO3", "Nitrate"};
//   spec.enable_visibility = true;
//   spec.enable_deposition = true;
//   
//   // Write header
//   std::string header = spec.generate_csv_header(
//       {"x", "y", "z", "C_total"});  // Base fields always present
//   
//   // Write metadata
//   std::string metadata = spec.generate_metadata();
//
// ============================================================================

#ifndef OUTPUT_SPECIFICATION_H
#define OUTPUT_SPECIFICATION_H

#include <string>
#include <vector>
#include <sstream>
#include <iomanip>

namespace OutputSpec {

// Enumeration for output field categories
enum class FieldCategory {
    WIND,           // Wind components and speed
    PRESSURE,       // Pressure/lambda fields
    CHEMISTRY,      // Chemistry species concentrations
    VISIBILITY,     // Visibility metrics (b_ext, VR, dV)
    DEPOSITION,     // Deposition flux fields
    TERRAIN,        // Terrain-related fields
    QUALITY_FLAG    // Data quality flags
};

// Structure defining output field metadata
struct OutputField {
    std::string name;           // Field name (e.g., "SO2", "b_ext")
    std::string unit;           // Physical unit (e.g., "µg/m³", "km")
    FieldCategory category;     // Field category
    std::string description;    // Human-readable description
    
    OutputField() = default;
    OutputField(const std::string& n, const std::string& u,
                FieldCategory c, const std::string& d)
        : name(n), unit(u), category(c), description(d) {}
};

// ============================================================================
// OutputSpecification Class
// ============================================================================
class OutputSpecification {
public:
    // Feature flags (configurable)
    bool enable_wind_components = true;      // u, v, w (always output)
    bool enable_wind_speed = true;           // |U| (always output)
    bool enable_pressure = false;            // lambda, div fields
    bool enable_terrain = false;             // terrain_z, slope, z0
    bool enable_chemistry = false;           // Chemistry species
    bool enable_visibility = false;          // Visibility metrics
    bool enable_deposition = false;          // Deposition flux
    bool enable_quality_flags = false;       // QA/QC flags

    // Configurable chemistry species
    std::vector<std::string> chemistry_species = {
        "SO2", "Sulfate", "NOx", "HNO3", "Nitrate"
    };

    // Visibility metric selections
    bool output_b_ext = true;        // Extinction coefficient
    bool output_visual_range = true; // Visual range (km)
    bool output_deciview = true;     // Deciview
    bool output_fog_prob = false;    // Fog probability
    bool output_icing_prob = false;  // Icing probability

    // Deposition flux options
    bool output_dry_flux = true;     // Dry deposition flux
    bool output_wet_flux = true;     // Wet deposition flux
    std::vector<std::string> deposition_species;  // Species for deposition

    // Quality flags
    bool output_puff_count = false;  // Number of puffs contributing
    bool output_concentration_std = false;  // Std dev of concentration

public:
    // Constructor
    OutputSpecification() = default;

    // Generate CSV header from output specification
    std::string generate_csv_header(
        const std::vector<std::string>& base_fields = 
            {"name", "x", "y", "z", "C_total"}) const;

    // Generate metadata lines (CSV comment format)
    std::string generate_metadata() const;

    // Parse specification from input string (inputs.i format)
    static OutputSpecification from_string(const std::string& spec_str);

    // Get list of all fields that will be output
    std::vector<OutputField> get_output_fields() const;

    // Get field index by name (-1 if not present)
    int get_field_index(const std::string& field_name) const;

    // Check if field will be output
    bool has_field(const std::string& field_name) const;

    // Validation: check consistency
    bool validate() const;

    // Get total number of output columns (excluding base fields)
    int get_num_output_fields() const;
};

// ============================================================================
// Inline Implementations
// ============================================================================

inline std::string OutputSpecification::generate_csv_header(
    const std::vector<std::string>& base_fields) const
{
    std::ostringstream oss;
    
    // Always output base fields first (e.g., "name,x,y,z,C_total")
    for (size_t i = 0; i < base_fields.size(); ++i) {
        if (i > 0) oss << ",";
        oss << base_fields[i];
    }
    
    // Chemistry species
    if (enable_chemistry) {
        for (const auto& spec : chemistry_species) {
            oss << "," << spec;
        }
    }
    
    // Visibility metrics
    if (enable_visibility) {
        if (output_b_ext) oss << ",b_ext";
        if (output_visual_range) oss << ",visual_range";
        if (output_deciview) oss << ",deciview";
        if (output_fog_prob) oss << ",fog_probability";
        if (output_icing_prob) oss << ",icing_probability";
    }
    
    // Deposition flux
    if (enable_deposition) {
        if (output_dry_flux) {
            for (const auto& spec : deposition_species) {
                oss << ",dry_flux_" << spec;
            }
        }
        if (output_wet_flux) {
            for (const auto& spec : deposition_species) {
                oss << ",wet_flux_" << spec;
            }
        }
    }
    
    // Pressure fields
    if (enable_pressure) {
        oss << ",lambda,div_before,div_after";
    }
    
    // Terrain fields
    if (enable_terrain) {
        oss << ",terrain_z,terrain_slope,z0_adaptive";
    }
    
    // Quality flags
    if (enable_quality_flags) {
        if (output_puff_count) oss << ",puff_count";
        if (output_concentration_std) oss << ",concentration_std";
    }
    
    return oss.str();
}

inline std::string OutputSpecification::generate_metadata() const
{
    std::ostringstream oss;
    oss << std::scientific << std::setprecision(6);
    
    // Feature flags
    oss << "# === Output Specification Metadata ===\n";
    oss << "# enable_wind_components: " << (enable_wind_components ? "true" : "false") << "\n";
    oss << "# enable_wind_speed: " << (enable_wind_speed ? "true" : "false") << "\n";
    oss << "# enable_pressure: " << (enable_pressure ? "true" : "false") << "\n";
    oss << "# enable_terrain: " << (enable_terrain ? "true" : "false") << "\n";
    oss << "# enable_chemistry: " << (enable_chemistry ? "true" : "false") << "\n";
    
    if (enable_chemistry) {
        oss << "# chemistry_species: ";
        for (size_t i = 0; i < chemistry_species.size(); ++i) {
            if (i > 0) oss << ",";
            oss << chemistry_species[i];
        }
        oss << "\n";
    }
    
    oss << "# enable_visibility: " << (enable_visibility ? "true" : "false") << "\n";
    if (enable_visibility) {
        oss << "#   b_ext: " << (output_b_ext ? "yes" : "no") << "\n";
        oss << "#   visual_range: " << (output_visual_range ? "yes" : "no") << "\n";
        oss << "#   deciview: " << (output_deciview ? "yes" : "no") << "\n";
        oss << "#   fog_probability: " << (output_fog_prob ? "yes" : "no") << "\n";
        oss << "#   icing_probability: " << (output_icing_prob ? "yes" : "no") << "\n";
    }
    
    oss << "# enable_deposition: " << (enable_deposition ? "true" : "false") << "\n";
    if (enable_deposition) {
        oss << "#   dry_flux: " << (output_dry_flux ? "yes" : "no") << "\n";
        oss << "#   wet_flux: " << (output_wet_flux ? "yes" : "no") << "\n";
        oss << "#   species: ";
        for (size_t i = 0; i < deposition_species.size(); ++i) {
            if (i > 0) oss << ",";
            oss << deposition_species[i];
        }
        oss << "\n";
    }
    
    oss << "# enable_quality_flags: " << (enable_quality_flags ? "true" : "false") << "\n";
    oss << "# === End Metadata ===\n";
    
    return oss.str();
}

inline std::vector<OutputField> OutputSpecification::get_output_fields() const
{
    std::vector<OutputField> fields;
    
    // Base fields (always present)
    fields.push_back(OutputField("x", "m", FieldCategory::TERRAIN,
        "X-coordinate (Easting)"));
    fields.push_back(OutputField("y", "m", FieldCategory::TERRAIN,
        "Y-coordinate (Northing)"));
    fields.push_back(OutputField("z", "m", FieldCategory::TERRAIN,
        "Z-coordinate (Height above ground)"));
    fields.push_back(OutputField("C_total", "µg/m³", FieldCategory::CHEMISTRY,
        "Total concentration"));
    
    // Wind fields
    if (enable_wind_components) {
        fields.push_back(OutputField("u", "m/s", FieldCategory::WIND, "U-component"));
        fields.push_back(OutputField("v", "m/s", FieldCategory::WIND, "V-component"));
        fields.push_back(OutputField("w", "m/s", FieldCategory::WIND, "W-component"));
    }
    if (enable_wind_speed) {
        fields.push_back(OutputField("wind_speed", "m/s", FieldCategory::WIND,
            "Horizontal wind speed"));
    }
    
    // Chemistry species
    if (enable_chemistry) {
        for (const auto& spec : chemistry_species) {
            fields.push_back(OutputField(spec, "µg/m³", FieldCategory::CHEMISTRY,
                "Concentration of " + spec));
        }
    }
    
    // Visibility
    if (enable_visibility) {
        if (output_b_ext) {
            fields.push_back(OutputField("b_ext", "1/Mm", FieldCategory::VISIBILITY,
                "Extinction coefficient"));
        }
        if (output_visual_range) {
            fields.push_back(OutputField("visual_range", "km", FieldCategory::VISIBILITY,
                "Visual range"));
        }
        if (output_deciview) {
            fields.push_back(OutputField("deciview", "dv", FieldCategory::VISIBILITY,
                "Deciview"));
        }
        if (output_fog_prob) {
            fields.push_back(OutputField("fog_probability", "fraction", FieldCategory::VISIBILITY,
                "Fog occurrence probability"));
        }
        if (output_icing_prob) {
            fields.push_back(OutputField("icing_probability", "fraction", FieldCategory::VISIBILITY,
                "Icing occurrence probability"));
        }
    }
    
    // Deposition
    if (enable_deposition) {
        if (output_dry_flux) {
            for (const auto& spec : deposition_species) {
                fields.push_back(OutputField("dry_flux_" + spec, "µg/(m²·s)",
                    FieldCategory::DEPOSITION,
                    "Dry deposition flux of " + spec));
            }
        }
        if (output_wet_flux) {
            for (const auto& spec : deposition_species) {
                fields.push_back(OutputField("wet_flux_" + spec, "µg/(m²·s)",
                    FieldCategory::DEPOSITION,
                    "Wet deposition flux of " + spec));
            }
        }
    }
    
    return fields;
}

inline int OutputSpecification::get_field_index(const std::string& field_name) const
{
    auto fields = get_output_fields();
    for (size_t i = 0; i < fields.size(); ++i) {
        if (fields[i].name == field_name) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

inline bool OutputSpecification::has_field(const std::string& field_name) const
{
    return get_field_index(field_name) >= 0;
}

inline bool OutputSpecification::validate() const
{
    // Check for invalid combinations
    if (enable_visibility && !enable_chemistry) {
        // Visibility metrics depend on chemistry data
        // For now, allow visibility without chemistry (compute from total concentration)
    }
    
    if (enable_deposition && deposition_species.empty()) {
        // Need at least one deposition species
        if (output_dry_flux || output_wet_flux) {
            return false;
        }
    }
    
    return true;
}

inline int OutputSpecification::get_num_output_fields() const
{
    return static_cast<int>(get_output_fields().size());
}

} // namespace OutputSpec

#endif // OUTPUT_SPECIFICATION_H
