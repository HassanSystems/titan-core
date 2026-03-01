#pragma once
#include <string>

constexpr int PROTOCOL_VERSION = 1;

enum class PayloadType {
    TEXT = 0,
    COMMAND = 1,
    FILE_META = 2,
    SYSTEM = 3
};

struct Message {
    int protocol;
    PayloadType payload;
    std::string from;
    std::string to;
    std::string body;
};

struct FileMeta {
    std::string filename;
    uint64_t size_bytes;
    std::string sha256;
};

inline std::string SerializeMessage(const Message& msg) {
    return "V:" + std::to_string(msg.protocol) + "\nPTYPE:" + std::to_string(static_cast<int>(msg.payload)) + "\nFROM:" + msg.from + "\nTO:" + msg.to + "\nBODY:" + msg.body + "\n[END]";
}

inline Message ParseMessage(const std::string& raw) {
    Message msg = {-1, PayloadType::TEXT, "", "", ""};
    size_t v_pos = raw.find("V:");
    size_t pt_pos = raw.find("\nPTYPE:");
    size_t f_pos = raw.find("\nFROM:");
    size_t to_pos = raw.find("\nTO:");
    size_t b_pos = raw.find("\nBODY:");
    size_t end_pos = raw.find("\n[END]");

    if (v_pos != std::string::npos && pt_pos != std::string::npos && f_pos != std::string::npos && to_pos != std::string::npos && b_pos != std::string::npos && end_pos != std::string::npos) {
        msg.protocol = std::stoi(raw.substr(v_pos + 2, pt_pos - (v_pos + 2)));
        msg.payload = static_cast<PayloadType>(std::stoi(raw.substr(pt_pos + 7, f_pos - (pt_pos + 7))));
        msg.from = raw.substr(f_pos + 6, to_pos - (f_pos + 6));
        msg.to = raw.substr(to_pos + 4, b_pos - (to_pos + 4));
        msg.body = raw.substr(b_pos + 6, end_pos - (b_pos + 6));
    }
    return msg;
}

inline std::string SerializeFileMeta(const FileMeta& meta) {
    return "{\"filename\":\"" + meta.filename + "\",\"size_bytes\":" + std::to_string(meta.size_bytes) + ",\"sha256\":\"" + meta.sha256 + "\"}";
}