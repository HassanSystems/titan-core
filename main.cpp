#include <iostream>
#include <winsock2.h>
#include <limits> 

using namespace std;

int main() {
    // 1. Initialize Winsock (The Network Engine)
    WSADATA wsaData;
    int wsaErr;
    
    // MAKE THE REQUEST: "Hey Windows, give me version 2.2 of Winsock"
    wsaErr = WSAStartup(MAKEWORD(2, 2), &wsaData);

    if (wsaErr != 0) {
        cout << ">> [FATAL] Winsock failed to start. Error Code: " << wsaErr << endl;
        return 1;
    }
    
    cout << ">> [NET] Winsock DLL Found!" << endl;
    cout << ">> [NET] Status: " << wsaData.szSystemStatus << endl;

    // ==========================================
    // DAY 5: CREATE THE SOCKET (The Phone)
    // ==========================================
    // AF_INET      = IPv4 (Internet Address)
    // SOCK_STREAM  = TCP (Reliable Connection)
    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (serverSocket == INVALID_SOCKET) {
        cout << ">> [FATAL] Error creating socket: " << WSAGetLastError() << endl;
        WSACleanup();
        return 1;
    }

    cout << ">> [NET] Server Socket Created! ID: " << serverSocket << endl;
    // ==========================================

    // Core System State
    bool systemOnline = true;
    int userChoice;

    cout << "=== HASSAN SYSTEMS | TERMINAL V1 ===" << endl;
    cout << "[OK] System Ready.\n" << endl;

    // Main Event Loop
    while (systemOnline) {
        cout << "\nroot@hassan:~# ";

        if (!(cin >> userChoice)) {
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            continue; 
        }

        switch (userChoice) {
            case 1:
                // We update this line to show the real socket ID
                cout << ">> [NET] Socket Active. ID: " << serverSocket << endl;
                break;
            case 2:
                cout << ">> [SYS] RAM: 14MB | Network: ACTIVE" << endl;
                break;
            case 3:
                cout << ">> [STOP] Shutting down engine." << endl;
                systemOnline = false;
                break;
            default:
                cout << ">> [ERR] Invalid Command." << endl;
        }
    }

    // CLEANUP
    closesocket(serverSocket); // Close the socket first
    WSACleanup();              // Then turn off the driver
    return 0;
}
