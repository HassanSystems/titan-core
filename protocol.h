#pragma once
#include <string>
#include <cstdint>

constexpr int PROTOCOL_VERSION = 1;
constexpr size_t CHUNK_SIZE = 4096;
constexpr int CHUNK_WINDOW = 50;

enum class PayloadType {
    TEXT = 0,
    COMMAND = 1,
    FILE_META = 2,
    SYSTEM = 3,
    FILE_CHUNK = 4,
    FILE_ACK = 5,
    FILE_ERROR = 6,
    SESSION_ACCEPT = 7,
    SESSION_REJECT = 8,
    MESSAGE_ACK = 9
};

enum class ErrorCode {
    INVALID_META = 0,
    HASH_MISMATCH = 1,
    CHUNK_OUT_OF_ORDER = 2,
    USER_ABORT = 3
};

struct Message {
    int protocol;
    std::string message_id;
    PayloadType payload;
    std::string from;
    std::string to;
    std::string body;
    uint64_t session_id;
};

struct MsgAck {
    std::string message_id;
};

struct FileMeta {
    std::string transfer_id;
    std::string filename;
    uint64_t size_bytes;
    std::string sha256;
};

struct FileChunk {
    std::string transfer_id;
    std::string filename;
    uint64_t index;
    uint64_t total_chunks;
    std::string data_base64;
};

struct FileAck {
    std::string transfer_id;
};

struct FileError {
    std::string transfer_id;
    ErrorCode code;
    std::string message;
};

inline std::string SerializeMessage(const Message& msg) {
    return "V:" + std::to_string(msg.protocol) + "\nMSGID:" + msg.message_id + "\nPTYPE:" + std::to_string(static_cast<int>(msg.payload)) + "\nFROM:" + msg.from + "\nTO:" + msg.to + "\nSESSION:" + std::to_string(msg.session_id) + "\nBODY:" + msg.body + "\n[END]";
}

inline Message ParseMessage(const std::string& raw) {
    Message msg = {-1, "", PayloadType::TEXT, "", "", "", 0};
    size_t v_pos = raw.find("V:");
    size_t m_pos = raw.find("\nMSGID:");
    size_t pt_pos = raw.find("\nPTYPE:");
    size_t f_pos = raw.find("\nFROM:");
    size_t to_pos = raw.find("\nTO:");
    size_t s_pos = raw.find("\nSESSION:");
    size_t b_pos = raw.find("\nBODY:");
    size_t end_pos = raw.find("\n[END]");

    if (v_pos != std::string::npos && m_pos != std::string::npos && pt_pos != std::string::npos && f_pos != std::string::npos && to_pos != std::string::npos && s_pos != std::string::npos && b_pos != std::string::npos && end_pos != std::string::npos) {
        msg.protocol = std::stoi(raw.substr(v_pos + 2, m_pos - (v_pos + 2)));
        msg.message_id = raw.substr(m_pos + 7, pt_pos - (m_pos + 7));
        msg.payload = static_cast<PayloadType>(std::stoi(raw.substr(pt_pos + 7, f_pos - (pt_pos + 7))));
        msg.from = raw.substr(f_pos + 6, to_pos - (f_pos + 6));
        msg.to = raw.substr(to_pos + 4, s_pos - (to_pos + 4));
        msg.session_id = std::stoull(raw.substr(s_pos + 9, b_pos - (s_pos + 9)));
        msg.body = raw.substr(b_pos + 6, end_pos - (b_pos + 6));
    }
    return msg;
}

inline std::string SerializeMsgAck(const MsgAck& ack) {
    return "{\"message_id\":\"" + ack.message_id + "\"}";
}

inline std::string SerializeFileMeta(const FileMeta& meta) {
    return "{\"transfer_id\":\"" + meta.transfer_id + "\",\"filename\":\"" + meta.filename + "\",\"size_bytes\":" + std::to_string(meta.size_bytes) + ",\"sha256\":\"" + meta.sha256 + "\"}";
}

inline std::string SerializeFileChunk(const FileChunk& chunk) {
    return "{\"transfer_id\":\"" + chunk.transfer_id + "\",\"filename\":\"" + chunk.filename + "\",\"index\":" + std::to_string(chunk.index) + ",\"total_chunks\":" + std::to_string(chunk.total_chunks) + ",\"data\":\"" + chunk.data_base64 + "\"}";
}

inline std::string SerializeFileAck(const FileAck& ack) {
    return "{\"transfer_id\":\"" + ack.transfer_id + "\"}";
}

inline std::string SerializeFileError(const FileError& err) {
    return "{\"transfer_id\":\"" + err.transfer_id + "\",\"code\":" + std::to_string(static_cast<int>(err.code)) + ",\"message\":\"" + err.message + "\"}";
}