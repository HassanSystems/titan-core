#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <ctime>     // For Time
#include <windows.h> // For Battery
#include "httplib.h"
#include "json.hpp"

using namespace std;
using json = nlohmann::json;

const string MEMORY_FILE = "titan_memory.txt";

// --- 1. SENSORS (New) ---
string get_time() {
    time_t now = time(0);
    tm *ltm = localtime(&now);
    char buffer[80];
    strftime(buffer, 80, "%I:%M %p", ltm); // Format: 06:45 PM
    return string(buffer);
}

string get_date() {
    time_t now = time(0);
    tm *ltm = localtime(&now);
    char buffer[80];
    strftime(buffer, 80, "%Y-%m-%d", ltm); // Format: 2026-02-04
    return string(buffer);
}

string get_battery() {
    SYSTEM_POWER_STATUS spsPwr;
    if (GetSystemPowerStatus(&spsPwr)) {
        return to_string((int)spsPwr.BatteryLifePercent) + "%";
    }
    return "Unknown";
}

// --- 2. MEMORY ---
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
// --- 3. THE VOICE ---
void speak(string text) {
    string safe_text = "";
    for (char c : text) {
        if (c == '\'') safe_text += "''"; // Escape single quotes
        else if (c == '\n' || c == '\r') safe_text += " "; // FIX: Replace Newlines with Space
        else safe_text += c;
    }
    
    string command = "powershell -Command \"Add-Type -AssemblyName System.Speech; (New-Object System.Speech.Synthesis.SpeechSynthesizer).Speak('" + safe_text + "');\"";
    system(command.c_str());
}

// --- 4. THE HAND ---
void execute_command(string cmd) {
    cout << ">> [TITAN]: EXECUTION AUTHORIZED: " << cmd << endl;
    speak("Executing command.");
    system(cmd.c_str()); 
}

// --- 5. THE BRAIN ---
int main() {
    httplib::Client cli("http://localhost:11434");

    cout << ">> [TITAN V4] Sensors Online (Time + Power)." << endl;
    speak("Titan V4 online. Sensors active.");

    while(true) {
        cout << "\n>> Commander: ";
        string user_input;
        getline(cin, user_input); 
        
        if (user_input == "exit") break;

        save_memory("USER", user_input);

        // --- DYNAMIC CONTEXT INJECTION ---
        string current_time = get_time();
        string current_date = get_date();
        string battery_level = get_battery();

        string system_instruction = "System Data: [Time: " + current_time + "] [Date: " + current_date + "] [Battery: " + battery_level + "]\n"
                                  "Context: You are Titan, an AI integrated into the user's PC.\n"
                                  "Recent Chat:\n" + load_recent_memory() + 
                                  "\nInstructions: "
                                  "1. If user asks for time/date/battery, use the System Data above.\n"
                                  "2. If user says 'open notepad/calc/paint', reply CMD:notepad (etc).\n"
                                  "3. Keep answers short.";
        
        json request_body = {
            {"model", "llama3.2"},
            {"prompt", system_instruction + "\nUser: " + user_input}, 
            {"stream", false} 
        };

        auto res = cli.Post("/api/generate", request_body.dump(), "application/json");

        if (res && res->status == 200) {
            json response = json::parse(res->body);
            string answer = response["response"];
            
            if (answer.find("CMD:") != string::npos) {
                string cmd_code = answer.substr(answer.find("CMD:") + 4);
                execute_command(cmd_code);
            } else {
                cout << ">> [TITAN]: " << answer << endl;
                speak(answer);
                save_memory("TITAN", answer); 
            }
        } 
    }
    return 0;
}