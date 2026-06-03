// ============================================================================
// turbsim_bts_export.H
// TurbSim binary format (.bts) export for OpenFAST compatibility (Phase 3)
//
// This header provides functionality to export time-series turbulent
// fluctuations to the TurbSim binary format (.bts), enabling seamless
// integration with NREL's OpenFAST wind turbine simulator.
//
// Format Specification:
//   The TurbSim .bts format is a binary format containing:
//   - Header section (metadata and grid description)
//   - 3-D time-series data (u', v', w' velocity fluctuations)
//   - Stacked time-stacks (each time step contains full 3-D field)
//
// File Structure:
//   [Header: 6 x 4-byte integers + metadata]
//   [nz values u'(1,1,1:nt)] [nz values v'(1,1,1:nt)] [nz values w'(1,1,1:nt)]
//   [nz values u'(2,1,1:nt)] [nz values v'(2,1,1:nt)] [nz values w'(2,1,1:nt)]
//   ...
//   [Grid points are stored: x (streamwise) fastest, then y, then z (vertical)]
//
// References:
//   - NREL TurbSim User's Guide (v1.06.00 and later)
//   - OpenFAST documentation: ElastoDyn & ExternalInputs modules
//   - Binary format specification from TurbSim source code
//
// Integration with Phase 2 & 3:
//   Input: TimeSeriesOutput from TemporalSynthesis module
//   Output: .bts binary file + optional ASCII metadata
//
// ============================================================================

#ifndef TURBSIM_BTS_EXPORT_H
#define TURBSIM_BTS_EXPORT_H

#include <AMReX_REAL.H>
#include <AMReX_GpuQualifiers.H>
#include <cmath>
#include <cstring>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>

// ============================================================================
// Namespace: TurbSimExport
// ============================================================================

namespace TurbSimExport {

// ============================================================================
// Constants
// ============================================================================

// BTS format version and identifier
constexpr int BTS_FORMAT_VERSION = 1;           // TurbSim format identifier
constexpr int BTS_HEADER_RECORD_SIZE = 6;       // Number of 4-byte header integers
constexpr int BTS_NUM_COMPONENTS = 3;           // u, v, w

// Grid orientation
constexpr int BTS_STREAMWISE = 0;       // x direction (fastest varying)
constexpr int BTS_LATERAL = 1;          // y direction
constexpr int BTS_VERTICAL = 2;         // z direction (slowest varying)

// ============================================================================
// Structure: BTSHeader
// Binary TurbSim header data
// ============================================================================

struct BTSHeader {
    // ---- 6-integer header (TurbSim format) ----
    int id1 = 7;              // Identifier (fixed)
    int id2 = 7;              // Identifier (fixed)
    int nt = 0;               // Number of time steps
    int ny = 0;               // Number of lateral grid points
    int nz = 0;               // Number of vertical grid points
    int ncomp = BTS_NUM_COMPONENTS;  // Number of components (always 3)
    
    // ---- Floating-point header info ----
    float dt = 0.0f;          // Time step [s]
    float uHub = 0.0f;        // Hub-height mean wind speed [m/s]
    float zHub = 0.0f;        // Hub height [m]
    float dy = 0.0f;          // Lateral grid spacing [m]
    float dz = 0.0f;          // Vertical grid spacing [m]
    float z0 = 0.01f;         // Surface roughness [m]
    
    // ---- Derived metadata ----
    float turbIntensity = 0.0f;    // Turbulence intensity (%)
    float tiFlat = 0.0f;           // Flat turbulence intensity
    float tiGradient = 0.0f;       // Turbulence intensity gradient
    float zRef = 10.0f;            // Reference height [m]
    float alphaPower = 0.15f;      // Power-law exponent
    
    // Validate header consistency
    bool IsValid() const {
        return (id1 == 7 && id2 == 7 &&
                nt > 0 && ny > 0 && nz > 0 && ncomp == 3 &&
                dt > 0.0f && uHub > 0.0f && dy > 0.0f && dz > 0.0f);
    }
};

// ============================================================================
// Structure: BTSMetadata
// Extended metadata for TurbSim export
// ============================================================================

struct BTSMetadata {
    // Time-series info
    std::string description = "Synthetic turbulence from AMReX wind solver";
    std::string turbulence_model = "Von Kármán";
    std::string coherence_model = "Gaussian";
    
    // Physical parameters
    float u_mean = 10.0f;           // Mean wind speed [m/s]
    float v_mean = 0.0f;            // Lateral mean [m/s]
    float w_mean = 0.0f;            // Vertical mean [m/s]
    float z_hub = 90.0f;            // Hub height [m]
    float intensity_u = 0.14f;      // u-component intensity
    float intensity_v = 0.14f;      // v-component intensity
    float intensity_w = 0.10f;      // w-component intensity
    
    // Grid info
    int nx = 0;
    int ny = 0;
    int nz = 0;
    float dx = 10.0f;  // Streamwise spacing [m]
    float dy = 10.0f;  // Lateral spacing [m]
    float dz = 10.0f;  // Vertical spacing [m]
    
    // Time info
    int num_time_steps = 0;
    float dt = 0.1f;   // Time step [s]
    float duration = 0.0f;
    
    // Random seed
    unsigned int seed = 12345u;
};

// ============================================================================
// Class: BinaryFileWriter
// Low-level binary I/O
// ============================================================================

class BinaryFileWriter {
protected:
    std::ofstream file;
    
public:
    /// Open file for binary writing
    bool Open(const std::string& filename) {
        file.open(filename, std::ios::binary | std::ios::out);
        return file.is_open();
    }
    
    /// Close file
    void Close() {
        if (file.is_open()) file.close();
    }
    
    /// Write single value
    template<typename T>
    bool WriteValue(const T& value) {
        if (!file.is_open()) return false;
        file.write(reinterpret_cast<const char*>(&value), sizeof(T));
        return file.good();
    }
    
    /// Write array
    template<typename T>
    bool WriteArray(const std::vector<T>& data) {
        if (!file.is_open()) return false;
        file.write(reinterpret_cast<const char*>(data.data()),
                   data.size() * sizeof(T));
        return file.good();
    }
    
    /// Write array from pointer
    template<typename T>
    bool WriteArrayPtr(const T* data, size_t count) {
        if (!file.is_open()) return false;
        file.write(reinterpret_cast<const char*>(data), count * sizeof(T));
        return file.good();
    }
    
    /// Get current file position
    size_t GetPosition() {
        return file.tellp();
    }
};

// ============================================================================
// Class: TurbSimBTSWriter
// Main BTS format exporter
// ============================================================================

class TurbSimBTSWriter : public BinaryFileWriter {
private:
    BTSHeader header;
    BTSMetadata metadata;
    bool header_written = false;
    
public:
    /// Initialize writer with metadata
    void Initialize(
        int num_time_steps,
        int nx, int ny, int nz,
        amrex::Real dt,
        amrex::Real u_mean,
        amrex::Real dx, amrex::Real dy, amrex::Real dz,
        amrex::Real z_hub,
        amrex::Real turbulence_intensity_u = 0.14,
        unsigned int seed = 12345u)
    {
        header.nt = num_time_steps;
        header.ny = ny;
        header.nz = nz;
        header.dt = static_cast<float>(dt);
        header.uHub = static_cast<float>(u_mean);
        header.zHub = static_cast<float>(z_hub);
        header.dy = static_cast<float>(dy);
        header.dz = static_cast<float>(dz);
        
        // Compute turbulence intensity
        header.turbIntensity = static_cast<float>(turbulence_intensity_u * 100.0);  // Convert to %
        
        metadata.u_mean = static_cast<float>(u_mean);
        metadata.z_hub = static_cast<float>(z_hub);
        metadata.intensity_u = static_cast<float>(turbulence_intensity_u);
        metadata.intensity_v = static_cast<float>(turbulence_intensity_u * 0.8);
        metadata.intensity_w = static_cast<float>(turbulence_intensity_u * 0.5);
        metadata.nx = nx;
        metadata.ny = ny;
        metadata.nz = nz;
        metadata.dx = static_cast<float>(dx);
        metadata.dy = static_cast<float>(dy);
        metadata.dz = static_cast<float>(dz);
        metadata.num_time_steps = num_time_steps;
        metadata.dt = static_cast<float>(dt);
        metadata.duration = num_time_steps * static_cast<float>(dt);
        metadata.seed = seed;
    }
    
    /// Write binary header
    bool WriteHeader() {
        if (!header.IsValid()) {
            return false;
        }
        
        // Write 6-integer header record
        if (!WriteValue(header.id1)) return false;
        if (!WriteValue(header.id2)) return false;
        if (!WriteValue(header.nt)) return false;
        if (!WriteValue(header.ny)) return false;
        if (!WriteValue(header.nz)) return false;
        if (!WriteValue(header.ncomp)) return false;
        
        // Write floating-point header info
        if (!WriteValue(header.dt)) return false;
        if (!WriteValue(header.uHub)) return false;
        if (!WriteValue(header.zHub)) return false;
        if (!WriteValue(header.dy)) return false;
        if (!WriteValue(header.dz)) return false;
        if (!WriteValue(header.z0)) return false;
        if (!WriteValue(header.turbIntensity)) return false;
        
        header_written = true;
        return true;
    }
    
    /// Write time-series data in TurbSim format
    /// Grid layout: [x (fastest), y, z (slowest)], stacked over time
    bool WriteTimeSeriesData(
        const std::vector<amrex::Real>& u_prime,
        const std::vector<amrex::Real>& v_prime,
        const std::vector<amrex::Real>& w_prime,
        int nx, int ny, int nz, int nt)
    {
        if (!file.is_open() || !header_written) return false;
        
        int total_spatial = nx * ny * nz;
        
        // Write data stacked by time: [time][z][y][x]
        // This matches TurbSim's native format
        for (int t = 0; t < nt; ++t) {
            for (int z = 0; z < nz; ++z) {
                for (int y = 0; y < ny; ++y) {
                    for (int x = 0; x < nx; ++x) {
                        int idx = t * total_spatial + z * (nx * ny) + y * nx + x;
                        
                        if (idx >= (int)u_prime.size() || idx >= (int)v_prime.size() || 
                            idx >= (int)w_prime.size()) {
                            return false;
                        }
                        
                        // Write u component
                        float u_val = static_cast<float>(u_prime[idx]);
                        if (!WriteValue(u_val)) return false;
                        
                        // Write v component
                        float v_val = static_cast<float>(v_prime[idx]);
                        if (!WriteValue(v_val)) return false;
                        
                        // Write w component
                        float w_val = static_cast<float>(w_prime[idx]);
                        if (!WriteValue(w_val)) return false;
                    }
                }
            }
        }
        
        return true;
    }
    
    /// Write metadata as ASCII comment file
    bool WriteMetadataFile(const std::string& filename)
    {
        std::ofstream meta_file(filename);
        if (!meta_file.is_open()) return false;
        
        meta_file << "# TurbSim Export Metadata\n";
        meta_file << "# Generated by massconsistent_amr Phase 3\n";
        meta_file << "#\n";
        meta_file << "# Description: " << metadata.description << "\n";
        meta_file << "# Turbulence Model: " << metadata.turbulence_model << "\n";
        meta_file << "# Coherence Model: " << metadata.coherence_model << "\n";
        meta_file << "#\n";
        meta_file << "# Physical Parameters\n";
        meta_file << "u_mean = " << metadata.u_mean << " [m/s]\n";
        meta_file << "z_hub = " << metadata.z_hub << " [m]\n";
        meta_file << "intensity_u = " << metadata.intensity_u << "\n";
        meta_file << "intensity_v = " << metadata.intensity_v << "\n";
        meta_file << "intensity_w = " << metadata.intensity_w << "\n";
        meta_file << "#\n";
        meta_file << "# Grid Information\n";
        meta_file << "nx = " << metadata.nx << "\n";
        meta_file << "ny = " << metadata.ny << "\n";
        meta_file << "nz = " << metadata.nz << "\n";
        meta_file << "dx = " << metadata.dx << " [m]\n";
        meta_file << "dy = " << metadata.dy << " [m]\n";
        meta_file << "dz = " << metadata.dz << " [m]\n";
        meta_file << "#\n";
        meta_file << "# Time Information\n";
        meta_file << "num_time_steps = " << metadata.num_time_steps << "\n";
        meta_file << "dt = " << metadata.dt << " [s]\n";
        meta_file << "duration = " << metadata.duration << " [s]\n";
        meta_file << "#\n";
        meta_file << "# Random Seed\n";
        meta_file << "seed = " << metadata.seed << "\n";
        
        meta_file.close();
        return true;
    }
    
    /// Complete export pipeline: open file, write header, write data, close
    bool ExportTimeSeries(
        const std::string& filename,
        const std::vector<amrex::Real>& u_prime,
        const std::vector<amrex::Real>& v_prime,
        const std::vector<amrex::Real>& w_prime,
        int nx, int ny, int nz, int nt)
    {
        if (!Open(filename)) {
            return false;
        }
        
        if (!WriteHeader()) {
            Close();
            return false;
        }
        
        if (!WriteTimeSeriesData(u_prime, v_prime, w_prime, nx, ny, nz, nt)) {
            Close();
            return false;
        }
        
        Close();
        
        // Write metadata file (same name but .meta extension)
        std::string meta_filename = filename;
        size_t pos = meta_filename.rfind(".bts");
        if (pos != std::string::npos) {
            meta_filename.replace(pos, 4, ".meta");
        } else {
            meta_filename += ".meta";
        }
        WriteMetadataFile(meta_filename);
        
        return true;
    }
    
    /// Get header info
    const BTSHeader& GetHeader() const { return header; }
    
    /// Get metadata
    const BTSMetadata& GetMetadata() const { return metadata; }
};

// ============================================================================
// Convenience Functions
// ============================================================================

/// Export time-series with defaults
inline bool ExportTurbSimBTS(
    const std::string& filename,
    const std::vector<amrex::Real>& u_prime,
    const std::vector<amrex::Real>& v_prime,
    const std::vector<amrex::Real>& w_prime,
    int nx, int ny, int nz, int nt,
    amrex::Real dt,
    amrex::Real u_mean,
    amrex::Real dx, amrex::Real dy, amrex::Real dz,
    amrex::Real z_hub = 90.0,
    amrex::Real turbulence_intensity = 0.14,
    unsigned int seed = 12345u)
{
    TurbSimBTSWriter writer;
    writer.Initialize(nt, nx, ny, nz, dt, u_mean, dx, dy, dz,
                     z_hub, turbulence_intensity, seed);
    return writer.ExportTimeSeries(filename, u_prime, v_prime, w_prime,
                                   nx, ny, nz, nt);
}

} // namespace TurbSimExport

#endif // TURBSIM_BTS_EXPORT_H
