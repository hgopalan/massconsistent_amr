#ifndef CSV_INPUT_LOADER_H
#define CSV_INPUT_LOADER_H

#include <AMReX_GpuQualifiers.H>
#include <AMReX_Print.H>
#include <AMReX_REAL.H>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <initializer_list>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

struct CSVInputConfig {
    amrex::Real default_source_x = 0.0;
    amrex::Real default_source_y = 0.0;
    amrex::Real default_source_z = 10.0;
    amrex::Real default_emission_rate = 1.0;
    amrex::Real default_emission_duration = 100.0;
    std::string default_source_type = "point";
    amrex::Real default_stack_diameter = 0.0;
    amrex::Real default_stack_exit_velocity = 0.0;
    amrex::Real default_stack_exit_temperature = 298.15;

    std::string default_profile_id = "profile_1";
    std::string default_stability_class = "D";
    amrex::Real default_met_u = 0.0;
    amrex::Real default_met_v = 0.0;
    amrex::Real default_met_w = 0.0;
    amrex::Real default_met_K_h = 1.0;
    amrex::Real default_met_K_v = 0.5;

    amrex::Real default_receptor_z = 1.5;
    std::string default_receptor_label_prefix = "R";

    amrex::Real default_particle_diameter_um = 1.0;
    amrex::Real default_particle_density = 1000.0;
    amrex::Real default_vd_grass = 0.0;
    amrex::Real default_vd_urban = 0.0;
    amrex::Real default_vd_water = 0.0;
    amrex::Real default_vd_forest = 0.0;

    bool verbose = true;
    bool warn_if_missing = true;
    bool warn_if_empty = true;
};

class CSVInputLoader {
public:
    explicit CSVInputLoader(CSVInputConfig config = CSVInputConfig())
        : m_config(std::move(config)) {}

    std::vector<Source> load_sources_csv(const std::string& filename) const
    {
        std::vector<Source> sources;
        if (filename.empty()) {
            return sources;
        }

        auto rows = read_rows(filename);
        if (rows.empty()) {
            return sources;
        }

        std::vector<std::string> default_header = {
            "source_id", "x", "y", "z", "type", "emission_rate", "emission_duration",
            "stack_diameter", "stack_exit_velocity", "stack_exit_temperature"
        };
        std::vector<std::string> legacy_header = {
            "source_id", "x", "y", "z", "emission_rate", "emission_duration", "type",
            "stack_diameter", "stack_exit_velocity", "stack_exit_temperature"
        };

        auto layout = resolve_layout(rows, default_header, filename,
            {"source_id", "x", "y", "z", "type", "emission_rate", "emission_duration", "stack_diameter", "stack_exit_velocity", "stack_exit_temperature"});
        if (!layout.has_header && !layout.rows.empty() && layout.rows.front().fields.size() >= 7) {
            amrex::Real temp_real = 0.0;
            const bool field4_numeric = parse_real(layout.rows.front().fields[4], temp_real);
            const bool field5_numeric = parse_real(layout.rows.front().fields[5], temp_real);
            const bool field6_numeric = parse_real(layout.rows.front().fields[6], temp_real);
            if (field4_numeric && field5_numeric && !field6_numeric) {
                layout.header = legacy_header;
                layout.header_index = make_header_index(layout.header);
            }
        }

        for (const auto& row : layout.rows) {
            Source src;
            src.source_id = get_string(row, layout.header_index, {"source_id", "source"}, "source_" + std::to_string(sources.size() + 1));
            src.x = get_real(row, layout.header_index, {"x"}, m_config.default_source_x, filename, row.line_number);
            src.y = get_real(row, layout.header_index, {"y"}, m_config.default_source_y, filename, row.line_number);
            src.z = get_real(row, layout.header_index, {"z"}, m_config.default_source_z, filename, row.line_number);
            src.type = normalise_source_type(get_string(row, layout.header_index, {"type", "source_type"}, m_config.default_source_type));
            src.emission_rate = get_real(row, layout.header_index, {"emission_rate", "rate"}, m_config.default_emission_rate, filename, row.line_number);
            src.emission_duration = get_real(row, layout.header_index, {"emission_duration", "duration"}, m_config.default_emission_duration, filename, row.line_number);
            src.stack_diameter = get_real(row, layout.header_index, {"stack_diameter", "diameter"}, m_config.default_stack_diameter, filename, row.line_number);
            src.stack_exit_velocity = get_real(row, layout.header_index, {"stack_exit_velocity", "exit_velocity"}, m_config.default_stack_exit_velocity, filename, row.line_number);
            src.stack_exit_temperature = get_real(row, layout.header_index, {"stack_exit_temperature", "exit_temperature"}, m_config.default_stack_exit_temperature, filename, row.line_number);

            if (!validate_source(src, filename, row.line_number)) {
                continue;
            }
            sources.push_back(src);
        }

        if (m_config.verbose && !sources.empty()) {
            amrex::Print() << "CSVInputLoader: loaded " << sources.size() << " source record(s) from " << filename << "\n";
        }
        return sources;
    }

    std::map<amrex::Real, amrex::Real> load_emissions_timeseries_csv(const std::string& filename) const
    {
        std::map<amrex::Real, amrex::Real> series;
        if (filename.empty()) {
            return series;
        }

        auto layout = resolve_layout(read_rows(filename), {"time", "emission_rate"}, filename,
            {"time", "emission_rate"});
        for (const auto& row : layout.rows) {
            amrex::Real time_value = get_real(row, layout.header_index, {"time", "t"}, std::numeric_limits<amrex::Real>::quiet_NaN(), filename, row.line_number);
            amrex::Real rate_value = get_real(row, layout.header_index, {"emission_rate", "rate"}, m_config.default_emission_rate, filename, row.line_number);
            if (!(time_value == time_value) || time_value < 0.0 || rate_value < 0.0) {
                warn(filename, row.line_number, "invalid emission time series entry; expected non-negative time and rate");
                continue;
            }
            series[time_value] = rate_value;
        }

        if (m_config.verbose && !series.empty()) {
            amrex::Print() << "CSVInputLoader: loaded " << series.size() << " emission time point(s) from " << filename << "\n";
        }
        return series;
    }

    std::map<std::string, DepositionParams> load_deposition_params_csv(const std::string& filename) const
    {
        std::map<std::string, DepositionParams> params;
        if (filename.empty()) {
            return params;
        }

        auto layout = resolve_layout(read_rows(filename),
            {"particle_class", "diameter", "density", "vd_grass", "vd_urban", "vd_water", "vd_forest"},
            filename,
            {"particle_class", "diameter", "density", "vd_grass", "vd_urban", "vd_water", "vd_forest"});

        for (const auto& row : layout.rows) {
            DepositionParams dep;
            dep.particle_class = get_string(row, layout.header_index, {"particle_class", "species", "species_id"}, "");
            dep.diameter_um = get_real(row, layout.header_index, {"diameter", "diameter_um"}, m_config.default_particle_diameter_um, filename, row.line_number);
            dep.density = get_real(row, layout.header_index, {"density", "particle_density"}, m_config.default_particle_density, filename, row.line_number);
            dep.vd_grass = get_real(row, layout.header_index, {"vd_grass", "grass"}, m_config.default_vd_grass, filename, row.line_number);
            dep.vd_urban = get_real(row, layout.header_index, {"vd_urban", "urban"}, m_config.default_vd_urban, filename, row.line_number);
            dep.vd_water = get_real(row, layout.header_index, {"vd_water", "water"}, m_config.default_vd_water, filename, row.line_number);
            dep.vd_forest = get_real(row, layout.header_index, {"vd_forest", "forest"}, m_config.default_vd_forest, filename, row.line_number);

            if (!validate_deposition(dep, filename, row.line_number)) {
                continue;
            }
            params[dep.particle_class] = dep;
        }

        if (m_config.verbose && !params.empty()) {
            amrex::Print() << "CSVInputLoader: loaded " << params.size() << " deposition parameter set(s) from " << filename << "\n";
        }
        return params;
    }

    std::vector<MetProfile> load_met_profiles_csv(const std::string& filename) const
    {
        std::vector<MetProfile> profiles;
        if (filename.empty()) {
            return profiles;
        }

        auto layout = resolve_layout(read_rows(filename),
            {"profile_id", "x_ref", "y_ref", "z_agl", "u", "v", "w", "K_h", "K_v", "stability_class"},
            filename,
            {"profile_id", "x_ref", "y_ref", "z_agl", "u", "v", "w", "K_h", "K_v", "stability_class"});

        std::map<std::string, MetProfile> profile_map;
        for (const auto& row : layout.rows) {
            const std::string profile_id = get_string(row, layout.header_index, {"profile_id", "profile"}, m_config.default_profile_id);
            const amrex::Real x_ref = get_real(row, layout.header_index, {"x_ref", "x"}, 0.0, filename, row.line_number);
            const amrex::Real y_ref = get_real(row, layout.header_index, {"y_ref", "y"}, 0.0, filename, row.line_number);
            const amrex::Real z_agl = get_real(row, layout.header_index, {"z_agl", "z"}, 0.0, filename, row.line_number);
            const amrex::Real u = get_real(row, layout.header_index, {"u"}, m_config.default_met_u, filename, row.line_number);
            const amrex::Real v = get_real(row, layout.header_index, {"v"}, m_config.default_met_v, filename, row.line_number);
            const amrex::Real w = get_real(row, layout.header_index, {"w"}, m_config.default_met_w, filename, row.line_number);
            const amrex::Real K_h = get_real(row, layout.header_index, {"K_h", "kh"}, m_config.default_met_K_h, filename, row.line_number);
            const amrex::Real K_v = get_real(row, layout.header_index, {"K_v", "kv"}, m_config.default_met_K_v, filename, row.line_number);
            const std::string stability = get_string(row, layout.header_index, {"stability_class", "stability"}, m_config.default_stability_class);

            if (profile_id.empty() || z_agl < 0.0 || K_h < 0.0 || K_v < 0.0) {
                warn(filename, row.line_number, "invalid meteorology profile entry; expected non-empty profile_id and non-negative z_agl/K values");
                continue;
            }

            MetProfile& prof = profile_map[profile_id];
            if (prof.z_agl.empty()) {
                prof.profile_id = profile_id;
                prof.x_ref = x_ref;
                prof.y_ref = y_ref;
                prof.stability_class = stability.empty() ? m_config.default_stability_class : stability;
            }
            prof.z_agl.push_back(z_agl);
            prof.u.push_back(u);
            prof.v.push_back(v);
            prof.w.push_back(w);
            prof.K_h.push_back(K_h);
            prof.K_v.push_back(K_v);
        }

        for (auto& item : profile_map) {
            sort_profile_levels(item.second);
            profiles.push_back(item.second);
        }

        if (m_config.verbose && !profiles.empty()) {
            amrex::Print() << "CSVInputLoader: loaded " << profiles.size() << " meteorology profile(s) from " << filename << "\n";
        }
        return profiles;
    }

    std::vector<Receptor> load_receptors_csv(const std::string& filename) const
    {
        std::vector<Receptor> receptors;
        if (filename.empty()) {
            return receptors;
        }

        auto layout = resolve_layout(read_rows(filename), {"x", "y", "z", "label"}, filename,
            {"x", "y", "z", "label"});
        for (const auto& row : layout.rows) {
            Receptor receptor;
            receptor.x = get_real(row, layout.header_index, {"x"}, 0.0, filename, row.line_number);
            receptor.y = get_real(row, layout.header_index, {"y"}, 0.0, filename, row.line_number);
            receptor.z = get_real(row, layout.header_index, {"z"}, m_config.default_receptor_z, filename, row.line_number);
            receptor.label = get_string(row, layout.header_index, {"label", "name", "receptor_id"},
                                        m_config.default_receptor_label_prefix + "_" + std::to_string(receptors.size() + 1));
            if (!validate_receptor(receptor, filename, row.line_number)) {
                continue;
            }
            receptors.push_back(receptor);
        }

        if (m_config.verbose && !receptors.empty()) {
            amrex::Print() << "CSVInputLoader: loaded " << receptors.size() << " receptor(s) from " << filename << "\n";
        }
        return receptors;
    }

private:
    struct Row {
        int line_number = 0;
        std::vector<std::string> fields;
    };

    struct Layout {
        bool has_header = false;
        std::vector<std::string> header;
        std::map<std::string, std::size_t> header_index;
        std::vector<Row> rows;
    };

    CSVInputConfig m_config;

    AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
    static bool nonnegative(amrex::Real value) noexcept
    {
        return value >= amrex::Real(0.0);
    }

    static void warn(const std::string& filename, int line_number, const std::string& message)
    {
        amrex::Print() << "CSVInputLoader: warning";
        if (!filename.empty()) {
            amrex::Print() << " [" << filename;
            if (line_number > 0) {
                amrex::Print() << ":" << line_number;
            }
            amrex::Print() << "]";
        }
        amrex::Print() << " " << message << "\n";
    }

    static std::string trim_copy(const std::string& value)
    {
        const auto first = value.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) {
            return "";
        }
        const auto last = value.find_last_not_of(" \t\r\n");
        return value.substr(first, last - first + 1);
    }

    static bool is_comment_or_blank(const std::string& line)
    {
        const std::string trimmed = trim_copy(line);
        return trimmed.empty() || trimmed[0] == '#';
    }

    static std::vector<std::string> split_csv_line(const std::string& line)
    {
        std::vector<std::string> tokens;
        std::stringstream ss(line);
        std::string token;
        while (std::getline(ss, token, ',')) {
            tokens.push_back(trim_copy(token));
        }
        if (!line.empty() && line.back() == ',') {
            tokens.emplace_back("");
        }
        return tokens;
    }

    static std::string canonical_key(const std::string& raw)
    {
        std::string key;
        key.reserve(raw.size());
        for (char ch : raw) {
            if (std::isalnum(static_cast<unsigned char>(ch))) {
                key.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
            }
        }
        return key;
    }

    static bool parse_real(const std::string& text, amrex::Real& value)
    {
        const std::string trimmed = trim_copy(text);
        if (trimmed.empty()) {
            return false;
        }
        char* end = nullptr;
        const double parsed = std::strtod(trimmed.c_str(), &end);
        if (end == trimmed.c_str() || *trimmed.c_str() == '\0') {
            return false;
        }
        while (end != nullptr && *end != '\0' && std::isspace(static_cast<unsigned char>(*end))) {
            ++end;
        }
        if (end == nullptr || *end != '\0') {
            return false;
        }
        value = static_cast<amrex::Real>(parsed);
        return true;
    }

    static std::map<std::string, std::size_t> make_header_index(const std::vector<std::string>& header)
    {
        std::map<std::string, std::size_t> index;
        for (std::size_t i = 0; i < header.size(); ++i) {
            index[canonical_key(header[i])] = i;
        }
        return index;
    }

    static bool row_looks_like_header(const Row& row, const std::vector<std::string>& known_keys)
    {
        int hits = 0;
        for (const auto& field : row.fields) {
            const std::string key = canonical_key(field);
            if (std::find(known_keys.begin(), known_keys.end(), key) != known_keys.end()) {
                ++hits;
            }
        }
        return hits > 0;
    }

    std::vector<Row> read_rows(const std::string& filename) const
    {
        std::vector<Row> rows;
        std::ifstream input(filename);
        if (!input.is_open()) {
            if (m_config.warn_if_missing) {
                warn(filename, 0, "file not found; using defaults/empty data");
            }
            return rows;
        }

        std::string line;
        int line_number = 0;
        while (std::getline(input, line)) {
            ++line_number;
            if (is_comment_or_blank(line)) {
                continue;
            }
            Row row;
            row.line_number = line_number;
            row.fields = split_csv_line(line);
            rows.push_back(row);
        }

        if (rows.empty() && m_config.warn_if_empty) {
            warn(filename, 0, "file is empty after removing comments/blank lines");
        }
        return rows;
    }

    Layout resolve_layout(std::vector<Row> rows,
                          const std::vector<std::string>& default_header,
                          const std::string& filename,
                          std::initializer_list<const char*> known_headers) const
    {
        Layout layout;
        if (rows.empty()) {
            layout.header = default_header;
            layout.header_index = make_header_index(layout.header);
            return layout;
        }

        std::vector<std::string> keys;
        for (const char* header : known_headers) {
            keys.push_back(canonical_key(header));
        }

        if (row_looks_like_header(rows.front(), keys)) {
            layout.has_header = true;
            layout.header = rows.front().fields;
            rows.erase(rows.begin());
        } else {
            layout.header = default_header;
        }

        layout.header_index = make_header_index(layout.header);
        layout.rows = std::move(rows);

        if (layout.rows.empty() && m_config.warn_if_empty) {
            warn(filename, 0, "no data rows found");
        }
        return layout;
    }

    static std::size_t missing_index()
    {
        return std::numeric_limits<std::size_t>::max();
    }

    static std::size_t find_index(const std::map<std::string, std::size_t>& header_index,
                                  std::initializer_list<const char*> aliases)
    {
        for (const char* alias : aliases) {
            const auto it = header_index.find(canonical_key(alias));
            if (it != header_index.end()) {
                return it->second;
            }
        }
        return missing_index();
    }

    static std::string get_string(const Row& row,
                                  const std::map<std::string, std::size_t>& header_index,
                                  std::initializer_list<const char*> aliases,
                                  const std::string& default_value)
    {
        const std::size_t index = find_index(header_index, aliases);
        if (index == missing_index() || index >= row.fields.size() || row.fields[index].empty()) {
            return default_value;
        }
        return row.fields[index];
    }

    static amrex::Real get_real(const Row& row,
                                const std::map<std::string, std::size_t>& header_index,
                                std::initializer_list<const char*> aliases,
                                amrex::Real default_value,
                                const std::string& filename,
                                int line_number)
    {
        const std::size_t index = find_index(header_index, aliases);
        if (index == missing_index() || index >= row.fields.size() || row.fields[index].empty()) {
            return default_value;
        }
        amrex::Real value = default_value;
        if (!parse_real(row.fields[index], value)) {
            warn(filename, line_number, "could not parse numeric field '" + row.fields[index] + "'; using default value");
            return default_value;
        }
        return value;
    }

    static std::string normalise_source_type(std::string type)
    {
        std::transform(type.begin(), type.end(), type.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        if (type.empty()) {
            return "point";
        }
        if (type != "point" && type != "stack" && type != "volume" && type != "line") {
            return "point";
        }
        return type;
    }

    static bool validate_source(const Source& src, const std::string& filename, int line_number)
    {
        if (!nonnegative(src.z) || !nonnegative(src.emission_rate) || !nonnegative(src.emission_duration) ||
            !nonnegative(src.stack_diameter) || !nonnegative(src.stack_exit_velocity) || src.stack_exit_temperature <= 0.0) {
            warn(filename, line_number, "invalid source entry; expected non-negative geometry/rates and positive stack temperature");
            return false;
        }
        return true;
    }

    static bool validate_deposition(const DepositionParams& dep, const std::string& filename, int line_number)
    {
        if (dep.particle_class.empty() || dep.diameter_um <= 0.0 || dep.density <= 0.0 ||
            dep.vd_grass < 0.0 || dep.vd_urban < 0.0 || dep.vd_water < 0.0 || dep.vd_forest < 0.0) {
            warn(filename, line_number, "invalid deposition parameters; expected non-empty species and positive diameter/density with non-negative deposition velocities");
            return false;
        }
        return true;
    }

    static bool validate_receptor(const Receptor& receptor, const std::string& filename, int line_number)
    {
        if (receptor.z < 0.0 || receptor.label.empty()) {
            warn(filename, line_number, "invalid receptor entry; expected non-negative z and non-empty label");
            return false;
        }
        return true;
    }

    static void sort_profile_levels(MetProfile& profile)
    {
        std::vector<std::size_t> order(profile.z_agl.size());
        for (std::size_t i = 0; i < order.size(); ++i) {
            order[i] = i;
        }
        std::sort(order.begin(), order.end(), [&](std::size_t lhs, std::size_t rhs) {
            return profile.z_agl[lhs] < profile.z_agl[rhs];
        });

        auto reorder = [&](std::vector<amrex::Real>& values) {
            std::vector<amrex::Real> sorted(values.size());
            for (std::size_t i = 0; i < order.size(); ++i) {
                sorted[i] = values[order[i]];
            }
            values.swap(sorted);
        };

        reorder(profile.z_agl);
        reorder(profile.u);
        reorder(profile.v);
        reorder(profile.w);
        reorder(profile.K_h);
        reorder(profile.K_v);
    }
};

#endif
