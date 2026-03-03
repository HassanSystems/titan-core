#pragma once
#include <string>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <cstdint>

// FNV-1a 64-bit hash: Fast, low collision, zero dependencies.
inline std::string ComputeFileHash(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) return "ERROR";
    
    uint64_t hash = 14695981039346656037ULL;
    char c;
    while (file.get(c)) {
        hash ^= static_cast<unsigned char>(c);
        hash *= 1099511628211ULL;
    }
    
    std::stringstream ss;
    ss << std::hex << std::setw(16) << std::setfill('0') << hash;
    return ss.str();
}