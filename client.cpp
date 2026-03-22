#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <iostream>
#include <winsock2.h>
#include <string>
#include <thread>
#include <filesystem>
#include <fstream>
#include <map>
#include <vector>
#include <set>
#include <cstdlib>
#include <ctime>
#include <memory>
#include <cstdint>
#include <mutex>
#include <chrono>

#pragma comment(lib, "ws2_32.lib")

#include "protocol.h"
#include "hash.h" 

using namespace std;
namespace fs = std::filesystem;

bool isRunning = true;
string myUsername = "";
uint64_t mySessionID = 0; 

const string b64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
string base64_encode(const char* buf, unsigned int bufLen) {
    string ret;
    int i = 0, j = 0;
    unsigned char c3[3], c4[4];
    while (bufLen--) {
        c3[i++] = *(buf++);
        if (i == 3) {
            c4[0] = (c3[0] & 0xfc) >> 2;
            c4[1] = ((c3[0] & 0x03) << 4) + ((c3[1] & 0xf0) >> 4);
            c4[2] = ((c3[1] & 0x0f) << 2) + ((c3[2] & 0xc0) >> 6);
            c4[3] = c3[2] & 0x3f;
            for(i = 0; i < 4; i++) ret += b64_chars[c4[i]];
            i = 0;
        }
    }
    if (i) {
        for(j = i; j < 3; j++) c3[j] = '\0';
        c4[0] = (c3[0] & 0xfc) >> 2;
        c4[1] = ((c3[0] & 0x03) << 4) + ((c3[1] & 0xf0) >> 4);
        c4[2] = ((c3[1] & 0x0f) << 2) + ((c3[2] & 0xc0) >> 6);
        c4[3] = c3[2] & 0x3f;
        for (j = 0; j < i + 1; j++) ret += b64_chars[c4[j]];
        while((i++ < 3)) ret += '=';
    }
    return ret;
}

static inline bool is_base64(unsigned char c) { return (isalnum(c) || (c == '+') || (c == '/')); }

string base64_decode(string const& encoded_string) {
    int in_len = encoded_string.size(), i = 0, j = 0, in_ = 0;
    unsigned char c4[4], c3[3];
    string ret;
    while (in_len-- && (encoded_string[in_] != '=') && is_base64(encoded_string[in_])) {
        c4[i++] = encoded_string[in_]; in_++;
        if (i == 4) {
            for (i = 0; i < 4; i++) c4[i] = b64_chars.find(c4[i]);
            c3[0] = (c4[0] << 2) + ((c4[1] & 0x30) >> 4);
            c3[1] = ((c4[1] & 0xf) << 4) + ((c4[2] & 0x3c) >> 2);
            c3[2] = ((c4[2] & 0x3) << 6) + c4[3];
            for (i = 0; i < 3; i++) ret += c3[i];
            i = 0;
        }
    }
    if (i) {
        for (j = i; j < 4; j++) c4[j] = 0;
        for (j = 0; j < 4; j++) c4[j] = b64_chars.find(c4[j]);
        c3[0] = (c4[0] << 2) + ((c4[1] & 0x30) >> 4);
        c3[1] = ((c4[1] & 0xf) << 4) + ((c4[2] & 0x3c) >> 2);
        c3[2] = ((c4[2] & 0x3) << 6) + c4[3];
        for (j = 0; j < i - 1; j++) ret += c3[j];
    }
    return ret;
}

void LogMessage(const string& text) {
    ofstream logf("titan_log_" + myUsername + ".txt", ios::app);
    if (logf) logf << text << "\n";
}

string GenerateMessageID() {
    return to_string(time(0)) + "-" + to_string(rand() % 100000);
}

map<string, shared_ptr<ofstream>> active_streams;
map<string, string> expected_hashes; 
map<string, uint64_t> expected_chunk_index; 
map<string, string> transfer_filenames;
map<string, bool> window_acks;
map<string, bool> aborted_transfers;
map<string, string> transfer_senders;
map<string, chrono::steady_clock::time_point> last_activity;
mutex transfer_mutex;

struct PendingMessage {
    Message msg;
    int retries;
    chrono::steady_clock::time_point last_sent;
};
map<string, PendingMessage> unacked_messages;
mutex arq_mutex;

void ARQWorker(SOCKET clientSocket) {
    while (isRunning) {
        this_thread::sleep_for(chrono::milliseconds(500));
        lock_guard<mutex> lock(arq_mutex);
        auto now = chrono::steady_clock::now();
        
        for (auto it = unacked_messages.begin(); it != unacked_messages.end(); ) {
            if (chrono::duration_cast<chrono::seconds>(now - it->second.last_sent).count() >= 2) {
                if (it->second.retries < 3) {
                    it->second.retries++;
                    it->second.last_sent = now;
                    string packet = SerializeMessage(it->second.msg);
                    send(clientSocket, packet.c_str(), packet.length(), 0);
                    LogMessage("[ARQ] Retrying message ID: " + it->first + " (Attempt " + to_string(it->second.retries) + ")");
                    ++it;
                } else {
                    cout << "\n>> [NETWORK ERROR] Message to " << it->second.msg.to << " failed to deliver (Timeout).\n> " << flush;
                    it = unacked_messages.erase(it);
                }
            } else {
                ++it;
            }
        }
    }
}

void TimeoutReaper() {
    while (isRunning) {
        this_thread::sleep_for(chrono::seconds(5));
        lock_guard<mutex> lock(transfer_mutex);
        auto now = chrono::steady_clock::now();
        
        for (auto it = active_streams.begin(); it != active_streams.end(); ) {
            string t_id = it->first;
            if (chrono::duration_cast<chrono::seconds>(now - last_activity[t_id]).count() > 15) {
                cout << "\n>> [REAPER] Zombie transfer killed (Timeout). ID: " << t_id << "\n> " << flush;
                
                it->second->close();
                it->second.reset(); 
                
                error_code ec; 
                if (fs::exists(transfer_filenames[t_id], ec)) {
                    fs::remove(transfer_filenames[t_id], ec);
                    if (ec) cout << ">> [SYSTEM WARNING] Could not delete corrupted file (OS Lock): " << ec.message() << endl;
                }
                
                expected_hashes.erase(t_id);
                expected_chunk_index.erase(t_id);
                transfer_filenames.erase(t_id);
                window_acks.erase(t_id);
                transfer_senders.erase(t_id);
                aborted_transfers[t_id] = true;
                last_activity.erase(t_id);
                
                it = active_streams.erase(it);
            } else {
                ++it;
            }
        }
    }
}

void ReceiveHandler(SOCKET clientSocket) {
    char buffer[4096];
    string tcp_buffer = ""; 
    
    while (isRunning) {
        memset(buffer, 0, 4096);
        int bytesReceived = recv(clientSocket, buffer, 4096, 0);
        
        if (bytesReceived <= 0) {
            cout << "\n>> [DISCONNECTED] Server is offline." << endl;
            isRunning = false;
            break;
        }

        tcp_buffer.append(buffer, bytesReceived);
        
        while (true) {
            size_t end_pos = tcp_buffer.find("\n[END]");
            if (end_pos == string::npos) break; 
            
            string raw_data = tcp_buffer.substr(0, end_pos + 6);
            tcp_buffer.erase(0, end_pos + 6);

            Message msg = ParseMessage(raw_data);
            if (msg.protocol != PROTOCOL_VERSION) continue;

            cout << "\r                                                                \r"; 
            
            if (msg.payload == PayloadType::SESSION_ACCEPT) {
                mySessionID = msg.session_id;
                cout << "[AUTH] Identity Verified. Session ID: " << mySessionID << "\n> " << flush;
            }
            else if (msg.payload == PayloadType::SESSION_REJECT) {
                cout << "\n[REJECTED] " << msg.body << endl;
                isRunning = false;
                break;
            }
            else if (msg.payload == PayloadType::SYSTEM) {
                cout << "[SYSTEM]: " << msg.body << "\n> " << flush;
            } 
            else if (msg.payload == PayloadType::MESSAGE_ACK) {
                try {
                    size_t id_s = msg.body.find("\"message_id\":\"") + 14;
                    size_t id_e = msg.body.find("\"", id_s);
                    string ack_id = msg.body.substr(id_s, id_e - id_s);
                    
                    lock_guard<mutex> lock(arq_mutex);
                    if (unacked_messages.count(ack_id)) {
                        unacked_messages.erase(ack_id);
                    }
                } catch(...) {}
            }
            else if (msg.payload == PayloadType::FILE_META) {
                try {
                    string b = msg.body;
                    size_t tid_s = b.find("\"transfer_id\":\"") + 15;
                    size_t tid_e = b.find("\"", tid_s);
                    string transfer_id = b.substr(tid_s, tid_e - tid_s);

                    size_t fn_s = b.find("\"filename\":\"") + 12;
                    size_t fn_e = b.find("\"", fn_s);
                    string filename = b.substr(fn_s, fn_e - fn_s);

                    size_t h_s = b.find("\"sha256\":\"") + 10;
                    size_t h_e = b.find("\"}", h_s);
                    expected_hashes[transfer_id] = b.substr(h_s, h_e - h_s);
                    transfer_filenames[transfer_id] = filename;
                    transfer_senders[transfer_id] = msg.from;

                    expected_chunk_index[transfer_id] = 0;
                    
                    {
                        lock_guard<mutex> lock(transfer_mutex);
                        active_streams[transfer_id] = make_shared<ofstream>(filename, ios::binary | ios::trunc);
                        last_activity[transfer_id] = chrono::steady_clock::now();
                    }

                    cout << "[META from " << msg.from << "] ID: " << transfer_id << " | File: " << filename << "\n> " << flush;
                } catch(...) {}
            }
            else if (msg.payload == PayloadType::FILE_ACK) {
                try {
                    string b = msg.body;
                    size_t tid_s = b.find("\"transfer_id\":\"") + 15;
                    size_t tid_e = b.find("\"", tid_s);
                    string transfer_id = b.substr(tid_s, tid_e - tid_s);
                    window_acks[transfer_id] = true;
                } catch (...) {}
            }
            else if (msg.payload == PayloadType::FILE_ERROR) {
                try {
                    string b = msg.body;
                    size_t tid_s = b.find("\"transfer_id\":\"") + 15;
                    size_t tid_e = b.find("\"", tid_s);
                    string transfer_id = b.substr(tid_s, tid_e - tid_s);

                    size_t c_s = b.find("\"code\":") + 7;
                    size_t c_e = b.find(",", c_s);
                    int code = stoi(b.substr(c_s, c_e - c_s));

                    size_t m_s = b.find("\"message\":\"") + 11;
                    size_t m_e = b.find("\"}", m_s);
                    string message = b.substr(m_s, m_e - m_s);

                    cout << "\n[NETWORK ERROR] Transfer " << transfer_id << " failed! Code: " << code << " - " << message << "\n> " << flush;

                    lock_guard<mutex> lock(transfer_mutex);
                    aborted_transfers[transfer_id] = true; 

                    if (active_streams.count(transfer_id)) {
                        active_streams[transfer_id]->close();
                        active_streams[transfer_id].reset();
                        error_code ec;
                        fs::remove(transfer_filenames[transfer_id], ec);
                        active_streams.erase(transfer_id);
                    }

                    expected_hashes.erase(transfer_id);
                    expected_chunk_index.erase(transfer_id);
                    transfer_filenames.erase(transfer_id);
                    window_acks.erase(transfer_id);
                    transfer_senders.erase(transfer_id);
                    last_activity.erase(transfer_id);
                } catch(...) {}
            }
            else if (msg.payload == PayloadType::FILE_CHUNK) {
                try {
                    string b = msg.body;
                    size_t tid_s = b.find("\"transfer_id\":\"") + 15;
                    size_t tid_e = b.find("\"", tid_s);
                    string transfer_id = b.substr(tid_s, tid_e - tid_s);
                    
                    {
                        lock_guard<mutex> lock(transfer_mutex);
                        last_activity[transfer_id] = chrono::steady_clock::now();
                    }
                    
                    size_t idx_s = b.find("\"index\":") + 8;
                    size_t idx_e = b.find(",", idx_s);
                    uint64_t index = stoull(b.substr(idx_s, idx_e - idx_s));

                    if (index != expected_chunk_index[transfer_id]) {
                        cout << "[ERROR] Chunk out of order for " << transfer_id << ". Expected: " << expected_chunk_index[transfer_id] << " Got: " << index << endl;
                        
                        FileError err;
                        err.transfer_id = transfer_id;
                        err.code = ErrorCode::CHUNK_OUT_OF_ORDER;
                        err.message = "Chunk sequence mismatch. Expected " + to_string(expected_chunk_index[transfer_id]);
                        Message errMsg;
                        errMsg.protocol = PROTOCOL_VERSION;
                        errMsg.message_id = GenerateMessageID();
                        errMsg.payload = PayloadType::FILE_ERROR;
                        errMsg.from = myUsername;
                        errMsg.to = transfer_senders[transfer_id];
                        errMsg.session_id = mySessionID;
                        errMsg.body = SerializeFileError(err);
                        string err_packet = SerializeMessage(errMsg);
                        send(clientSocket, err_packet.c_str(), err_packet.length(), 0);

                        lock_guard<mutex> lock(transfer_mutex);
                        if (active_streams.count(transfer_id)) {
                            active_streams[transfer_id]->close();
                            active_streams[transfer_id].reset();
                            error_code ec;
                            fs::remove(transfer_filenames[transfer_id], ec);
                            active_streams.erase(transfer_id);
                        }
                        expected_hashes.erase(transfer_id);
                        expected_chunk_index.erase(transfer_id);
                        transfer_filenames.erase(transfer_id);
                        window_acks.erase(transfer_id);
                        transfer_senders.erase(transfer_id);
                        last_activity.erase(transfer_id);
                        continue;
                    }

                    size_t tc_s = b.find("\"total_chunks\":") + 15;
                    size_t tc_e = b.find(",", tc_s);
                    uint64_t total_chunks = stoull(b.substr(tc_s, tc_e - tc_s));

                    size_t d_s = b.find("\"data\":\"") + 8;
                    size_t d_e = b.find("\"}", d_s);
                    string data_b64 = b.substr(d_s, d_e - d_s);

                    {
                        lock_guard<mutex> lock(transfer_mutex);
                        if (active_streams.count(transfer_id)) {
                            *active_streams[transfer_id] << base64_decode(data_b64);
                        }
                    }

                    expected_chunk_index[transfer_id]++;

                    if ((index + 1) % CHUNK_WINDOW == 0 || (index + 1) == total_chunks) {
                        FileAck ack;
                        ack.transfer_id = transfer_id;
                        Message ackMsg;
                        ackMsg.protocol = PROTOCOL_VERSION;
                        ackMsg.message_id = GenerateMessageID();
                        ackMsg.payload = PayloadType::FILE_ACK;
                        ackMsg.from = myUsername;
                        ackMsg.to = msg.from;
                        ackMsg.session_id = mySessionID;
                        ackMsg.body = SerializeFileAck(ack);
                        
                        string ack_packet = SerializeMessage(ackMsg);
                        send(clientSocket, ack_packet.c_str(), ack_packet.length(), 0);
                    }
                    
                    if (expected_chunk_index[transfer_id] == total_chunks) {
                        string filename = transfer_filenames[transfer_id];

                        {
                            lock_guard<mutex> lock(transfer_mutex);
                            if (active_streams.count(transfer_id)) {
                                active_streams[transfer_id]->close();
                                active_streams.erase(transfer_id);
                            }
                        }
                        
                        string computed_hash = ComputeFileHash(filename);
                        if (computed_hash == expected_hashes[transfer_id]) {
                            cout << "[FILE] Streamed & Verified (" << transfer_id << "): " << filename << "\n> " << flush;
                        } else {
                            cout << "[ERROR] CORRUPT TRANSFER! Hash mismatch for " << filename << ". Deleting.\n> " << flush;
                            error_code ec;
                            fs::remove(filename, ec); 

                            FileError err;
                            err.transfer_id = transfer_id;
                            err.code = ErrorCode::HASH_MISMATCH;
                            err.message = "Hash mismatch during final verification.";
                            Message errMsg;
                            errMsg.protocol = PROTOCOL_VERSION;
                            errMsg.message_id = GenerateMessageID();
                            errMsg.payload = PayloadType::FILE_ERROR;
                            errMsg.from = myUsername;
                            errMsg.to = msg.from;
                            errMsg.session_id = mySessionID;
                            errMsg.body = SerializeFileError(err);
                            string err_packet = SerializeMessage(errMsg);
                            send(clientSocket, err_packet.c_str(), err_packet.length(), 0);
                        }

                        lock_guard<mutex> lock(transfer_mutex);
                        expected_hashes.erase(transfer_id);
                        expected_chunk_index.erase(transfer_id);
                        transfer_filenames.erase(transfer_id);
                        window_acks.erase(transfer_id);
                        transfer_senders.erase(transfer_id);
                        aborted_transfers.erase(transfer_id);
                        last_activity.erase(transfer_id);
                    }
                } catch (...) {
                    cout << "[ERROR] Corrupt file chunk dropped.\n> " << flush;
                }
            }
            else if (msg.to == "ALL") {
                string output = "[" + msg.from + "]: " + msg.body;
                cout << output << "\n> " << flush;
                if (msg.payload == PayloadType::TEXT) LogMessage(output);
            } 
            else {
                string output = "[Private from " + msg.from + "]: " + msg.body;
                cout << output << "\n> " << flush;
                if (msg.payload == PayloadType::TEXT) LogMessage(output);
            }
        }
    }
}

int main() {
    srand(static_cast<unsigned int>(time(0))); 
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serverAddr.sin_port = htons(8080);

    cout << ">> [CLIENT] Connecting to Network..." << endl;
    if (connect(clientSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        cout << ">> [FATAL] Server offline." << endl;
        return 1;
    }

    cout << ">> Enter your Username: ";
    getline(cin, myUsername);

    ifstream logFile("titan_log_" + myUsername + ".txt");
    if (logFile) {
        cout << "=== Chat History ===" << endl;
        string line;
        while (getline(logFile, line)) {
            cout << line << endl;
        }
        cout << "====================" << endl;
        logFile.close();
    }
    
    Message join_msg;
    join_msg.protocol = PROTOCOL_VERSION;
    join_msg.message_id = "SYS_JOIN";
    join_msg.payload = PayloadType::SYSTEM;
    join_msg.from = myUsername;
    join_msg.to = "server";
    join_msg.session_id = 0; 
    join_msg.body = "JOIN";

    string join_packet = SerializeMessage(join_msg);
    send(clientSocket, join_packet.c_str(), join_packet.length(), 0);

    thread receiver(ReceiveHandler, clientSocket);
    receiver.detach(); 

    thread reaper(TimeoutReaper);
    reaper.detach();

    thread arq(ARQWorker, clientSocket);
    arq.detach();

    while (mySessionID == 0 && isRunning) {
        this_thread::sleep_for(chrono::milliseconds(50));
    }

    if (!isRunning) {
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    string input;
    while (isRunning) {
        cout << "> ";
        getline(cin, input);

        if (input == "exit") {
            isRunning = false;
            break;
        }
        if (input.empty()) continue;

        if (input.substr(0, 7) == "/abort ") {
            string transfer_id = input.substr(7);
            
            if (transfer_senders.count(transfer_id)) {
                FileError err;
                err.transfer_id = transfer_id;
                err.code = ErrorCode::USER_ABORT;
                err.message = "Receiver explicitly aborted the transfer.";
                Message errMsg;
                errMsg.protocol = PROTOCOL_VERSION;
                errMsg.message_id = GenerateMessageID();
                errMsg.payload = PayloadType::FILE_ERROR;
                errMsg.from = myUsername;
                errMsg.to = transfer_senders[transfer_id];
                errMsg.session_id = mySessionID;
                errMsg.body = SerializeFileError(err);
                
                string err_packet = SerializeMessage(errMsg);
                send(clientSocket, err_packet.c_str(), err_packet.length(), 0);
            }

            lock_guard<mutex> lock(transfer_mutex);
            if (active_streams.count(transfer_id)) {
                active_streams[transfer_id]->close();
                active_streams[transfer_id].reset();
                error_code ec;
                if (fs::exists(transfer_filenames[transfer_id], ec)) {
                    fs::remove(transfer_filenames[transfer_id], ec);
                }
                active_streams.erase(transfer_id);
            }

            expected_hashes.erase(transfer_id);
            expected_chunk_index.erase(transfer_id);
            transfer_filenames.erase(transfer_id);
            window_acks.erase(transfer_id);
            transfer_senders.erase(transfer_id);
            last_activity.erase(transfer_id);
            aborted_transfers[transfer_id] = true;
            
            cout << "[SYSTEM] Aborted transfer and purged disk for ID: " << transfer_id << endl;
            continue;
        }

        Message outMsg;
        outMsg.protocol = PROTOCOL_VERSION;
        outMsg.message_id = GenerateMessageID();
        outMsg.from = myUsername;
        outMsg.session_id = mySessionID; 

        if (input.substr(0, 6) == "/file ") {
            size_t targetStart = input.find('@');
            size_t pathStart = input.find(' ', targetStart);
            
            if (targetStart != string::npos && pathStart != string::npos) {
                string target = input.substr(targetStart + 1, pathStart - targetStart - 1);
                string path = input.substr(pathStart + 1);

                try {
                    FileMeta meta;
                    string t_id = to_string(time(0)) + "-" + to_string(rand() % 10000);
                    meta.transfer_id = t_id;
                    meta.filename = fs::path(path).filename().string();
                    meta.size_bytes = fs::file_size(path);
                    
                    cout << "[META] Hashing file..." << endl;
                    meta.sha256 = ComputeFileHash(path); 

                    if (meta.sha256 == "ERROR") {
                        cout << "[ERROR] Could not read file for hashing." << endl;
                        continue;
                    }

                    window_acks[t_id] = false;
                    aborted_transfers[t_id] = false;

                    outMsg.payload = PayloadType::FILE_META;
                    outMsg.to = target;
                    outMsg.body = SerializeFileMeta(meta);

                    string packet = SerializeMessage(outMsg);
                    send(clientSocket, packet.c_str(), packet.length(), 0);
                    
                    cout << "[META] Announced " << meta.filename << " (ID: " << t_id << ") to " << target << endl;

                    ifstream file(path, ios::binary);
                    if (!file) continue;

                    uint64_t total_chunks = (meta.size_bytes + CHUNK_SIZE - 1) / CHUNK_SIZE;
                    char chunk_buffer[CHUNK_SIZE];
                    uint64_t chunk_index = 0;

                    while (file.read(chunk_buffer, CHUNK_SIZE) || file.gcount() > 0) {
                        if (aborted_transfers[t_id]) {
                            cout << "[SYSTEM] Transfer halted. Chunks stopped." << endl;
                            break;
                        }

                        size_t bytes_read = file.gcount();

                        if (chunk_index > 0 && chunk_index % CHUNK_WINDOW == 0) {
                            while (!window_acks[t_id] && !aborted_transfers[t_id]) {
                                this_thread::sleep_for(chrono::milliseconds(5));
                            }
                            if (aborted_transfers[t_id]) {
                                cout << "[SYSTEM] Transfer halted. Chunks stopped." << endl;
                                break;
                            }
                            window_acks[t_id] = false; 
                        }
                        
                        FileChunk chunk;
                        chunk.transfer_id = t_id;
                        chunk.filename = meta.filename;
                        chunk.index = chunk_index++;
                        chunk.total_chunks = total_chunks;
                        chunk.data_base64 = base64_encode(chunk_buffer, bytes_read);

                        Message chunkMsg;
                        chunkMsg.protocol = PROTOCOL_VERSION;
                        chunkMsg.message_id = GenerateMessageID();
                        chunkMsg.payload = PayloadType::FILE_CHUNK;
                        chunkMsg.from = myUsername;
                        chunkMsg.to = target;
                        chunkMsg.session_id = mySessionID;
                        chunkMsg.body = SerializeFileChunk(chunk);

                        string chunk_packet = SerializeMessage(chunkMsg);
                        send(clientSocket, chunk_packet.c_str(), chunk_packet.length(), 0);
                        
                        this_thread::sleep_for(chrono::milliseconds(2)); 
                    }
                    if (!aborted_transfers[t_id]) {
                        cout << "[FILE] " << total_chunks << " chunks dispatched for ID: " << t_id << endl;
                    }

                } catch (...) {
                    cout << "[ERROR] File transmission failed." << endl;
                }
            } else {
                cout << "[ERROR] Invalid format. Use: /file @username filepath" << endl;
            }
            continue;
        }

        if (input[0] == '@') {
            size_t spacePos = input.find(' ');
            if (spacePos != string::npos) {
                outMsg.to = input.substr(1, spacePos - 1); 
                outMsg.body = input.substr(spacePos + 1);    
                outMsg.payload = (outMsg.to == "titan") ? PayloadType::COMMAND : PayloadType::TEXT;
                
                string log_str = "[Sent Private to " + outMsg.to + "]: " + outMsg.body;
                cout << log_str << endl;
                if (outMsg.payload == PayloadType::TEXT) LogMessage(log_str);

            } else {
                cout << "[ERROR] Invalid format. Use: @username message" << endl;
                continue;
            }
        } else {
            outMsg.to = "ALL";
            outMsg.body = input;
            outMsg.payload = PayloadType::TEXT;

            LogMessage("[You -> ALL]: " + input);
        }

        string packet = SerializeMessage(outMsg);
        
        // Add to ARQ buffer before sending (Private messages only to avoid ALL broadcast chaos)
        if (outMsg.to != "ALL" && (outMsg.payload == PayloadType::TEXT || outMsg.payload == PayloadType::COMMAND)) {
            lock_guard<mutex> lock(arq_mutex);
            unacked_messages[outMsg.message_id] = {outMsg, 0, chrono::steady_clock::now()};
        }

        send(clientSocket, packet.c_str(), packet.length(), 0);
    }

    closesocket(clientSocket);
    WSACleanup();
    return 0;
}