#include <iostream>
#include <winsock2.h> // The Windows Networking Library
#include <limits> 

using namespace std;

// We need to link the library for the compiler (Visual Studio comment)
// #pragma comment(lib, "ws2_32.lib")

int main() {
    // 1. Initialize Winsock (The Network Engine)
    WSADATA wsaData;
    int wsaErr;
    
    // MAKE THE REQUEST: "Hey Windows, give me version 2.2 of Winsock"
    wsaErr = WSAStartup(MAKEWORD(2, 2), &wsaData);

    if (wsaErr != 0) {
        cout << ">> [FATAL] Winsock failed to start. Error Code: " << wsaErr << endl;
        return 1; // Stop the program if networking fails
    }
    
    cout << ">> [NET] Winsock DLL Found!" << endl;
    cout << ">> [NET] Status: " << wsaData.szSystemStatus << endl;

    // Core System State
    bool systemOnline = true;
    int userChoice;

    cout << "=== HASSAN SYSTEMS | TERMINAL V1 ===" << endl;
    cout << "[OK] System Ready.\n" << endl;

    // Main Event Loop
    while (systemOnline) {
        cout << "\nroot@hassan:~# ";

        // Input Validation
        if (!(cin >> userChoice)) {
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            continue; 
        }

        switch (userChoice) {
            case 1:
                cout << ">> [NET] Server Socket initialization pending..." << endl;
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

    // CLEANUP: Turn off Winsock when done
    WSACleanup();
    return 0;
}
