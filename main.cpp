#include<iostream>
#include<limits> // Required for input validation
using namespace std;

int main() {
    // Core System State
    bool systemOnline = true;
    int userChoice;

    // System Initialization Sequence
    cout << "=== HASSAN SYSTEMS | TERMINAL V1 ===" << endl;

    // Simulate boot sequence latency
    for (int i = 0; i <= 100; i = i + 10) {
        cout << "[BOOT] Loading Core Modules... " << i << "%" << endl;
    }
    cout << "[OK] System Ready.\n" << endl;

    // Main Event Loop (Server Heartbeat)
    while (systemOnline) {
        cout << "\n-----------------------------" << endl;
        cout << "1. Initialize Connections" << endl;
        cout << "2. System Status" << endl;
        cout << "3. Terminate Process" << endl;
        cout << "root@hassan:~# ";

        // Input Validation (Protects against non-integer crashes)
        if (!(cin >> userChoice)) {
            cin.clear(); // Clear error flag
            cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Discard bad input
            continue; 
        }

        // Command Dispatcher
        switch (userChoice) {
            case 1:
                // Pending: Socket implementation (Winsock)
                cout << ">> [NET] Listening on Port 8080..." << endl;
                break;

            case 2:
                cout << ">> [SYS] RAM: 14MB | CPU: OK" << endl;
                break;

            case 3:
                cout << ">> [STOP] Shutting down engine." << endl;
                systemOnline = false;
                break;

            default:
                cout << ">> [ERR] Invalid Command." << endl;
        }
    }
    return 0;
}
