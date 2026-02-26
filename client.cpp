#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <iostream>
#include <winsock2.h>
#include <string>
#include <thread>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

bool isRunning = true;
string myUsername = "";

struct Message {
    string type;   
    string from;
    string to;     
    string body;
};

string SerializeMessage(const Message& msg) {
    return "TYPE:" + msg.type + "\nFROM:" + msg.from + "\nTO:" + msg.to + "\nBODY:" + msg.body + "\n[END]";
}

Message ParseMessage(const string& raw) {
    Message msg;
    size_t t_pos = raw.find("TYPE:");
    size_t f_pos = raw.find("\nFROM:");
    size_t to_pos = raw.find("\nTO:");
    size_t b_pos = raw.find("\nBODY:");
    size_t end_pos = raw.find("\n[END]");

    if (t_pos != string::npos && f_pos != string::npos && to_pos != string::npos && b_pos != string::npos && end_pos != string::npos) {
        msg.type = raw.substr(t_pos + 5, f_pos - (t_pos + 5));
        msg.from = raw.substr(f_pos + 6, to_pos - (f_pos + 6));
        msg.to = raw.substr(to_pos + 4, b_pos - (to_pos + 4));
        msg.body = raw.substr(b_pos + 6, end_pos - (b_pos + 6));
    }
    return msg;
}

void ReceiveHandler(SOCKET clientSocket) {
    char buffer[4096];
    string tcp_buffer = ""; // Fixed: Buffer to store partial packets
    
    while (isRunning) {
        memset(buffer, 0, 4096);
        int bytesReceived = recv(clientSocket, buffer, 4096, 0);
        
        if (bytesReceived <= 0) {
            cout << "\n>> [DISCONNECTED] Server is offline." << endl;
            isRunning = false;
            break;
        }

        // Fixed: Append incoming stream bytes to our buffer
        tcp_buffer.append(buffer, bytesReceived);
        
        // Fixed: Process all complete messages containing [END]
        while (true) {
            size_t end_pos = tcp_buffer.find("\n[END]");
            if (end_pos == string::npos) break; // Incomplete message, wait for next recv
            
            string raw_data = tcp_buffer.substr(0, end_pos + 6);
            tcp_buffer.erase(0, end_pos + 6);

            Message msg = ParseMessage(raw_data);
            if (msg.type.empty()) continue;

            cout << "\r                                                                \r"; 
            
            if (msg.type == "PUBLIC") {
                cout << "[" << msg.from << "]: " << msg.body << "\n> " << flush;
            } 
            else if (msg.type == "PRIVATE") {
                cout << "[Private from " << msg.from << "]: " << msg.body << "\n> " << flush;
            } 
            else if (msg.type == "SYSTEM") {
                cout << "[SYSTEM]: " << msg.body << "\n> " << flush;
            }
        }
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

    cout << ">> [CLIENT] Connecting to Network..." << endl;
    if (connect(clientSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        cout << ">> [FATAL] Server offline." << endl;
        return 1;
    }

    cout << ">> Enter your Username: ";
    getline(cin, myUsername);
    
    Message join_msg = {"SYSTEM", myUsername, "server", "JOIN"};
    string join_packet = SerializeMessage(join_msg);
    send(clientSocket, join_packet.c_str(), join_packet.length(), 0);

    thread receiver(ReceiveHandler, clientSocket);
    receiver.detach(); 

    string input;
    while (isRunning) {
        cout << "> ";
        getline(cin, input);

        if (input == "exit") {
            isRunning = false;
            break;
        }
        if (input.empty()) continue;

        Message outMsg;
        outMsg.from = myUsername;

        if (input[0] == '@') {
            size_t spacePos = input.find(' ');
            if (spacePos != string::npos) {
                outMsg.type = "PRIVATE";
                outMsg.to = input.substr(1, spacePos - 1); 
                outMsg.body = input.substr(spacePos + 1);    
                
                cout << "[Sent Private to " << outMsg.to << "]: " << outMsg.body << endl;
            } else {
                cout << "[ERROR] Invalid format. Use: @username message" << endl;
                continue;
            }
        } else {
            outMsg.type = "PUBLIC";
            outMsg.to = "ALL";
            outMsg.body = input;
        }

        string packet = SerializeMessage(outMsg);
        send(clientSocket, packet.c_str(), packet.length(), 0);
    }

    closesocket(clientSocket);
    WSACleanup();
    return 0;
}