/* [Titan Core] Component: TitanServer.cpp | Role: TCP server handling incoming P2P connections and routing messages. */
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
#include <random>
#include <chrono>

#pragma comment(lib, "ws2_32.lib")

#include "protocol.h" 

using namespace std;

const size_t MAX_CONCURRENT_USERS = 50; 
const int MAX_MSGS_PER_SEC = 5;         

struct Session {
    uint64_t session_id;
    string username;
    SOCKET socket;
};

map<string, Session> active_users;
map<SOCKET, string> socket_to_user;
mutex map_lock;

deque<string> global_chat_history;
mutex history_mutex;
const int MAX_HISTORY_LINES = 15;

uint64_t GenerateSessionID() {
    static random_device rd;
    static mt19937_64 gen(rd());
    static uniform_int_distribution<uint64_t> dis;
    return dis(gen);
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
    for (auto const& [name, session] : active_users) {
        if (session.socket != senderSocket) {
            targets.push_back(session.socket);
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
        if (msg.payload != PayloadType::FILE_META && msg.payload != PayloadType::FILE_CHUNK && msg.payload != PayloadType::MESSAGE_ACK) {
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
        if (msg.payload != PayloadType::FILE_META && msg.payload != PayloadType::FILE_CHUNK && msg.payload != PayloadType::MESSAGE_ACK) {
            cout << ">> [PRIVATE] " << msg.from << " -> " << msg.to << endl;
            LogMessage("[PRIVATE] " + msg.from + " -> " + msg.to + ": " + msg.body);
        }

        SOCKET targetSocket = INVALID_SOCKET;
        
        map_lock.lock();
        if (active_users.count(msg.to)) {
            targetSocket = active_users[msg.to].socket;
        }
        map_lock.unlock();

        if (targetSocket != INVALID_SOCKET) {
            string packet = SerializeMessage(msg);
            send(targetSocket, packet.c_str(), packet.length(), 0);
        } else {
            Message err;
            err.protocol = PROTOCOL_VERSION;
            err.message_id = "SYS";
            err.payload = PayloadType::SYSTEM;
            err.from = "Server";
            err.to = msg.from;
            err.session_id = 0;
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
    uint64_t current_session_id = 0;

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
                if (active_users.size() >= MAX_CONCURRENT_USERS) {
                    map_lock.unlock();
                    cout << ">> [BACKPRESSURE] Server full. Dropped connection from: " << username << endl;
                    
                    Message rej;
                    rej.protocol = PROTOCOL_VERSION;
                    rej.message_id = "SYS";
                    rej.payload = PayloadType::SESSION_REJECT;
                    rej.from = "Server";
                    rej.to = username;
                    rej.session_id = 0;
                    rej.body = "Server at maximum capacity. Try again later.";
                    string pckt = SerializeMessage(rej);
                    send(clientSocket, pckt.c_str(), pckt.length(), 0);
                    closesocket(clientSocket);
                    return; 
                }

                if (active_users.count(username)) {
                    map_lock.unlock();
                    cout << ">> [REJECTED] Duplicate login attempt for: " << username << endl;
                    
                    Message rej;
                    rej.protocol = PROTOCOL_VERSION;
                    rej.message_id = "SYS";
                    rej.payload = PayloadType::SESSION_REJECT;
                    rej.from = "Server";
                    rej.to = username;
                    rej.session_id = 0;
                    rej.body = "Username already active on the network.";
                    string pckt = SerializeMessage(rej);
                    send(clientSocket, pckt.c_str(), pckt.length(), 0);
                    closesocket(clientSocket);
                    return; 
                }
                
                current_session_id = GenerateSessionID();
                active_users[username] = {current_session_id, username, clientSocket};
                socket_to_user[clientSocket] = username;
                map_lock.unlock();
                
                cout << ">> [CONN] " << username << " assigned Session: " << current_session_id << endl;
                
                Message acc;
                acc.protocol = PROTOCOL_VERSION;
                acc.message_id = "SYS";
                acc.payload = PayloadType::SESSION_ACCEPT;
                acc.from = "Server";
                acc.to = username;
                acc.session_id = current_session_id;
                acc.body = "Welcome to the Titan Network.";
                
                string pckt = SerializeMessage(acc);
                send(clientSocket, pckt.c_str(), pckt.length(), 0);
            }
        }
    }

    int msg_count = 0;
    auto window_start = chrono::steady_clock::now();

    while (true) {
        memset(buffer, 0, 4096);
        int bytes = recv(clientSocket, buffer, 4096, 0);

        if (bytes <= 0) {
            map_lock.lock();
            if (socket_to_user.count(clientSocket)) {
                string uname = socket_to_user[clientSocket];
                cout << ">> [DISC] " << uname << " disconnected." << endl;
                active_users.erase(uname);
                socket_to_user.erase(clientSocket);
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
                
                if (parsed_msg.payload == PayloadType::TEXT || parsed_msg.payload == PayloadType::COMMAND) {
                    auto now = chrono::steady_clock::now();
                    if (chrono::duration_cast<chrono::seconds>(now - window_start).count() >= 1) {
                        msg_count = 0;
                        window_start = now;
                    }
                    msg_count++;

                    if (msg_count > MAX_MSGS_PER_SEC) {
                        cout << ">> [PRESSURE POINT] Traffic limit exceeded by " << username << ". Packet dropped." << endl;
                        LogMessage("[PRESSURE POINT] Traffic limit exceeded by " + username);
                        
                        Message warn;
                        warn.protocol = PROTOCOL_VERSION;
                        warn.message_id = "SYS";
                        warn.payload = PayloadType::SYSTEM;
                        warn.from = "Server";
                        warn.to = username;
                        warn.session_id = current_session_id;
                        warn.body = "SYSTEM: You are sending messages too fast. Packet dropped.";
                        string warn_pckt = SerializeMessage(warn);
                        send(clientSocket, warn_pckt.c_str(), warn_pckt.length(), 0);
                        
                        continue; 
                    }
                }

                bool is_valid = false;
                map_lock.lock();
                if (active_users.count(username) && 
                    active_users[username].session_id == parsed_msg.session_id && 
                    parsed_msg.from == username) {
                    is_valid = true;
                }
                map_lock.unlock();

                if (is_valid) {
                    RouteMessage(parsed_msg, clientSocket);
                } else {
                    cout << ">> [SECURITY] Spoofed packet dropped from socket " << clientSocket << endl;
                }
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
    cout << ">> Backpressure Limits Active: Max 5 msg/sec per client." << endl;
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