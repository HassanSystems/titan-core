#include <iostream>
#include <string>
#include <cstdlib>
#include "httplib.h"
#include "json.hpp"

using namespace std;
using json = nlohmann::json;

// --- 1. THE VOICE (Output) ---
void speak(string text) {
    // SANITIZATION: Double the single quotes for PowerShell (e.g. "I'm" -> "I''m")
    string safe_text = "";
    for (char c : text) {
        if (c == '\'') safe_text += "''"; 
        else safe_text += c;
    }

    string command = "powershell -Command \"Add-Type -AssemblyName System.Speech; (New-Object System.Speech.Synthesis.SpeechSynthesizer).Speak('" + safe_text + "');\"";
    system(command.c_str());
}

// --- 2. THE HAND (Action) ---
void execute_command(string cmd) {
    cout << ">> [TITAN]: EXECUTION AUTHORIZED: " << cmd << endl;
    speak("Opening application.");
    system(cmd.c_str()); 
}

// --- 3. THE BRAIN ---
int main() {
    httplib::Client cli("http://localhost:11434");

    cout << ">> [TITAN V2] System Online. Control Enabled." << endl;
    speak("Titan Version 2 online.");

    while(true) {
        cout << "\n>> Commander: ";
        string user_input;
        getline(cin, user_input); 

        if (user_input == "exit") break;

        // --- AGGRESSIVE PROMPT ---
        // We put the instructions BEFORE the user input so the AI sees them first.
        string system_instruction = "You are a PC automation tool. Strict rules:\n"
                                  "1. If user says 'open notepad', reply ONLY: CMD:notepad\n"
                                  "2. If user says 'open calculator', reply ONLY: CMD:calc\n"
                                  "3. If user says 'open paint', reply ONLY: CMD:mspaint\n"
                                  "4. Otherwise, answer briefly.\n"
                                  "User: " + user_input;
        
        json request_body = {
            {"model", "llama3.2"},
            {"prompt", system_instruction}, 
            {"stream", false} 
        };

        auto res = cli.Post("/api/generate", request_body.dump(), "application/json");

        if (res && res->status == 200) {
            json response = json::parse(res->body);
            string answer = response["response"];
            
            // --- PARSING LOGIC ---
            if (answer.find("CMD:") != string::npos) {
                // Clean up the command (remove whitespace)
                string cmd_code = answer.substr(answer.find("CMD:") + 4);
                // Remove any trailing newlines or spaces
                cmd_code.erase(0, cmd_code.find_first_not_of(" \n\r\t"));
                cmd_code.erase(cmd_code.find_last_not_of(" \n\r\t") + 1);
                
                execute_command(cmd_code);
            } else {
                cout << ">> [TITAN]: " << answer << endl;
                speak(answer);
            }
        } 
    }

    return 0;
}