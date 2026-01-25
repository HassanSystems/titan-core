#include <iostream>
#include <winsock2.h>
#include <string>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

int main() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serverAddr.sin_port = htons(8080);

    cout << ">> [CLIENT] Connecting to Titan Core..." << endl;
    if (connect(clientSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        cout << ">> [FATAL] Server offline." << endl;
        return 1;
    }
    cout << ">> [SUCCESS] Connected! Type your messages below.\n" << endl;

    // INFINITE LOOP: Keep chatting until you type "exit"
    string message;
    char buffer[4096];

    while (true) {
        cout << "> ";
        getline(cin, message); // Allow spaces in text

        if (message == "exit") break;

        // Send
        send(clientSocket, message.c_str(), message.length(), 0);

        // Receive Echo
        memset(buffer, 0, 4096);
        int bytesReceived = recv(clientSocket, buffer, 4096, 0);
        if (bytesReceived > 0) {
            cout << "[SERVER]: " << buffer << endl;
        }
    }

    closesocket(clientSocket);
    WSACleanup();
    return 0;
}
