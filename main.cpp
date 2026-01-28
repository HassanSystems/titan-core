#include <iostream>
#include <winsock2.h>
#include <thread>
#include <vector>
#include <map>
#include <mutex>
#include <sstream> // NEW: For splitting strings
#include <fstream> // NEW: For File Handling (Added Day 15)

#pragma comment(lib, "ws2_32.lib")

using namespace std;

// =============================================================
// LOCKING STRATEGY (Architecture Advice by Y. Fuchs)
// =============================================================
// This mutex protects the 'client_map' from Race Conditions.
//
// SCENARIO: 
// If Client A connects (Write) at the exact same time Client B 
// disconnects (Delete), the Server will crash without this lock.
//
// RULES:
// 1. MUST lock before writing to client_map (New Connection).
// 2. MUST lock before deleting from client_map (Disconnect).
// 3. MUST lock before iterating to find a user (Private Message).
// =============================================================
map<SOCKET, string> client_map;
mutex map_lock;

// === LOGGER MODULE (Added Day 15) ===
// Writes every message to a text file with a timestamp
void LogMessage(string message) {
    ofstream logFile;
    // app = APPEND mode (don't delete old logs, just add to the bottom)
    logFile.open("titan_logs.txt", ios::app); 
    if (logFile.is_open()) {
        logFile << message << endl;
        logFile.close();
    }
}

void ClientHandler(SOCKET clientSocket) {
    char buffer[4096];

    // STEP 1: HANDSHAKE
    memset(buffer, 0, 4096);
    int bytesReceived = recv(clientSocket, buffer, 4096, 0);
    string username = "";

    if (bytesReceived > 0) {
        username = string(buffer, bytesReceived);
        map_lock.lock();
        client_map[clientSocket] = username;
        map_lock.unlock();
        cout << ">> [CONN] " << username << " has joined!" << endl;
    }

    // STEP 2: CHAT LOOP
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

        string msg(buffer, bytes);
        
        // === PRIVATE MESSAGING LOGIC ===
        if (msg[0] == '@') {
            // Format: "@Bob Hello secret friend"
            size_t spacePos = msg.find(' ');
            if (spacePos != string::npos) {
                string targetName = msg.substr(1, spacePos - 1); // Extract "Bob"
                string privateMsg = msg.substr(spacePos + 1);    // Extract "Hello secret friend"

                bool found = false;
                
                map_lock.lock(); // LOCK while searching
                for (auto const& [sock, name] : client_map) {
                    if (name == targetName) {
                        string packet = "[Private from " + username + "]: " + privateMsg;
                        send(sock, packet.c_str(), packet.length(), 0);
                        found = true;
                        break;
                    }
                }
                map_lock.unlock(); // UNLOCK

                // Feedback to Sender
                if (found) {
                    string confirm = "[Sent Private]: " + privateMsg;
                    send(clientSocket, confirm.c_str(), confirm.length(), 0);
                    cout << ">> [WHISPER] " << username << " -> " << targetName << endl;

                    // NEW: Save Private Message to File
                    LogMessage("[PRIVATE] " + username + " -> " + targetName + ": " + privateMsg);
                } else {
                    string error = "[ERROR] User '" + targetName + "' not found.";
                    send(clientSocket, error.c_str(), error.length(), 0);
                }
            }
        } 
        // === PUBLIC BROADCAST LOGIC ===
        else {
            cout << ">> [" << username << "]: " << msg << endl; // Log to server
            
            // NEW: Save Public Message to File
            LogMessage("[PUBLIC] " + username + ": " + msg);

            map_lock.lock();
            for (auto const& [sock, name] : client_map) {
                if (sock != clientSocket) { // Don't echo to self
                    string packet = "[" + username + "]: " + msg;
                    send(sock, packet.c_str(), packet.length(), 0);
                }
            }
            map_lock.unlock();
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

    cout << "=== TITAN CORE | PRIVATE MESSAGING ONLINE ===" << endl;
    
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
