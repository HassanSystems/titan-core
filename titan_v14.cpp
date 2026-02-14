#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <ctime>
#include <sstream>
#include <algorithm>
#include <winsock2.h>
#include <windows.h> 
#include <filesystem> // NEW: For File Navigation
#include "httplib.h"
#include "json.hpp"

using namespace std;
using json = nlohmann::json;
namespace fs = std::filesystem; // NEW: Shorten filesystem namespace

// --- CONFIGURATION ---
const string MEMORY_FILE = "titan_memory.txt";
const string ACTION_LOG_FILE = "titan_actions.log"; 
const string VOICE_INPUT_FILE = "titan_voice_input.txt";
const string TESSERACT_PATH = R"(C:\Program Files\Tesseract-OCR\tesseract.exe)";

// =============================================================
//  MODULE 0: SAFETY & LOGGING 🛡️
// =============================================================
void log_action(string action_type, string details) {
    ofstream f(ACTION_LOG_FILE, ios::app);
    time_t now = time(0);
    string time_str = ctime(&now);
    if (!time_str.empty()) time_str.pop_back();
    if (f.is_open()) { f << "[" << time_str << "] [" << action_type << "] " << details << endl; f.close(); }
}

// =============================================================
//  MODULE 1: THE EARS 👂
// =============================================================
void generate_hearing_script() {
    string python_code = R"(
import sounddevice as sd
from scipy.io.wavfile import write
import whisper
import os, sys, warnings, winsound

sys.stdout.reconfigure(encoding='utf-8')
warnings.filterwarnings("ignore")
FS = 44100; SECONDS = 5; AUDIO_FILE = "titan_input.wav"; TEXT_FILE = "titan_voice_input.txt"

def listen_and_transcribe():
    try:
        print(f">> INITIALIZING EARS...")
        model = whisper.load_model("base")
        print(f">> LISTENING NOW (Speak after beep)...")
        winsound.Beep(1000, 500)
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
    generate_hearing_script();
    system("python titan_ears.py");
    ifstream file(VOICE_INPUT_FILE);
    string line, t = "";
    if (file.is_open()) { while (getline(file, line)) t += line + " "; file.close(); }
    return t.empty() ? "" : t;
}

// =============================================================
//  MODULE 2: MOTOR CONTROL (Safe Hands) 🖐️
// =============================================================
void click_mouse_safe(int x, int y) {
    int screen_w = GetSystemMetrics(SM_CXSCREEN);
    int screen_h = GetSystemMetrics(SM_CYSCREEN);
    if (x < 0 || x > screen_w || y < 0 || y > screen_h) { cout << ">> [SAFETY]: Click blocked." << endl; return; }
    SetCursorPos(x, y); Sleep(50);
    INPUT i[2] = {};
    i[0].type = INPUT_MOUSE; i[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    i[1].type = INPUT_MOUSE; i[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
    SendInput(2, i, sizeof(INPUT));
    cout << ">> [HANDS]: Clicked " << x << "," << y << endl;
    log_action("CLICK", to_string(x) + "," + to_string(y));
}

void type_text_safe(string text) {
    for (unsigned char c : text) {
        INPUT inputs[2] = {};
        inputs[0].type = INPUT_KEYBOARD; inputs[0].ki.wScan = c; inputs[0].ki.dwFlags = KEYEVENTF_UNICODE;
        inputs[1].type = INPUT_KEYBOARD; inputs[1].ki.wScan = c; inputs[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
        SendInput(2, inputs, sizeof(INPUT)); Sleep(10); 
    }
    log_action("TYPE", text);
}

void exec_cmd_safe(string command) {
    string bad_cmds[] = {"del", "rm", "format", "shutdown", "erase"};
    string lower_cmd = command;
    transform(lower_cmd.begin(), lower_cmd.end(), lower_cmd.begin(), ::tolower);
    for (const string& bad : bad_cmds) {
        if (lower_cmd.find(bad) != string::npos) {
            cout << ">> [SAFETY]: Blocked dangerous command: " << command << endl;
            log_action("BLOCKED", command);
            return;
        }
    }
    cout << ">> [CMD]: Executing: " << command << endl;
    system(command.c_str());
    log_action("CMD", command);
}

// =============================================================
//  MODULE 3: VISION 👁️
// =============================================================
void capture_screen() {
    string p = "import pyautogui; pyautogui.screenshot().save('screen_memory.png')";
    ofstream f("titan_capture.py"); f << p; f.close();
    system("python titan_capture.py");
}

string run_python_vision() {
    if (GetFileAttributesA(TESSERACT_PATH.c_str()) == INVALID_FILE_ATTRIBUTES) return "Error: Tesseract not found.";
    string p = R"(
import os, subprocess, sys
sys.stdout.reconfigure(encoding='utf-8')
t = r')" + TESSERACT_PATH + R"('
img = 'screen_memory.png'
if os.path.exists(img):
    r = subprocess.run([t, img, 'stdout'], capture_output=True)
    print(r.stdout.decode('utf-8', errors='ignore').strip())
)";
    ofstream f("titan_vision.py"); f << p; f.close();
    system("python titan_vision.py > titan_vision_data.txt");
    ifstream r("titan_vision_data.txt"); string l, res = "";
    if (r.is_open()) { while (getline(r, l)) res += l + " "; r.close(); }
    log_action("VISION", "Analyzed screen");
    return res.empty() ? "I see nothing." : res;
}

// =============================================================
//  MODULE 6: FILE SYSTEM (New V14) 📂
// =============================================================
void list_directory(string path) {
    string contents = "FILES IN " + path + ":\n";
    try {
        // If path is just ".", use current directory
        if (path == "." || path.empty()) path = fs::current_path().string();
        
        for (const auto& entry : fs::directory_iterator(path)) {
            contents += entry.path().filename().string() + " | ";
        }
        cout << ">> [EXPLORER]: Found files." << endl;
        // Save to memory so AI can read it
        ofstream f(MEMORY_FILE, ios::app); 
        f << "TITAN_OBSERVATION: " << contents << endl;
    } catch (const exception& e) {
        cout << ">> [ERROR]: Cannot open directory " << path << endl;
    }
}

void read_file_content(string path) {
    ifstream f(path);
    if (f.is_open()) {
        stringstream buffer;
        buffer << f.rdbuf();
        string content = buffer.str().substr(0, 2000); // Limit size
        cout << ">> [READER]: Read " << path << endl;
        ofstream mem(MEMORY_FILE, ios::app); 
        mem << "TITAN_READ_FILE (" << path << "): " << content << endl;
    } else {
        cout << ">> [ERROR]: Cannot read file " << path << endl;
    }
}

// =============================================================
//  MODULE 5: HARDWARE CONTROL 🔋
// =============================================================
string get_battery_status() {
    SYSTEM_POWER_STATUS sps;
    if (GetSystemPowerStatus(&sps)) return to_string((int)sps.BatteryLifePercent) + "%";
    return "Unknown";
}

void set_volume(string action) {
    DWORD key = 0;
    if (action == "UP") key = VK_VOLUME_UP;
    else if (action == "DOWN") key = VK_VOLUME_DOWN;
    else if (action == "MUTE") key = VK_VOLUME_MUTE;
    if (key != 0) { keybd_event(key, 0, 0, 0); keybd_event(key, 0, KEYEVENTF_KEYUP, 0); log_action("HARDWARE", "Volume " + action); }
}

void system_power(string action) {
    if (action == "LOCK") { LockWorkStation(); log_action("HARDWARE", "System Locked"); }
}

// =============================================================
//  MAIN ENGINE 🧠
// =============================================================
void save_mem(string s, string c) { ofstream f(MEMORY_FILE, ios::app); f << s << ": " << c << endl; }

string load_recent_memory() {
    ifstream f(MEMORY_FILE); string l, m=""; vector<string> h; 
    while(getline(f,l)) h.push_back(l);
    int s = (h.size()>6)?h.size()-6:0; 
    for(int i=s; i<h.size(); i++) m+=h[i]+"\n"; 
    return m;
}

void speak(string t) { 
    string safe_t = "";
    for(char c : t) { if(c=='\'') safe_t+="''"; else if(c!='\n') safe_t+=c; }
    string c="powershell -Command \"Add-Type -AssemblyName System.Speech; (New-Object System.Speech.Synthesis.SpeechSynthesizer).Speak('"+safe_t+"');\""; 
    system(c.c_str()); 
}

bool get_human_confirmation(string action) {
    cout << "\n>> [SECURITY]: Titan requests permission to " << action << endl;
    speak("Commander, confirmation required.");
    cout << ">> [CONFIRM] (y/n): ";
    string choice;
    getline(cin, choice);
    return (choice == "y" || choice == "Y");
}

int main() {
    // Cleanup old scripts
    remove("titan_ears.py"); remove("titan_capture.py"); remove("titan_vision.py");
    
    httplib::Client cli("http://localhost:11434");
    cli.set_read_timeout(120); 

    cout << ">> [TITAN V14] Explorer Module Online." << endl;
    speak("Titan V14 online.");

    while(true) {
        cout << "\n>> Commander: ";
        string user_input;
        getline(cin, user_input); 
        
        if (user_input == "LISTEN" || user_input == "listen") {
            string spoken = listen_to_user();
            if (spoken.length() > 1) { user_input = spoken; cout << ">> [VOICE]: " << user_input << endl; } 
            else continue;
        }
        if (user_input == "exit") break;
        save_mem("USER", user_input);

      string system_instruction = 
    "Context: You are Titan V14. You control PC Hardware & Files.\n"
    "Battery: " + get_battery_status() + "\n"
    "Memory:\n" + load_recent_memory() + 
    "\nINSTRUCTIONS:\n"
    "Respond with ONE command on the first line.\n"
    "Valid Commands:\n"
    "TYPE:text | CLICK:x,y | WATCH | SYS:LOCK\n"
    "LIST:path (Internal Tool) | READ:path (Internal Tool)\n"
    "CMD:command (Windows Shell Only)\n"; 
        
        json req = { {"model", "llama3.2"}, {"prompt", system_instruction + "\nUser: " + user_input}, {"stream", false} };
        auto res = cli.Post("/api/generate", req.dump(), "application/json");

        if (res && res->status == 200) {
            try {
                json response = json::parse(res->body);
                string answer = response["response"];
                cout << ">> [TITAN]: " << answer << endl;
                save_mem("TITAN", answer);

                // --- V14 PARSER ---
                if (answer.rfind("TYPE:", 0) == 0) type_text_safe(answer.substr(5)); 
                else if (answer.rfind("CLICK:", 0) == 0) {
                    try {
                        string c = answer.substr(6);
                        int comma = c.find(",");
                        if(comma != string::npos)
                            click_mouse_safe(stoi(c.substr(0, comma)), stoi(c.substr(comma+1)));
                    } catch (...) { cout << ">> [ERROR] Bad Click Coords" << endl; }
                }
                else if (answer.rfind("WATCH", 0) == 0) {
                    capture_screen(); 
                    string s = run_python_vision();
                    cout << ">> [SEES]: " << s.substr(0, 50) << "..." << endl;
                    save_mem("TITAN (SEES)", s);
                }
                else if (answer.rfind("CMD:", 0) == 0) {
                    string cmd = answer.substr(4);
                    if (get_human_confirmation("execute command: " + cmd)) exec_cmd_safe(cmd);
                    else cout << ">> [TITAN]: Action aborted." << endl;
                }
                else if (answer.rfind("SYS:", 0) == 0) {
                    string sys_act = answer.substr(4);
                    if (get_human_confirmation("system action: " + sys_act)) system_power(sys_act);
                    else cout << ">> [TITAN]: System action canceled." << endl;
                }
                // --- NEW V14 COMMANDS ---
                else if (answer.rfind("LIST:", 0) == 0) list_directory(answer.substr(5));
                else if (answer.rfind("READ:", 0) == 0) read_file_content(answer.substr(5));
                else if (answer.rfind("VOL:", 0) == 0) set_volume(answer.substr(4));
                else speak(answer);
                
            } catch (const exception& e) { cout << ">> [ERROR] Crash Prevented: " << e.what() << endl; }
        } else { cout << ">> [ERROR] Ollama offline." << endl; }
    }
    return 0;
}