#include <iostream>
#include <string>
#include <cstdlib>
#include "httplib.h"
#include "json.hpp"

using namespace std;
using json = nlohmann::json;

// --- VOICE MODULE ---
void speak(string text) {
    // Escape single quotes to prevent command breakage
    string safe_text = "";
    for (char c : text) {
        if (c == '\'') safe_text += " "; 
        else safe_text += c;
    }
    
    // Send to PowerShell
    string command = "powershell -Command \"Add-Type -AssemblyName System.Speech; (New-Object System.Speech.Synthesis.SpeechSynthesizer).Speak('" + safe_text + "');\"";
    system(command.c_str());
}

// --- MAIN BRAIN ---
int main() {
    // 1. Setup Connection
    httplib::Client cli("http://localhost:11434");
    
    // 2. Intro
    cout << ">> [TITAN] System Online." << endl;
    speak("System Online. I am listening.");

    while(true) {
        cout << "\n>> You: ";
        string user_input;
        getline(cin, user_input); 

        if (user_input == "exit") break;

        cout << ">> [THINKING]..." << endl;

        // 3. Build Request for Llama 3.2
        json request_body = {
            {"model", "llama3.2"},
            {"prompt", user_input + " (Answer in 1 short sentence, max 15 words)"},
            {"stream", false} 
        };

        // 4. Send to AI
        auto res = cli.Post("/api/generate", request_body.dump(), "application/json");

        if (res && res->status == 200) {
            json response = json::parse(res->body);
            string answer = response["response"];
            
            // 5. Output Results
            cout << ">> [TITAN]: " << answer << endl;
            speak(answer); // <--- THE VOICE SPEAKS HERE
        } else {
            cout << ">> [ERROR] Brain not responding." << endl;
            speak("I cannot reach my neural network.");
        }
    }

    speak("Shutting down.");
    return 0;
}