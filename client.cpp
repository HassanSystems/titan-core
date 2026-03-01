#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <iostream>
#include <winsock2.h>
#include <string>
#include <thread>
#include <filesystem>

#pragma comment(lib, "ws2_32.lib")

#include "protocol.h"

using namespace std;
namespace fs = std::filesystem;

bool isRunning = true;
string myUsername = "";

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
                cout << "[META from " << msg.from << "]: " << msg.body << "\n> " << flush;
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
                    meta.sha256 = "pending_hash"; 

                    outMsg.payload = PayloadType::FILE_META;
                    outMsg.to = target;
                    outMsg.body = SerializeFileMeta(meta);

                    string packet = SerializeMessage(outMsg);
                    send(clientSocket, packet.c_str(), packet.length(), 0);
                    
                    cout << "[META] Announced " << meta.filename << " (" << meta.size_bytes << " bytes) to " << target << endl;
                } catch (...) {
                    cout << "[ERROR] Cannot read file metadata. Check path." << endl;
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