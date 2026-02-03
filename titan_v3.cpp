#include <iostream>
#include <string>
#include <vector>
#include <fstream>   // For file handling
#include <cstdlib>
#include "httplib.h"
#include "json.hpp"

using namespace std;
using json = nlohmann::json;

const string MEMORY_FILE = "titan_memory.txt";

// --- 1. MEMORY MODULE (New) ---
void save_memory(string speaker, string content) {
    ofstream file(MEMORY_FILE, ios::app); // Open in append mode
    if (file.is_open()) {
        file << speaker << ": " << content << endl;
        file.close();
    }
}

string load_recent_memory() {
    ifstream file(MEMORY_FILE);
    string line, memory_context = "";
    vector<string> history;

    // Read all lines
    if (file.is_open()) {
        while (getline(file, line)) {
            history.push_back(line);
        }
        file.close();
    }

    // Get last 5 lines for immediate context
    int start_index = (history.size() > 5) ? history.size() - 5 : 0;
    for (int i = start_index; i < history.size(); i++) {
        memory_context += history[i] + "\n";
    }
    return memory_context;
}

// --- 2. THE VOICE ---
void speak(string text) {
    string safe_text = "";
    for (char c : text) {
        if (c == '\'') safe_text += "''"; 
        else safe_text += c;
    }
    string command = "powershell -Command \"Add-Type -AssemblyName System.Speech; (New-Object System.Speech.Synthesis.SpeechSynthesizer).Speak('" + safe_text + "');\"";
    system(command.c_str());
}

// --- 3. THE HAND ---
void execute_command(string cmd) {
    cout << ">> [TITAN]: EXECUTION AUTHORIZED: " << cmd << endl;
    speak("Executing command.");
    save_memory("TITAN", "Executed system command: " + cmd); // Log action
    system(cmd.c_str()); 
}

// --- 4. THE BRAIN ---
int main() {
    httplib::Client cli("http://localhost:11434");

    cout << ">> [TITAN V3] Memory Systems Online." << endl;
    speak("Titan Version 3 initialized. Accessing memory banks.");

    while(true) {
        cout << "\n>> Commander: ";
        string user_input;
        getline(cin, user_input); 
        
        if (user_input == "exit") break;

        // Save User input to memory
        save_memory("USER", user_input);

        // Load recent context to remind the AI
        string recent_context = load_recent_memory();

        // --- CONTEXT INJECTION ---
        string system_instruction = "System: You are Titan. You have control over Windows. "
                                  "Here is the recent conversation history:\n" + recent_context + 
                                  "\nInstruction: If user says 'open notepad', reply CMD:notepad. "
                                  "If 'open calc', reply CMD:calc. Otherwise, answer normally based on history.";
        
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
                cmd_code.erase(0, cmd_code.find_first_not_of(" \n\r\t"));
                cmd_code.erase(cmd_code.find_last_not_of(" \n\r\t") + 1);
                execute_command(cmd_code);
            } else {
                cout << ">> [TITAN]: " << answer << endl;
                speak(answer);
                save_memory("TITAN", answer); // Save AI response to memory
            }
        } 
    }
    return 0;
}