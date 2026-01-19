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
        cout << ">> [FATAL] Socket failed: " << WSAGetLastError() << endl;
        WSACleanup();
        return 1;
    }
    cout << ">> [NET] Server Socket Created! ID: " << serverSocket << endl;

    // ==========================================
    // DAY 6: BIND THE PORT (The Phone Number)
    // ==========================================
    sockaddr_in service;
    service.sin_family = AF_INET;           // IPv4
    service.sin_addr.s_addr = INADDR_ANY;   // Listen on ALL network cards
    service.sin_port = htons(8080);         // Port 8080

    if (bind(serverSocket, (SOCKADDR*)&service, sizeof(service)) == SOCKET_ERROR) {
        cout << ">> [FATAL] Bind failed: " << WSAGetLastError() << endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }
    
    cout << ">> [NET] Socket Bound to Port 8080." << endl;
    // ==========================================

    // System Loop
    bool systemOnline = true;
    int userChoice;

    cout << "=== TITAN CORE | SYSTEM V1 ===" << endl;
    cout << "[OK] System Ready.\n" << endl;

    while (systemOnline) {
        // CHANGED: Professional Prompt
        cout << "\nroot@titan-core:~# ";
        
        if (!(cin >> userChoice)) {
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            continue; 
        }

        switch (userChoice) {
            case 1:
                cout << ">> [NET] Listening on Port 8080..." << endl;
                break;
            case 2:
                cout << ">> [SYS] RAM: 14MB | Network: BOUND (8080)" << endl;
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
