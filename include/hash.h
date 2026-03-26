/* [Titan Core] Component: hash.h | Role: Cryptographic hashing utilities for verifying packet and data integrity. */
#pragma once
#include <string>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <cstdint>

// FNV-1a 64-bit hash: Buffered for high-speed disk streaming.
inline std::string ComputeFileHash(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) return "ERROR";
    
    uint64_t hash = 14695981039346656037ULL;
    
    const size_t bufferSize = 8192; // 8KB buffer
    char buffer[bufferSize];
    
    while (file.read(buffer, bufferSize) || file.gcount() > 0) {
        size_t bytesRead = file.gcount();
        for (size_t i = 0; i < bytesRead; ++i) {
            hash ^= static_cast<unsigned char>(buffer[i]);
            hash *= 1099511628211ULL;
        }
    }
    
    std::stringstream ss;
    ss << std::hex << std::setw(16) << std::setfill('0') << hash;
    return ss.str();
}