#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <ctime>
#include <winsock2.h>
#include <windows.h> 
#include "httplib.h"
#include "json.hpp"

using namespace std;
using json = nlohmann::json;

const string MEMORY_FILE = "titan_memory.txt";
const string PYTHON_OUTPUT = "titan_vision_data.txt";
const string VOICE_INPUT_FILE = "titan_voice_input.txt";

// =============================================================
//  MODULE 1: THE EARS (With BEEP Synchronization) 👂
// =============================================================
void generate_hearing_script() {
    string python_code = R"(
import sounddevice as sd
from scipy.io.wavfile import write
import whisper
import os
import warnings
import sys
import winsound # REQUIRED FOR BEEP

# Setup
sys.stdout.reconfigure(encoding='utf-8')
warnings.filterwarnings("ignore")

FS = 44100  
SECONDS = 5 
AUDIO_FILE = "titan_input.wav"
TEXT_FILE = "titan_voice_input.txt"

def listen_and_transcribe():
    print(f">> INITIALIZING EARS...")
    
    # LOAD MODEL FIRST (So we don't wait while recording)
    model = whisper.load_model("base")
    
    # READY SIGNAL
    print(f">> LISTENING NOW (Speak after beep)...")
    winsound.Beep(1000, 500) # 1000Hz for 500ms
    
    try:
        # RECORD
        recording = sd.rec(int(SECONDS * FS), samplerate=FS, channels=1)
        sd.wait()
        write(AUDIO_FILE, FS, recording)
        print(">> PROCESSING AUDIO...")

        # TRANSCRIBE
        result = model.transcribe(AUDIO_FILE)
        text = result["text"].strip()
        
        print(f">> HEARD: {text}")
        
        with open(TEXT_FILE, "w", encoding="utf-8") as f:
            f.write(text)
            
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    listen_and_transcribe()
)";

    ofstream file("titan_ears.py");
    file << python_code;
    file.close();
}

string listen_to_user() {
    cout << ">> [TITAN]: Loading Auditory Cortex..." << endl;
    generate_hearing_script();
    system("python titan_ears.py");

    ifstream file(VOICE_INPUT_FILE);
    string line, spoken_text = "";
    
    if (file.is_open()) {
        while (getline(file, line)) spoken_text += line + " ";
        file.close();
    }
    return spoken_text.empty() ? "" : spoken_text;
}

// =============================================================
//  MODULE 2: THE HANDS 🖐️
// =============================================================
void click_mouse(int x, int y) {
    SetCursorPos(x, y);
    Sleep(100); 
    INPUT inputs[2] = {};
    inputs[0].type = INPUT_MOUSE; inputs[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    inputs[1].type = INPUT_MOUSE; inputs[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
    SendInput(2, inputs, sizeof(INPUT));
    cout << ">> [HANDS]: Clicked (" << x << "," << y << ")" << endl;
}

void type_text(string text) {
    cout << ">> [HANDS]: Typing..." << endl;
    for (char c : text) {
        INPUT inputs[2] = {};
        SHORT vk = VkKeyScan(c); 
        inputs[0].type = INPUT_KEYBOARD; inputs[0].ki.wVk = LOBYTE(vk);
        inputs[1].type = INPUT_KEYBOARD; inputs[1].ki.wVk = LOBYTE(vk); inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(2, inputs, sizeof(INPUT));
        Sleep(50); 
    }
}

// =============================================================
//  MODULE 3: THE RETINA 👁️
// =============================================================
void capture_screen() {
    string python_code = R"(
import pyautogui
try:
   screenshot = pyautogui.screenshot()
   screenshot.save('screen_memory.png')
except: pass
)";
    ofstream file("titan_capture.py"); file << python_code; file.close();
    system("python titan_capture.py");
}

string run_python_vision(string image_path) {
    string clean_path = "";
    for (char c : image_path) { if (isalnum(c) || c == '.') clean_path += c; }

    string python_code = R"(
import os, subprocess
try:
    tesseract_cmd = r'C:\Program Files\Tesseract-OCR\tesseract.exe'
    img_path = ')" + clean_path + R"('
    if not os.path.exists(img_path): exit()
    result = subprocess.run([tesseract_cmd, img_path, 'stdout'], capture_output=True)
    text = result.stdout.decode('utf-8', errors='ignore').strip()
    with open('titan_vision_data.txt', 'w', encoding='utf-8') as f: f.write(text)
except: pass
)";

    ofstream file("titan_vision.py"); file << python_code; file.close();
    system("python titan_vision.py");

    ifstream result_file(PYTHON_OUTPUT);
    string result, line;
    if (result_file.is_open()) {
        while (getline(result_file, line)) result += line + " ";
        result_file.close();
        return result;
    }
    return "Error.";
}

// =============================================================
//  MODULE 4: UTILS
// =============================================================
void save_memory(string speaker, string content) {
    ofstream file(MEMORY_FILE, ios::app);
    if (file.is_open()) { file << speaker << ": " << content << endl; file.close(); }
}

string load_recent_memory() {
    ifstream file(MEMORY_FILE);
    string line, memory_context = "";
    vector<string> history;
    if (file.is_open()) { while (getline(file, line)) history.push_back(line); file.close(); }
    int start_index = (history.size() > 5) ? history.size() - 5 : 0;
    for (int i = start_index; i < history.size(); i++) memory_context += history[i] + "\n";
    return memory_context;
}

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

// =============================================================
//  MAIN ENGINE 🧠
// =============================================================
int main() {
    remove("titan_voice_input.txt");
    httplib::Client cli("http://localhost:11434");

    cout << ">> [TITAN V10.2] Vision + Hands + Ears Online." << endl;
    speak("Titan online.");

    while(true) {
        cout << "\n>> Commander (Type LISTEN): ";
        string user_input;
        getline(cin, user_input); 
        
        if (user_input == "LISTEN" || user_input == "listen") {
            string spoken = listen_to_user();
            if (spoken != "") {
                user_input = spoken;
                cout << ">> [VOICE RECOGNIZED]: " << user_input << endl;
            } else {
                cout << ">> [TITAN]: I heard silence. Check your mic." << endl;
                continue;
            }
        }

        if (user_input == "exit") break;
        save_memory("USER", user_input);

        string system_instruction = 
            "Context: You are Titan V10. You can HEAR, SEE, and TOUCH.\n"
            "Recent Chat:\n" + load_recent_memory() + 
            "\nTOOLS:\n"
            "1. TYPE:text -> Types text physically.\n"
            "2. CLICK:x,y -> Clicks mouse.\n"
            "3. WATCH -> Takes screenshot & reads it.\n"
            "4. CMD:command -> Runs system command.\n";
        
        json request_body = {
            {"model", "llama3.2"},
            {"prompt", system_instruction + "\nUser: " + user_input}, 
            {"stream", false} 
        };

        auto res = cli.Post("/api/generate", request_body.dump(), "application/json");

        if (res && res->status == 200) {
            json response = json::parse(res->body);
            string answer = response["response"];
            
            if (answer.find("TYPE:") != string::npos) {
                speak("Typing.");
                type_text(answer.substr(answer.find("TYPE:") + 5));
            }
            else if (answer.find("CLICK:") != string::npos) {
                string coords = answer.substr(answer.find("CLICK:") + 6);
                int comma = coords.find(",");
                if (comma != string::npos) click_mouse(stoi(coords.substr(0, comma)), stoi(coords.substr(comma + 1)));
            }
            else if (answer.find("WATCH") != string::npos) {
                capture_screen(); 
                string seen_text = run_python_vision("screen_memory.png");
                cout << ">> [TITAN SEES]: " << seen_text.substr(0, 100) << "..." << endl; 
                speak("I see it.");
                save_memory("TITAN (SEES)", seen_text);
            }
            else if (answer.find("CMD:") != string::npos) {
                system(answer.substr(answer.find("CMD:") + 4).c_str());
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