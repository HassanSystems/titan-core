#include <iostream>
#include <winsock2.h>
#include <string>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

int main() {
    // 1. Initialize Winsock
    WSADATA wsaData;
    int wsaErr = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (wsaErr != 0) {
        cout << ">> [FATAL] Client Winsock Failed." << endl;
        return 1;
    }

    // 2. Create Socket
    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket == INVALID_SOCKET) {
        cout << ">> [FATAL] Client Socket Failed." << endl;
        WSACleanup();
        return 1;
    }

    // 3. Connect to Server (Localhost:8080)
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1"); // Connect to SELF
    serverAddr.sin_port = htons(8080); // Port 8080

    cout << ">> [CLIENT] Connecting to Titan Core..." << endl;

    if (connect(clientSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        cout << ">> [FATAL] Connection Failed. Is the Server running?" << endl;
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    cout << ">> [SUCCESS] Connected to Server!" << endl;

    // 4. Send Data
    string message = "Titan Client Handshake: v1.0";
    send(clientSocket, message.c_str(), message.length(), 0);
    cout << ">> [TX] Sent: " << message << endl;

    // 5. Receive Response
    char buffer[4096] = {0};
    int bytesReceived = recv(clientSocket, buffer, 4096, 0);
    
    if (bytesReceived > 0) {
        cout << ">> [RX] Server Replied: \n" << endl;
        cout << "----------------------------------" << endl;
        cout << buffer << endl; // Print what the server sent back
        cout << "----------------------------------" << endl;
    }

    // 6. Close
    closesocket(clientSocket);
    WSACleanup();
    cout << "\n>> [CLIENT] Session Ended." << endl;

    // Pause so you can see the output
    int pause;
    cin >> pause; 
    return 0;
}
