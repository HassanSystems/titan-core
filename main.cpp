#include <iostream>
#include <winsock2.h>
#include <thread>
#include <vector>
#include <map>
#include <mutex>
#include <sstream> 
#include <fstream> 

#pragma comment(lib, "ws2_32.lib")

using namespace std;

// =============================================================
// LOCKING STRATEGY
// =============================================================
map<SOCKET, string> client_map;
mutex map_lock;

// === LOGGER MODULE (Day 15) ===
// Writes every message to a text file
void LogMessage(string message) {
    ofstream logFile;
    logFile.open("titan_logs.txt", ios::app); 
    if (logFile.is_open()) {
        logFile << message << endl;
        logFile.close();
    }
}

// === HISTORY MODULE (Added Day 16) ===
// Reads the log file and sends it to the new user
void SendHistory(SOCKET clientSocket) {
    ifstream historyFile("titan_logs.txt");
    string line;
    
    if (historyFile.is_open()) {
        string header = "\n=== PREVIOUS CHAT HISTORY ===\n";
        send(clientSocket, header.c_str(), header.length(), 0);

        while (getline(historyFile, line)) {
            line += "\n"; // Add newline back (getline removes it)
            send(clientSocket, line.c_str(), line.length(), 0);
            Sleep(10); // Small delay to prevent packets clumping together
        }

        string footer = "=============================\n\n";
        send(clientSocket, footer.c_str(), footer.length(), 0);
        
        historyFile.close();
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

        // NEW: Send them the past (Day 16)
        SendHistory(clientSocket);
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
            size_t spacePos = msg.find(' ');
            if (spacePos != string::npos) {
                string targetName = msg.substr(1, spacePos - 1); 
                string privateMsg = msg.substr(spacePos + 1);    

                bool found = false;
                
                map_lock.lock(); 
                for (auto const& [sock, name] : client_map) {
                    if (name == targetName) {
                        string packet = "[Private from " + username + "]: " + privateMsg;
                        send(sock, packet.c_str(), packet.length(), 0);
                        found = true;
                        break;
                    }
                }
                map_lock.unlock(); 

                if (found) {
                    string confirm = "[Sent Private]: " + privateMsg;
                    send(clientSocket, confirm.c_str(), confirm.length(), 0);
                    cout << ">> [WHISPER] " << username << " -> " << targetName << endl;
                    LogMessage("[PRIVATE] " + username + " -> " + targetName + ": " + privateMsg);
                } else {
                    string error = "[ERROR] User '" + targetName + "' not found.";
                    send(clientSocket, error.c_str(), error.length(), 0);
                }
            }
        } 
        // === PUBLIC BROADCAST LOGIC ===
        else {
            cout << ">> [" << username << "]: " << msg << endl; 
            LogMessage("[PUBLIC] " + username + ": " + msg);

            map_lock.lock();
            for (auto const& [sock, name] : client_map) {
                if (sock != clientSocket) { 
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

    cout << "=== TITAN CORE | HISTORY ENABLED ===" << endl;
    
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