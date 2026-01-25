#include <iostream>
#include <winsock2.h>
#include <thread> // NEW: For handling multiple clients
#include <vector>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

// This function runs on a separate thread for EACH client
void ClientHandler(SOCKET clientSocket) {
    char buffer[4096];
    
    while (true) {
        memset(buffer, 0, 4096);
        int bytesReceived = recv(clientSocket, buffer, 4096, 0);

        if (bytesReceived <= 0) {
            cout << ">> [NET] Client disconnected." << endl;
            break;
        }

        string msg(buffer, bytesReceived);
        cout << ">> [MSG] Received: " << msg << endl;

        // Echo back to client
        string response = "Titan Echo: " + msg;
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

    cout << "=== TITAN CORE | MULTI-THREADED SYSTEM ===" << endl;
    cout << ">> [NET] Listening for connections..." << endl;

    while (true) {
        // Main thread only does one thing: ACCEPT new people
        SOCKET clientSocket = accept(serverSocket, NULL, NULL);
        
        if (clientSocket != INVALID_SOCKET) {
            cout << ">> [NEW] Client Connected! Spawning thread..." << endl;
            
            // Create a new thread for this user
            thread t(ClientHandler, clientSocket);
            t.detach(); // Let the thread run independently
        }
    }

    closesocket(serverSocket);
    WSACleanup();
    return 0;
}
