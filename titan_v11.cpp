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
//  MODULE 1: THE EARS (Hearing) 👂
// =============================================================
void generate_hearing_script() {
    string python_code = R"(
import sounddevice as sd
from scipy.io.wavfile import write
import whisper
import os
import warnings
import sys
import winsound

sys.stdout.reconfigure(encoding='utf-8')
warnings.filterwarnings("ignore")

FS = 44100; SECONDS = 5; AUDIO_FILE = "titan_input.wav"
TEXT_FILE = "titan_voice_input.txt"

def listen_and_transcribe():
    print(f">> INITIALIZING EARS...")
    model = whisper.load_model("base")
    print(f">> LISTENING NOW (Speak after beep)...")
    winsound.Beep(1000, 500)
    
    try:
        recording = sd.rec(int(SECONDS * FS), samplerate=FS, channels=1)
        sd.wait()
        write(AUDIO_FILE, FS, recording)
        print(">> PROCESSING...")
        result = model.transcribe(AUDIO_FILE)
        text = result["text"].strip()
        print(f">> HEARD: {text}")
        with open(TEXT_FILE, "w", encoding="utf-8") as f: f.write(text)
    except Exception as e: print(f"Error: {e}")

if __name__ == "__main__": listen_and_transcribe()
)";
    ofstream file("titan_ears.py"); file << python_code; file.close();
}

string listen_to_user() {
    cout << ">> [TITAN]: Loading Ears..." << endl;
    generate_hearing_script();
    system("python titan_ears.py");
    ifstream file(VOICE_INPUT_FILE);
    string line, t = "";
    if (file.is_open()) { while (getline(file, line)) t += line + " "; file.close(); }
    return t.empty() ? "" : t;
}

// =============================================================
//  MODULE 2: MOTOR CONTROL (Hands) 🖐️
// =============================================================
void click_mouse(int x, int y) {
    SetCursorPos(x, y); Sleep(100); 
    INPUT i[2] = {};
    i[0].type = INPUT_MOUSE; i[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    i[1].type = INPUT_MOUSE; i[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
    SendInput(2, i, sizeof(INPUT));
    cout << ">> [HANDS]: Clicked " << x << "," << y << endl;
}

void type_text(string text) {
    for (char c : text) {
        INPUT i[2] = {};
        SHORT vk = VkKeyScan(c); 
        i[0].type = INPUT_KEYBOARD; i[0].ki.wVk = LOBYTE(vk);
        i[1].type = INPUT_KEYBOARD; i[1].ki.wVk = LOBYTE(vk); i[1].ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(2, i, sizeof(INPUT)); Sleep(20); 
    }
}

// =============================================================
//  MODULE 3: VISION 👁️
// =============================================================
void capture_screen() {
    string p = "import pyautogui; pyautogui.screenshot().save('screen_memory.png')";
    ofstream f("titan_capture.py"); f << p; f.close();
    system("python titan_capture.py");
}

string run_python_vision(string path) {
    string p = R"(
import os, subprocess
try:
    t = r'C:\Program Files\Tesseract-OCR\tesseract.exe'
    if os.path.exists('screen_memory.png'):
        r = subprocess.run([t, 'screen_memory.png', 'stdout'], capture_output=True)
        print(r.stdout.decode('utf-8').strip())
except: pass
)";
    ofstream f("titan_vision.py"); f << p; f.close();
    // Quick hack to read output
    system("python titan_vision.py > titan_vision_data.txt");
    ifstream r("titan_vision_data.txt"); string l, res = "";
    if (r.is_open()) { while (getline(r, l)) res += l + " "; r.close(); }
    return res;
}

// =============================================================
//  MODULE 5: HARDWARE CONTROL (The Body) 🔋🔊 [NEW]
// =============================================================
string get_battery_status() {
    SYSTEM_POWER_STATUS sps;
    if (GetSystemPowerStatus(&sps)) {
        return to_string((int)sps.BatteryLifePercent) + "%";
    }
    return "Unknown";
}

void set_volume(string action) {
    if (action == "UP") {
        keybd_event(VK_VOLUME_UP, 0, 0, 0); keybd_event(VK_VOLUME_UP, 0, KEYEVENTF_KEYUP, 0);
    } else if (action == "DOWN") {
        keybd_event(VK_VOLUME_DOWN, 0, 0, 0); keybd_event(VK_VOLUME_DOWN, 0, KEYEVENTF_KEYUP, 0);
    } else if (action == "MUTE") {
        keybd_event(VK_VOLUME_MUTE, 0, 0, 0); keybd_event(VK_VOLUME_MUTE, 0, KEYEVENTF_KEYUP, 0);
    }
    cout << ">> [HARDWARE]: Volume " << action << endl;
}

void system_power(string action) {
    if (action == "LOCK") LockWorkStation();
    // Sleep requires extra permissions, usually Lock is enough for testing
}

// =============================================================
//  MAIN ENGINE 🧠
// =============================================================
void save_mem(string s, string c) { ofstream f(MEMORY_FILE, ios::app); f << s << ": " << c << endl; }
string load_mem() { ifstream f(MEMORY_FILE); string l, m=""; while(getline(f,l)) m+=l+"\n"; return m; }
void speak(string t) { string c="powershell -Command \"Add-Type -AssemblyName System.Speech; (New-Object System.Speech.Synthesis.SpeechSynthesizer).Speak('"+t+"');\""; system(c.c_str()); }

int main() {
    httplib::Client cli("http://localhost:11434");
    cout << ">> [TITAN V11] Hardware Systems Online." << endl;
    speak("Titan V11 ready.");

    while(true) {
        cout << "\n>> Commander (Type LISTEN): ";
        string user_input;
        getline(cin, user_input); 
        
        if (user_input == "LISTEN" || user_input == "listen") {
            string spoken = listen_to_user();
            if (spoken.length() > 1) {
                user_input = spoken;
                cout << ">> [VOICE]: " << user_input << endl;
            } else continue;
        }
        if (user_input == "exit") break;
        save_mem("USER", user_input);

        // [UPDATED PROMPT] Teach Titan about its new body
        string system_instruction = 
            "Context: You are Titan V11. You control the PC Hardware.\n"
            "Battery Level: " + get_battery_status() + "\n"
            "Tools:\n"
            "1. TYPE:text\n"
            "2. CLICK:x,y\n"
            "3. WATCH\n"
            "4. CMD:command\n"
            "5. VOL:UP / VOL:DOWN / VOL:MUTE\n"
            "6. SYS:LOCK\n";
        
        json req = { {"model", "llama3.2"}, {"prompt", system_instruction + "\nUser: " + user_input}, {"stream", false} };
        auto res = cli.Post("/api/generate", req.dump(), "application/json");

        if (res && res->status == 200) {
            json response = json::parse(res->body);
            string answer = response["response"];
            
            // --- PARSER UPDATE ---
            if (answer.find("TYPE:") != string::npos) type_text(answer.substr(answer.find("TYPE:")+5));
            else if (answer.find("CLICK:") != string::npos) {
                string c = answer.substr(answer.find("CLICK:")+6);
                click_mouse(stoi(c.substr(0, c.find(","))), stoi(c.substr(c.find(",")+1)));
            }
            else if (answer.find("WATCH") != string::npos) {
                capture_screen(); 
                string s = run_python_vision("screen_memory.png");
                cout << ">> [SEES]: " << s.substr(0, 50) << "..." << endl;
                save_mem("TITAN (SEES)", s);
            }
            else if (answer.find("CMD:") != string::npos) system(answer.substr(answer.find("CMD:")+4).c_str());
            
            // [NEW] Hardware Parsers
            else if (answer.find("VOL:") != string::npos) {
                set_volume(answer.substr(answer.find("VOL:")+4));
                speak("Volume adjusted.");
            }
            else if (answer.find("SYS:") != string::npos) {
                system_power(answer.substr(answer.find("SYS:")+4));
            }
            else {
                cout << ">> [TITAN]: " << answer << endl;
                speak(answer);
                save_mem("TITAN", answer); 
            }
        } 
    }
    return 0;
}