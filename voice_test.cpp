#include <iostream>
#include <cstdlib>  // Required for system()
#include <string>

using namespace std;

// This function forces Windows to speak
void speak(string text) {
    // We build a PowerShell command to use the System Speech Synthesizer
    string command = "powershell -Command \"Add-Type -AssemblyName System.Speech; (New-Object System.Speech.Synthesis.SpeechSynthesizer).Speak('" + text + "');\"";
    
    // Execute the command
    system(command.c_str());
}

int main() {
    cout << ">> [INIT] Initializing Vocal Circuits..." << endl;
    
    speak("System Online.");
    cout << ">> Voice 1: Done." << endl;

    speak("Welcome back, Commander Hassan.");
    cout << ">> Voice 2: Done." << endl;

    speak("Titan Core is ready for instructions.");
    cout << ">> Voice 3: Done." << endl;

    return 0;
}