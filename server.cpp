#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <iostream>
#include <winsock2.h>
#include <thread>
#include <vector>
#include <map>
#include <mutex>
#include <string>
#include <fstream>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

map<SOCKET, string> client_map;
mutex map_lock;

struct Message {
    string type;   
    string from;
    string to;     
    string body;
};

string SerializeMessage(const Message& msg) {
    return "TYPE:" + msg.type + "\nFROM:" + msg.from + "\nTO:" + msg.to + "\nBODY:" + msg.body + "\n[END]";
}

Message ParseMessage(const string& raw) {
    Message msg;
    size_t t_pos = raw.find("TYPE:");
    size_t f_pos = raw.find("\nFROM:");
    size_t to_pos = raw.find("\nTO:");
    size_t b_pos = raw.find("\nBODY:");
    size_t end_pos = raw.find("\n[END]");

    if (t_pos != string::npos && f_pos != string::npos && to_pos != string::npos && b_pos != string::npos && end_pos != string::npos) {
        msg.type = raw.substr(t_pos + 5, f_pos - (t_pos + 5));
        msg.from = raw.substr(f_pos + 6, to_pos - (f_pos + 6));
        msg.to = raw.substr(to_pos + 4, b_pos - (to_pos + 4));
        msg.body = raw.substr(b_pos + 6, end_pos - (b_pos + 6));
    }
    return msg;
}

void LogMessage(const string& message) {
    ofstream logFile("titan_logs.txt", ios::app); 
    if (logFile.is_open()) {
        logFile << message << endl;
    }
}

void BroadcastPublic(const Message& msg, SOCKET senderSocket) {
    string packet = SerializeMessage(msg);
    vector<SOCKET> targets;

    map_lock.lock();
    for (auto const& [sock, name] : client_map) {
        if (sock != senderSocket) {
            targets.push_back(sock);
        }
    }
    map_lock.unlock();

    for (SOCKET sock : targets) {
        send(sock, packet.c_str(), packet.length(), 0);
    }
}

void RouteMessage(const Message& msg, SOCKET senderSocket) {
    if (msg.type == "PUBLIC") {
        cout << ">> [PUBLIC] " << msg.from << ": " << msg.body << endl;
        LogMessage("[PUBLIC] " + msg.from + ": " + msg.body);
        BroadcastPublic(msg, senderSocket);
    } 
    else if (msg.type == "PRIVATE") {
        cout << ">> [PRIVATE] " << msg.from << " -> " << msg.to << endl;
        LogMessage("[PRIVATE] " + msg.from + " -> " + msg.to + ": " + msg.body);
        
        // --- TRAINING WHEELS REMOVED: Titan placeholder block deleted here ---

        SOCKET targetSocket = INVALID_SOCKET;
        
        map_lock.lock();
        for (auto const& [sock, name] : client_map) {
            if (name == msg.to) {
                targetSocket = sock;
                break;
            }
        }
        map_lock.unlock();

        if (targetSocket != INVALID_SOCKET) {
            string packet = SerializeMessage(msg);
            send(targetSocket, packet.c_str(), packet.length(), 0);
        } else {
            Message err = {"SYSTEM", "Server", msg.from, "User '" + msg.to + "' not found."};
            string err_packet = SerializeMessage(err);
            send(senderSocket, err_packet.c_str(), err_packet.length(), 0);
        }
    }
}

void ClientHandler(SOCKET clientSocket) {
    char buffer[4096] = {0};
    string username = "Unknown";
    string tcp_buffer = ""; // Fixed: Buffer to handle chopped stream data

    // Initial Join Phase
    int bytesReceived = recv(clientSocket, buffer, 4096, 0);
    if (bytesReceived > 0) {
        tcp_buffer.append(buffer, bytesReceived);
        size_t end_pos = tcp_buffer.find("\n[END]");
        
        if (end_pos != string::npos) {
            string raw_data = tcp_buffer.substr(0, end_pos + 6);
            tcp_buffer.erase(0, end_pos + 6);
            
            Message join_msg = ParseMessage(raw_data);
            
            if (join_msg.type == "SYSTEM" && join_msg.body == "JOIN") {
                username = join_msg.from;
                
                map_lock.lock();
                client_map[clientSocket] = username;
                map_lock.unlock();
                
                cout << ">> [CONN] " << username << " has joined!" << endl;
                
                Message welcome = {"SYSTEM", "Server", username, "Welcome to the Titan Network."};
                string pckt = SerializeMessage(welcome);
                send(clientSocket, pckt.c_str(), pckt.length(), 0);
            }
        }
    }

    // Main Listen Loop
    while (true) {
        memset(buffer, 0, 4096);
        int bytes = recv(clientSocket, buffer, 4096, 0);

        if (bytes <= 0) {
            map_lock.lock();
            if (client_map.count(clientSocket)) {
                cout << ">> [DISC] " << client_map[clientSocket] << " disconnected." << endl;
                client_map.erase(clientSocket);
            }
            map_lock.unlock();
            break;
        }

        // Fixed: Append incoming stream bytes to our persistent frame buffer
        tcp_buffer.append(buffer, bytes);
        
        // Fixed: Read out ALL complete messages currently stored in the buffer
        while (true) {
            size_t end_pos = tcp_buffer.find("\n[END]");
            if (end_pos == string::npos) break; // Not enough data yet, wait for next recv()
            
            string raw_data = tcp_buffer.substr(0, end_pos + 6);
            tcp_buffer.erase(0, end_pos + 6);

            Message parsed_msg = ParseMessage(raw_data);
            
            if (!parsed_msg.type.empty()) {
                RouteMessage(parsed_msg, clientSocket);
            }
        }
    }

    closesocket(clientSocket);
}

int main() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in service;
    service.sin_family = AF_INET;
    service.sin_addr.s_addr = INADDR_ANY;
    service.sin_port = htons(8080);

    bind(serverSocket, (SOCKADDR*)&service, sizeof(service));
    listen(serverSocket, SOMAXCONN);

    cout << "=== TITAN NETWORK SERVER | PROTOCOL V1 ===" << endl;
    cout << ">> Listening on Port 8080...\n" << endl;
    
    while (true) {
        SOCKET clientSocket = accept(serverSocket, NULL, NULL);
        if (clientSocket != INVALID_SOCKET) {
            thread t(ClientHandler, clientSocket);
            t.detach();
        }
    }

    closesocket(serverSocket);
    WSACleanup();
    return 0;
}