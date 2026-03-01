#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <iostream>
#include <winsock2.h>
#include <thread>
#include <vector>
#include <map>
#include <mutex>
#include <string>
#include <fstream>
#include <deque>

#pragma comment(lib, "ws2_32.lib")

#include "protocol.h" 

using namespace std;

map<SOCKET, string> client_map;
mutex map_lock;

deque<string> global_chat_history;
mutex history_mutex;
const int MAX_HISTORY_LINES = 15;

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

void RouteMessage(Message msg, SOCKET senderSocket) {
    if (msg.payload == PayloadType::FILE_META) {
        cout << ">> [META TRANSFER] " << msg.from << " -> " << msg.to << " : " << msg.body << endl;
        LogMessage("[META TRANSFER] " + msg.from + " -> " + msg.to);
    }

    if (msg.to == "ALL") {
        if (msg.payload != PayloadType::FILE_META) {
            cout << ">> [PUBLIC] " << msg.from << ": " << msg.body << endl;
            LogMessage("[PUBLIC] " + msg.from + ": " + msg.body);

            {
                lock_guard<mutex> lock(history_mutex);
                global_chat_history.push_back(msg.from + " said: " + msg.body);
                if (global_chat_history.size() > MAX_HISTORY_LINES) {
                    global_chat_history.pop_front();
                }
            }
        }

        BroadcastPublic(msg, senderSocket);
    } 
    else { 
        if (msg.payload != PayloadType::FILE_META) {
            cout << ">> [PRIVATE] " << msg.from << " -> " << msg.to << endl;
            LogMessage("[PRIVATE] " + msg.from + " -> " + msg.to + ": " + msg.body);
        }

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
            Message err;
            err.protocol = PROTOCOL_VERSION;
            err.payload = PayloadType::SYSTEM;
            err.from = "Server";
            err.to = msg.from;
            err.body = "User '" + msg.to + "' not found.";
            
            string err_packet = SerializeMessage(err);
            send(senderSocket, err_packet.c_str(), err_packet.length(), 0);
        }
    }
}

void ClientHandler(SOCKET clientSocket) {
    char buffer[4096] = {0};
    string username = "Unknown";
    string tcp_buffer = ""; 

    int bytesReceived = recv(clientSocket, buffer, 4096, 0);
    if (bytesReceived > 0) {
        tcp_buffer.append(buffer, bytesReceived);
        size_t end_pos = tcp_buffer.find("\n[END]");
        
        if (end_pos != string::npos) {
            string raw_data = tcp_buffer.substr(0, end_pos + 6);
            tcp_buffer.erase(0, end_pos + 6);
            
            Message join_msg = ParseMessage(raw_data);
            
            if (join_msg.payload == PayloadType::SYSTEM && join_msg.body == "JOIN") {
                username = join_msg.from;
                
                map_lock.lock();
                client_map[clientSocket] = username;
                map_lock.unlock();
                
                cout << ">> [CONN] " << username << " has joined!" << endl;
                
                Message welcome;
                welcome.protocol = PROTOCOL_VERSION;
                welcome.payload = PayloadType::SYSTEM;
                welcome.from = "Server";
                welcome.to = username;
                welcome.body = "Welcome to the Titan Network.";
                
                string pckt = SerializeMessage(welcome);
                send(clientSocket, pckt.c_str(), pckt.length(), 0);
            }
        }
    }

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

        tcp_buffer.append(buffer, bytes);
        
        while (true) {
            size_t end_pos = tcp_buffer.find("\n[END]");
            if (end_pos == string::npos) break; 
            
            string raw_data = tcp_buffer.substr(0, end_pos + 6);
            tcp_buffer.erase(0, end_pos + 6);

            Message parsed_msg = ParseMessage(raw_data);
            
            if (parsed_msg.protocol == PROTOCOL_VERSION) {
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