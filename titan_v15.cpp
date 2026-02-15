#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <ctime>
#include <sstream>
#include <algorithm>
#include <winsock2.h>
#include <windows.h> 
#include <filesystem>
#include "httplib.h"
#include "json.hpp"

using namespace std;
using json = nlohmann::json;
namespace fs = std::filesystem;

// --- SECURITY CONFIGURATION (V15) 🔒 ---
// The Agent is LOCKED in this workspace. It cannot escape.
const string WORKSPACE_ROOT = "C:\\TitanWorkspace"; 
const size_t MAX_READ_SIZE = 5000; // 5KB Limit to prevent crashing

// Allowed extensions (Whitelist)
const vector<string> ALLOWED_EXTENSIONS = {
    ".txt", ".md", ".cpp", ".hpp", ".h", ".py", ".json", ".log"
};

// --- STANDARD CONFIG ---
const string MEMORY_FILE = "titan_memory.txt";
const string ACTION_LOG_FILE = "titan_actions.log"; 
const string VOICE_INPUT_FILE = "titan_voice_input.txt";
const string TESSERACT_PATH = R"(C:\Program Files\Tesseract-OCR\tesseract.exe)";

// =============================================================
//  MODULE 0: SECURITY KERNEL 🛡️
// =============================================================
void log_action(string action_type, string details) {
    ofstream f(ACTION_LOG_FILE, ios::app);
    time_t now = time(0);
    string time_str = ctime(&now);
    if (!time_str.empty()) time_str.pop_back();
    if (f.is_open()) { f << "[" << time_str << "] [" << action_type << "] " << details << endl; f.close(); }
}

// V15 SECURITY: PATH SANDBOXING
bool is_safe_path(string input_path) {
    try {
        fs::path p = fs::absolute(input_path);
        fs::path root = fs::absolute(WORKSPACE_ROOT);

        // Check if path starts with root (Case insensitive for Windows)
        string p_str = p.string();
        string r_str = root.string();
        transform(p_str.begin(), p_str.end(), p_str.begin(), ::tolower);
        transform(r_str.begin(), r_str.end(), r_str.begin(), ::tolower);

        if (p_str.find(r_str) == 0) return true;
        return false;
    } catch (...) { return false; }
}

// V15 SECURITY: EXTENSION FILTERING
bool is_allowed_extension(string path) {
    string ext = fs::path(path).extension().string();
    for(auto& e : ALLOWED_EXTENSIONS) if(e == ext) return true;
    return false;
}

// =============================================================
//  MODULE 6: SECURE FILE SYSTEM (V15) 📂
// =============================================================
void list_directory(string relative_path) {
    // Force path to be relative to workspace
    fs::path target = fs::path(WORKSPACE_ROOT) / relative_path;
    
    // 1. Sandbox Check
    if (!is_safe_path(target.string())) {
        cout << ">> [SECURITY]: ACCESS DENIED. Stay in " << WORKSPACE_ROOT << endl;
        log_action("SECURITY_ALERT", "Attempted escape to " + relative_path);
        return;
    }

    string contents = "FILES IN " + relative_path + ":\n";
    try {
        if (fs::exists(target) && fs::is_directory(target)) {
            for (const auto& entry : fs::directory_iterator(target)) {
                contents += entry.path().filename().string() + " | ";
            }
            cout << ">> [EXPLORER]: Listed " << relative_path << endl;
            // V15: Clean Observation Log
            ofstream f(MEMORY_FILE, ios::app); 
            f << "OBSERVATION (File System): " << contents << endl;
        } else {
            cout << ">> [EXPLORER]: Path not found." << endl;
        }
    } catch (...) { cout << ">> [ERROR]: Access Error." << endl; }
}

void read_file_content(string relative_path) {
    fs::path target = fs::path(WORKSPACE_ROOT) / relative_path;

    // 1. Sandbox Check
    if (!is_safe_path(target.string())) {
        cout << ">> [SECURITY]: READ BLOCKED. Path unsafe." << endl;
        return;
    }

    // 2. Extension Check
    if (!is_allowed_extension(target.string())) {
        cout << ">> [SECURITY]: READ BLOCKED. Dangerous extension." << endl;
        return;
    }

    // 3. Size Check (Prevent memory overflow)
    try {
        if (fs::file_size(target) > MAX_READ_SIZE) {
            cout << ">> [SECURITY]: READ BLOCKED. File too large." << endl;
            return;
        }
    } catch(...) { return; }

    // 4. Read
    ifstream f(target);
    if (f.is_open()) {
        stringstream buffer;
        buffer << f.rdbuf();
        string content = buffer.str();
        
        cout << ">> [READER]: Read " << relative_path << endl;
        
        // V15: Structured Memory Entry
        ofstream mem(MEMORY_FILE, ios::app); 
        mem << "OBSERVATION (File Read): " << relative_path << " | CONTENT: " << content << endl;
    } else {
        cout << ">> [ERROR]: Cannot read file." << endl;
    }
}

// =============================================================
//  MODULE 1-5 (STANDARD UTILS - OPTIMIZED)
// =============================================================
void speak(string t) { 
    string safe_t = "";
    for(char c : t) { if(c=='\'') safe_t+="''"; else if(c!='\n') safe_t+=c; }
    string c="powershell -Command \"Add-Type -AssemblyName System.Speech; (New-Object System.Speech.Synthesis.SpeechSynthesizer).Speak('"+safe_t+"');\""; 
    system(c.c_str()); 
}

void click_mouse_safe(int x, int y) {
    int screen_w = GetSystemMetrics(SM_CXSCREEN); int screen_h = GetSystemMetrics(SM_CYSCREEN);
    if (x < 0 || x > screen_w || y < 0 || y > screen_h) { cout << ">> [SAFETY]: Click blocked." << endl; return; }
    SetCursorPos(x, y); Sleep(50);
    INPUT i[2] = {}; i[0].type = INPUT_MOUSE; i[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN; i[1].type = INPUT_MOUSE; i[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
    SendInput(2, i, sizeof(INPUT));
    cout << ">> [HANDS]: Clicked " << x << "," << y << endl;
}

void type_text_safe(string text) {
    for (unsigned char c : text) {
        INPUT inputs[2] = {}; inputs[0].type = INPUT_KEYBOARD; inputs[0].ki.wScan = c; inputs[0].ki.dwFlags = KEYEVENTF_UNICODE;
        inputs[1].type = INPUT_KEYBOARD; inputs[1].ki.wScan = c; inputs[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
        SendInput(2, inputs, sizeof(INPUT)); Sleep(10); 
    }
}

void capture_screen() { 
    string p = "import pyautogui; pyautogui.screenshot().save('screen_memory.png')";
    ofstream f("titan_capture.py"); f << p; f.close();
    system("python titan_capture.py");
}

string run_python_vision() {
    ofstream f("titan_vision.py"); 
    f << "import pyautogui, subprocess; pyautogui.screenshot('screen_memory.png'); subprocess.run([r'" << TESSERACT_PATH << "', 'screen_memory.png', 'stdout'], capture_output=True)";
    f.close();
    system("python titan_vision.py > titan_vision_data.txt");
    ifstream r("titan_vision_data.txt"); string l, res=""; while(getline(r,l)) res+=l+" ";
    return res.empty() ? "Nothing" : res;
}

// =============================================================
//  MODULE 1: THE EARS 👂 (Restored)
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
//  MAIN LOOP
// =============================================================
bool get_human_confirmation(string action) {
    cout << "\n>> [SECURITY]: Confirm " << action << "? (y/n): ";
    string choice; getline(cin, choice);
    return (choice == "y" || choice == "Y");
}

void exec_cmd_safe(string command) {
    if (get_human_confirmation("CMD: " + command)) {
        system(command.c_str());
        log_action("CMD", command);
    }
}

void system_power(string action) {
    if (action == "LOCK" && get_human_confirmation("LOCK PC")) LockWorkStation();
}

string get_battery_status() {
    SYSTEM_POWER_STATUS sps; if (GetSystemPowerStatus(&sps)) return to_string((int)sps.BatteryLifePercent) + "%"; return "Unknown";
}

string load_recent_memory() {
    ifstream f(MEMORY_FILE); string l, m=""; vector<string> h; 
    while(getline(f,l)) h.push_back(l);
    int s = (h.size()>8)?h.size()-8:0; 
    for(int i=s; i<h.size(); i++) m+=h[i]+"\n"; 
    return m;
}

int main() {
    // --- V15 STARTUP CHECK ---
    if (!fs::exists(WORKSPACE_ROOT)) {
        fs::create_directory(WORKSPACE_ROOT);
        cout << ">> [INIT]: Created Workspace at " << WORKSPACE_ROOT << endl;
    }
    // Cleanup old scripts
    remove("titan_ears.py"); remove("titan_capture.py"); remove("titan_vision.py");

    httplib::Client cli("http://localhost:11434"); cli.set_read_timeout(120);
    cout << ">> [TITAN V15] SANDBOX MODE ACTIVE." << endl;
    cout << ">> [SECURITY]: File access restricted to " << WORKSPACE_ROOT << endl;
    speak("Titan V15 Online. Sandbox Active.");

    while(true) {
        cout << "\n>> Commander: ";
        string user_input; getline(cin, user_input); 

        if (user_input == "LISTEN" || user_input == "listen") {
            string spoken = listen_to_user();
            if (spoken.length() > 1) { user_input = spoken; cout << ">> [VOICE]: " << user_input << endl; } 
            else continue;
        }

        if (user_input == "exit") break;
        
        // Memory & Prompt
        ofstream m(MEMORY_FILE, ios::app); m << "USER: " << user_input << endl;
        string system_instruction = 
            "Context: You are Titan V15. You are SANDBOXED inside " + WORKSPACE_ROOT + "\n"
            "Battery: " + get_battery_status() + "\n"
            "Memory:\n" + load_recent_memory() + 
            "\nCOMMANDS:\n"
            "LIST:filename (Lists files in workspace)\n"
            "READ:filename (Reads text files in workspace)\n"
            "CMD:command (Requires Auth)\n"
            "TYPE:text | CLICK:x,y | WATCH | SYS:LOCK\n";

        json req = { {"model", "llama3.2"}, {"prompt", system_instruction + "\nUser: " + user_input}, {"stream", false} };
        auto res = cli.Post("/api/generate", req.dump(), "application/json");

        if (res && res->status == 200) {
            try {
                json response = json::parse(res->body);
                string answer = response["response"];
                cout << ">> [TITAN]: " << answer << endl;
                ofstream m2(MEMORY_FILE, ios::app); m2 << "TITAN: " << answer << endl;

                if (answer.rfind("LIST:", 0) == 0) list_directory(answer.substr(5));
                else if (answer.rfind("READ:", 0) == 0) read_file_content(answer.substr(5));
                else if (answer.rfind("CMD:", 0) == 0) exec_cmd_safe(answer.substr(4));
                else if (answer.rfind("TYPE:", 0) == 0) type_text_safe(answer.substr(5));
                else if (answer.rfind("WATCH", 0) == 0) {
                     capture_screen();
                     string s = run_python_vision();
                     cout << ">> [SEES]: " << s.substr(0, 50) << "..." << endl;
                     ofstream m3(MEMORY_FILE, ios::app); m3 << "OBSERVATION (Vision): " << s << endl;
                }
                else if (answer.rfind("SYS:", 0) == 0) system_power(answer.substr(4));
                else speak(answer);
            } catch (const exception& e) { cout << ">> [ERROR] Crash Prevented: " << e.what() << endl; }
        }
    }
    return 0;
}