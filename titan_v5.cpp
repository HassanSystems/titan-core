#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <ctime>
#include <windows.h>
#include "httplib.h"
#include "json.hpp"

using namespace std;
using json = nlohmann::json;

const string MEMORY_FILE = "titan_memory.txt";

// --- 1. FILE SYSTEM MODULE (New) ---
void write_file(string filename, string content) {
    ofstream file(filename);
    if (file.is_open()) {
        file << content;
        file.close();
        cout << ">> [TITAN]: File created: " << filename << endl;
    } else {
        cout << ">> [TITAN]: Error creating file." << endl;
    }
}

string read_file(string filename) {
    ifstream file(filename);
    string content, line;
    if (file.is_open()) {
        while (getline(file, line)) {
            content += line + "\n";
        }
        file.close();
        return content;
    }
    return "Error: File not found.";
}

// --- 2. SENSORS ---
string get_time() {
    time_t now = time(0);
    tm *ltm = localtime(&now);
    char buffer[80];
    strftime(buffer, 80, "%I:%M %p", ltm);
    return string(buffer);
}

string get_battery() {
    SYSTEM_POWER_STATUS spsPwr;
    if (GetSystemPowerStatus(&spsPwr)) {
        return to_string((int)spsPwr.BatteryLifePercent) + "%";
    }
    return "Unknown";
}

// --- 3. MEMORY ---
void save_memory(string speaker, string content) {
    ofstream file(MEMORY_FILE, ios::app);
    if (file.is_open()) {
        file << speaker << ": " << content << endl;
        file.close();
    }
}

string load_recent_memory() {
    ifstream file(MEMORY_FILE);
    string line, memory_context = "";
    vector<string> history;
    if (file.is_open()) {
        while (getline(file, line)) history.push_back(line);
        file.close();
    }
    int start_index = (history.size() > 5) ? history.size() - 5 : 0;
    for (int i = start_index; i < history.size(); i++) {
        memory_context += history[i] + "\n";
    }
    return memory_context;
}

// --- 4. VOICE & HANDS ---
void speak(string text) {
    string safe_text = "";
    for (char c : text) {
        if (c == '\'') safe_text += "''"; 
        else if (c == '\n' || c == '\r') safe_text += " "; 
        else safe_text += c;
    }
    string command = "powershell -Command \"Add-Type -AssemblyName System.Speech; (New-Object System.Speech.Synthesis.SpeechSynthesizer).Speak('" + safe_text + "');\"";
    system(command.c_str());
}

void execute_command(string cmd) {
    cout << ">> [TITAN]: EXECUTION AUTHORIZED: " << cmd << endl;
    speak("Executing.");
    system(cmd.c_str()); 
}

// --- 5. THE BRAIN ---
int main() {
    httplib::Client cli("http://localhost:11434");

    cout << ">> [TITAN V5] File System Access (Read/Write) Online." << endl;
    speak("Titan V5 online. File systems ready.");

    while(true) {
        cout << "\n>> Commander: ";
        string user_input;
        getline(cin, user_input); 
        
        if (user_input == "exit") break;
        save_memory("USER", user_input);

        string sensor_data = "[Time: " + get_time() + "] [Battery: " + get_battery() + "]";
        
        // --- PROMPT ENGINEERING ---
        string system_instruction = "System Data: " + sensor_data + "\n"
                                  "Context: You are Titan. You can control the PC.\n"
                                  "Recent Chat:\n" + load_recent_memory() + 
                                  "\nTOOLS AVAILABLE:\n"
                                  "1. To write a file, reply ONLY: WRITE:filename.ext|content\n"
                                  "2. To read a file, reply ONLY: READ:filename.ext\n"
                                  "3. To open apps, reply ONLY: CMD:command\n"
                                  "4. Otherwise, reply normally.\n"
                                  "Example: User 'Save hello to test.txt'. Titan: WRITE:test.txt|hello";
        
        json request_body = {
            {"model", "llama3.2"},
            {"prompt", system_instruction + "\nUser: " + user_input}, 
            {"stream", false} 
        };

        auto res = cli.Post("/api/generate", request_body.dump(), "application/json");

        if (res && res->status == 200) {
            json response = json::parse(res->body);
            string answer = response["response"];
            
            // --- PARSER ---
            if (answer.find("CMD:") != string::npos) {
                string cmd = answer.substr(answer.find("CMD:") + 4);
                execute_command(cmd);
            } 
            else if (answer.find("WRITE:") != string::npos) {
                size_t split = answer.find("|");
                if (split != string::npos) {
                    string filename = answer.substr(6, split - 6);
                    string content = answer.substr(split + 1);
                    write_file(filename, content);
                    speak("File written successfully.");
                }
            }
            else if (answer.find("READ:") != string::npos) {
                string filename = answer.substr(5);
                string content = read_file(filename);
                cout << ">> [FILE CONTENT]: " << content << endl;
                speak("I have read the file.");
                // Feed content back to Titan immediately (Advanced move for later)
            }
            else {
                cout << ">> [TITAN]: " << answer << endl;
                speak(answer);
                save_memory("TITAN", answer); 
            }
        } 
    }
    return 0;
}