#include <iostream>
#include <winsock2.h>
#include <thread>
#include <vector>
#include <map>
#include <mutex>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

// GLOBAL REGISTRY
// Stores: Socket ID -> "Username"
map<SOCKET, string> client_map;
mutex map_lock; // The "Lock" to protect the map

void ClientHandler(SOCKET clientSocket) {
    char buffer[4096];

    // STEP 1: HANDSHAKE (Get Name)
    memset(buffer, 0, 4096);
    int bytesReceived = recv(clientSocket, buffer, 4096, 0);
    string username = "";

    if (bytesReceived > 0) {
        username = string(buffer, bytesReceived);

        // LOCK THE MAP (Stop others from touching it)
        map_lock.lock();
        client_map[clientSocket] = username;
        map_lock.unlock(); // UNLOCK (Safe to resume)

        cout << ">> [CONN] " << username << " has joined the server!" << endl;
    }

    // STEP 2: CHAT LOOP
    while (true) {
        memset(buffer, 0, 4096);
        int bytes = recv(clientSocket, buffer, 4096, 0);

        if (bytes <= 0) {
            // LOCK & REMOVE USER
            map_lock.lock();
            if (client_map.count(clientSocket)) {
                cout << ">> [DISC] " << client_map[clientSocket] << " disconnected." << endl;
                client_map.erase(clientSocket);
            }
            map_lock.unlock();
            break;
        }

        string msg(buffer, bytes);
        
        // Server now knows WHO said it
        cout << ">> [" << username << "]: " << msg << endl;

        // Echo back (Simple confirmation)
        string response = "[SERVER]: Message received, " + username;
        send(clientSocket, response.c_str(), response.length(), 0);
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

    cout << "=== TITAN CORE | IDENTITY SYSTEM ONLINE ===" << endl;
    cout << ">> [NET] Waiting for users..." << endl;

    while (true) {
        SOCKET clientSocket = accept(serverSocket, NULL, NULL);
        if (clientSocket != INVALID_SOCKET) {
            // New connection? Spawn a thread to handle the handshake.
            thread t(ClientHandler, clientSocket);
            t.detach();
        }
    }

    closesocket(serverSocket);
    WSACleanup();
    return 0;
 }
