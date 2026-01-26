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
    cout << ">> [SUCCESS] Connected!" << endl;

    // STEP 1: SEND IDENTITY
    string name;
    cout << ">> Enter your Username: ";
    getline(cin, name);
    send(clientSocket, name.c_str(), name.length(), 0); // Send Name First

    cout << ">> [INFO] Welcome, " << name << ". You can now chat.\n" << endl;

    // STEP 2: CHAT LOOP
    string message;
    char buffer[4096];

    while (true) {
        cout << "> ";
        getline(cin, message);

        if (message == "exit") break;

        send(clientSocket, message.c_str(), message.length(), 0);

        memset(buffer, 0, 4096);
        int bytesReceived = recv(clientSocket, buffer, 4096, 0);
        if (bytesReceived > 0) {
            cout << buffer << endl;
        }
    }

    closesocket(clientSocket);
    WSACleanup();
    return 0;
}
