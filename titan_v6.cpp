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

// --- 1. THE BRIDGE (Fixed: Removes leading spaces) ---
void run_python(string code) {
    // CLEANER: Remove leading spaces or newlines so Python doesn't crash
    while (code.length() > 0 && (code[0] == ' ' || code[0] == '\n')) {
        code = code.substr(1);
    }

    // 1. Save the code to a file
    ofstream file("titan_script.py");
    if (file.is_open()) {
        file << code;
        file.close();
        cout << ">> [TITAN]: Python script generated." << endl;
    } else {
        cout << ">> [ERROR]: Could not save script." << endl;
        return;
    }

    // 2. Execute the script
    cout << ">> [TITAN]: Executing Python..." << endl;
    string command = "python titan_script.py";
    system(command.c_str());
}

// --- 2. FILE SYSTEM ---
void write_file(string filename, string content) {
    ofstream file(filename);
    if (file.is_open()) {
        file << content;
        file.close();
        cout << ">> [TITAN]: File created: " << filename << endl;
    }
}

string read_file(string filename) {
    ifstream file(filename);
    string content, line;
    if (file.is_open()) {
        while (getline(file, line)) content += line + "\n";
        file.close();
        return content;
    }
    return "Error: File not found.";
}

// --- 3. SENSORS ---
string get_time() {
    time_t now = time(0);
    tm *ltm = localtime(&now);
    char buffer[80];
    strftime(buffer, 80, "%I:%M %p", ltm);
    return string(buffer);
}

string get_battery() {
    SYSTEM_POWER_STATUS spsPwr;
    if (GetSystemPowerStatus(&spsPwr)) return to_string((int)spsPwr.BatteryLifePercent) + "%";
    return "Unknown";
}

// --- 4. MEMORY ---
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

// --- 5. VOICE & HANDS ---
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

// --- 6. MAIN ENGINE ---
int main() {
    httplib::Client cli("http://localhost:11434");

    cout << ">> [TITAN V6] The Architect (Python Bridge) Online." << endl;
    speak("Titan V6 online. Python bridge ready.");

    while(true) {
        cout << "\n>> Commander: ";
        string user_input;
        getline(cin, user_input); 
        
        if (user_input == "exit") break;
        save_memory("USER", user_input);

        string sensor_data = "[Time: " + get_time() + "] [Battery: " + get_battery() + "]";
        
        // --- PROMPT ENGINEERING ---
        string system_instruction = "System Data: " + sensor_data + "\n"
                                  "Context: You are Titan. You can control the PC and WRITE CODE.\n"
                                  "Recent Chat:\n" + load_recent_memory() + 
                                  "\nTOOLS AVAILABLE:\n"
                                  "1. To run python code (math, graphs), reply ONLY: PYTHON: <code>\n"
                                  "2. To write a file, reply ONLY: WRITE:filename.ext|content\n"
                                  "3. To open apps, reply ONLY: CMD:command\n"
                                  "4. Otherwise, reply normally.\n"
                                  "Example: User 'Draw a graph'. Titan: PYTHON: import matplotlib.pyplot as plt...";
        
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
                execute_command(answer.substr(answer.find("CMD:") + 4));
            } 
            else if (answer.find("WRITE:") != string::npos) {
                size_t split = answer.find("|");
                if (split != string::npos) {
                    write_file(answer.substr(6, split - 6), answer.substr(split + 1));
                    speak("File written.");
                }
            }
            else if (answer.find("PYTHON:") != string::npos) {
                // Extract Code
                string code = answer.substr(answer.find("PYTHON:") + 7);
                speak("Generating Python script.");
                run_python(code);
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