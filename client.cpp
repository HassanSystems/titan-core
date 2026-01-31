#include <iostream>
#include <winsock2.h>
#include <string>
#include <thread> // REQUIRED for preventing the freeze

#pragma comment(lib, "ws2_32.lib")

using namespace std;

bool isRunning = true;

// THREAD FUNCTION: Listens for messages in the background
// This prevents the "Freeze" because it never stops the main typing loop
void ReceiveHandler(SOCKET clientSocket) {
    char buffer[4096];
    while (isRunning) {
        memset(buffer, 0, 4096);
        int bytesReceived = recv(clientSocket, buffer, 4096, 0);
        
        if (bytesReceived <= 0) {
            cout << "\n>> [DISCONNECTED] Server is offline." << endl;
            isRunning = false;
            break;
        }

        string msg(buffer, bytesReceived);
        
        // VISUAL FIX: Move cursor to start (\r), print message, then restore prompt
        cout << "\r" << msg << "\n> " << flush; 
    }
}

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

    // STEP 1: HANDSHAKE (Send Name)
    string name;
    cout << ">> Enter your Username: ";
    getline(cin, name);
    send(clientSocket, name.c_str(), name.length(), 0);

    cout << ">> [INFO] Welcome, " << name << ". You can now chat.\n" << endl;

    // STEP 2: START LISTENER THREAD
    thread receiver(ReceiveHandler, clientSocket);
    receiver.detach(); // Detach so it runs forever in background

    // STEP 3: MAIN LOOP (Typing Only)
    string message;
    while (isRunning) {
        cout << "> ";
        getline(cin, message);

        if (message == "exit") {
            isRunning = false;
            break;
        }

        // Send immediately. Do NOT wait for reply.
        send(clientSocket, message.c_str(), message.length(), 0);
    }

    closesocket(clientSocket);
    WSACleanup();
    return 0;
}