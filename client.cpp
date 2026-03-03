#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <iostream>
#include <winsock2.h>
#include <string>
#include <thread>
#include <filesystem>
#include <fstream>
#include <map>
#include <vector>

#pragma comment(lib, "ws2_32.lib")

#include "protocol.h"
#include "hash.h" 

using namespace std;
namespace fs = std::filesystem;

bool isRunning = true;
string myUsername = "";

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

map<string, map<uint32_t, string>> file_buffers;
map<string, string> expected_hashes; 

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
            
            if (msg.payload == PayloadType::SYSTEM) {
                cout << "[SYSTEM]: " << msg.body << "\n> " << flush;
            } 
            else if (msg.payload == PayloadType::FILE_META) {
                try {
                    string b = msg.body;
                    size_t fn_s = b.find("\"filename\":\"") + 12;
                    size_t fn_e = b.find("\"", fn_s);
                    string filename = b.substr(fn_s, fn_e - fn_s);

                    size_t h_s = b.find("\"sha256\":\"") + 10;
                    size_t h_e = b.find("\"}", h_s);
                    expected_hashes[filename] = b.substr(h_s, h_e - h_s);
                } catch(...) {}

                cout << "[META from " << msg.from << "]: " << msg.body << "\n> " << flush;
            }
            else if (msg.payload == PayloadType::FILE_CHUNK) {
                try {
                    string b = msg.body;
                    size_t fn_s = b.find("\"filename\":\"") + 12;
                    size_t fn_e = b.find("\"", fn_s);
                    string filename = b.substr(fn_s, fn_e - fn_s);
                    
                    size_t idx_s = b.find("\"index\":") + 8;
                    size_t idx_e = b.find(",", idx_s);
                    uint32_t index = stoul(b.substr(idx_s, idx_e - idx_s));

                    size_t tc_s = b.find("\"total_chunks\":") + 15;
                    size_t tc_e = b.find(",", tc_s);
                    uint32_t total_chunks = stoul(b.substr(tc_s, tc_e - tc_s));

                    size_t d_s = b.find("\"data\":\"") + 8;
                    size_t d_e = b.find("\"}", d_s);
                    string data_b64 = b.substr(d_s, d_e - d_s);

                    file_buffers[filename][index] = base64_decode(data_b64);
                    
                    if (file_buffers[filename].size() == total_chunks) {
                        ofstream out_file(filename, ios::binary);
                        for (uint32_t i = 0; i < total_chunks; i++) {
                            out_file << file_buffers[filename][i];
                        }
                        out_file.close();
                        
                        string computed_hash = ComputeFileHash(filename);
                        if (computed_hash == expected_hashes[filename]) {
                            cout << "[FILE] Reassembled & Verified (" << computed_hash << "): " << filename << "\n> " << flush;
                        } else {
                            cout << "[ERROR] CORRUPT TRANSFER! Hash mismatch for " << filename << ". Deleting.\n> " << flush;
                            fs::remove(filename); 
                        }

                        file_buffers.erase(filename);
                        expected_hashes.erase(filename);
                    }
                } catch (...) {
                    cout << "[ERROR] Corrupt file chunk dropped.\n> " << flush;
                }
            }
            else if (msg.to == "ALL") {
                cout << "[" << msg.from << "]: " << msg.body << "\n> " << flush;
            } 
            else {
                cout << "[Private from " << msg.from << "]: " << msg.body << "\n> " << flush;
            }
        }
    }
}

int main() {
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
    
    Message join_msg;
    join_msg.protocol = PROTOCOL_VERSION;
    join_msg.payload = PayloadType::SYSTEM;
    join_msg.from = myUsername;
    join_msg.to = "server";
    join_msg.body = "JOIN";

    string join_packet = SerializeMessage(join_msg);
    send(clientSocket, join_packet.c_str(), join_packet.length(), 0);

    thread receiver(ReceiveHandler, clientSocket);
    receiver.detach(); 

    string input;
    while (isRunning) {
        cout << "> ";
        getline(cin, input);

        if (input == "exit") {
            isRunning = false;
            break;
        }
        if (input.empty()) continue;

        Message outMsg;
        outMsg.protocol = PROTOCOL_VERSION;
        outMsg.from = myUsername;

        if (input.substr(0, 6) == "/file ") {
            size_t targetStart = input.find('@');
            size_t pathStart = input.find(' ', targetStart);
            
            if (targetStart != string::npos && pathStart != string::npos) {
                string target = input.substr(targetStart + 1, pathStart - targetStart - 1);
                string path = input.substr(pathStart + 1);

                try {
                    FileMeta meta;
                    meta.filename = fs::path(path).filename().string();
                    meta.size_bytes = fs::file_size(path);
                    
                    cout << "[META] Hashing file..." << endl;
                    meta.sha256 = ComputeFileHash(path); 

                    if (meta.sha256 == "ERROR") {
                        cout << "[ERROR] Could not read file for hashing." << endl;
                        continue;
                    }

                    outMsg.payload = PayloadType::FILE_META;
                    outMsg.to = target;
                    outMsg.body = SerializeFileMeta(meta);

                    string packet = SerializeMessage(outMsg);
                    send(clientSocket, packet.c_str(), packet.length(), 0);
                    
                    cout << "[META] Announced " << meta.filename << " (Hash: " << meta.sha256 << ") to " << target << endl;

                    ifstream file(path, ios::binary);
                    if (!file) continue;

                    uint32_t total_chunks = (meta.size_bytes + CHUNK_SIZE - 1) / CHUNK_SIZE;
                    char chunk_buffer[CHUNK_SIZE];
                    uint32_t chunk_index = 0;

                    while (file.read(chunk_buffer, CHUNK_SIZE) || file.gcount() > 0) {
                        size_t bytes_read = file.gcount();
                        
                        FileChunk chunk;
                        chunk.filename = meta.filename;
                        chunk.index = chunk_index++;
                        chunk.total_chunks = total_chunks;
                        chunk.data_base64 = base64_encode(chunk_buffer, bytes_read);

                        Message chunkMsg;
                        chunkMsg.protocol = PROTOCOL_VERSION;
                        chunkMsg.payload = PayloadType::FILE_CHUNK;
                        chunkMsg.from = myUsername;
                        chunkMsg.to = target;
                        chunkMsg.body = SerializeFileChunk(chunk);

                        string chunk_packet = SerializeMessage(chunkMsg);
                        send(clientSocket, chunk_packet.c_str(), chunk_packet.length(), 0);
                        
                        this_thread::sleep_for(chrono::milliseconds(5)); 
                    }
                    cout << "[FILE] " << total_chunks << " chunks dispatched." << endl;

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
                
                cout << "[Sent Private to " << outMsg.to << "]: " << outMsg.body << endl;
            } else {
                cout << "[ERROR] Invalid format. Use: @username message" << endl;
                continue;
            }
        } else {
            outMsg.to = "ALL";
            outMsg.body = input;
            outMsg.payload = PayloadType::TEXT;
        }

        string packet = SerializeMessage(outMsg);
        send(clientSocket, packet.c_str(), packet.length(), 0);
    }

    closesocket(clientSocket);
    WSACleanup();
    return 0;
}