#include <iostream>
#include <winsock2.h>
#include <limits> 

using namespace std;

int main() {
    // 1. Initialize Winsock
    WSADATA wsaData;
    int wsaErr = WSAStartup(MAKEWORD(2, 2), &wsaData);

    if (wsaErr != 0) {
        cout << ">> [FATAL] Winsock failed: " << wsaErr << endl;
        return 1;
    }
    cout << ">> [NET] Winsock DLL Found!" << endl;

    // 2. Create Socket
    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == INVALID_SOCKET) {
        cout << ">> [FATAL] Socket failed." << endl;
        WSACleanup();
        return 1;
    }

    // 3. Bind
    sockaddr_in service;
    service.sin_family = AF_INET;
    service.sin_addr.s_addr = INADDR_ANY;
    service.sin_port = htons(8080);

    if (bind(serverSocket, (SOCKADDR*)&service, sizeof(service)) == SOCKET_ERROR) {
        cout << ">> [FATAL] Bind failed." << endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    // 4. Listen
    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        cout << ">> [FATAL] Listen failed." << endl;
        return 1;
    }
    
    // ==========================================
    // SYSTEM LOOP
    // ==========================================
    bool systemOnline = true;
    int userChoice;
    
    // DAY 9 VARIABLES
    SOCKET clientSocket;
    sockaddr_in clientAddr;
    int clientAddrSize = sizeof(clientAddr);
    char buffer[4096]; // The Bucket: Holds up to 4096 characters of text

    cout << "=== TITAN CORE | SYSTEM V1 ===" << endl;
    cout << ">> [NET] LISTENING on Port 8080..." << endl;
    cout << "[OK] System Ready.\n" << endl;

    while (systemOnline) {
        cout << "\nroot@titan-core:~# ";
        
        if (!(cin >> userChoice)) {
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            continue; 
        }

        switch (userChoice) {
            case 1:
                cout << ">> [NET] Waiting for client connection..." << endl;
                
                // 1. Accept the call
                clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientAddrSize);
                
                if (clientSocket == INVALID_SOCKET) {
                    cout << ">> [ERR] Accept failed: " << WSAGetLastError() << endl;
                } else {
                    cout << ">> [SUCCESS] CLIENT CONNECTED!" << endl;
                    
                    // 2. DAY 9: RECEIVE DATA
                    // Clear the buffer (clean the bucket)
                    memset(buffer, 0, 4096);
                    
                    // Receive data from client
                    int bytesReceived = recv(clientSocket, buffer, 4096, 0);
                    
                    if (bytesReceived > 0) {
                        cout << ">> [DATA] INCOMING PACKET DETECTED (" << bytesReceived << " bytes)" << endl;
                        cout << "-----------------------------------" << endl;
                        cout << buffer << endl; // PRINT THE BROWSER'S REQUEST
                        cout << "-----------------------------------" << endl;
                    }

                    // 3. Close connection
                    closesocket(clientSocket);
                    cout << ">> [NET] Connection closed." << endl;
                }
                break;
                
            case 2:
                cout << ">> [SYS] RAM: 14MB | Socket: " << serverSocket << endl;
                break;
                
            case 3:
                cout << ">> [STOP] System Halt." << endl;
                systemOnline = false;
                break;
                
            default:
                cout << ">> [ERR] Invalid Command." << endl;
        }
    }

    closesocket(serverSocket);
    WSACleanup();
    return 0;
}
