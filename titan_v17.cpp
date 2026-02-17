#include <winsock2.h>
#include <windows.h>
#include <algorithm>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include "httplib.h"
#include "json.hpp"

using namespace std;
using json = nlohmann::json;
namespace fs = std::filesystem;

// --- CONFIGURATION ---
const string WORKSPACE_ROOT = "C:\\TitanWorkspace";
const size_t MAX_READ_SIZE = 5000; 
const vector<string> ALLOWED_EXTENSIONS = {
    ".txt", ".md", ".cpp", ".hpp", ".h", ".py", ".json", ".log"
};

const string MEMORY_FILE = "titan_memory.txt";
const string ACTION_LOG_FILE = "titan_actions.log";
const string VOICE_INPUT_FILE = "titan_voice_input.txt";
const string TESSERACT_PATH = R"(C:\Program Files\Tesseract-OCR\tesseract.exe)";

// --- LOGGING & SECURITY ---

void log_action(string action_type, string details) {
    ofstream f(ACTION_LOG_FILE, ios::app);
    time_t now = time(0);
    string time_str = ctime(&now);
    if (!time_str.empty()) time_str.pop_back();
    if (f.is_open()) {
        f << "[" << time_str << "] [" << action_type << "] " << details << endl;
        f.close();
    }
}

bool is_safe_path(string input_path) {
    try {
        fs::path p = fs::absolute(input_path);
        fs::path root = fs::absolute(WORKSPACE_ROOT);
        string p_str = p.string();
        string r_str = root.string();
        transform(p_str.begin(), p_str.end(), p_str.begin(), ::tolower);
        transform(r_str.begin(), r_str.end(), r_str.begin(), ::tolower);
        return p_str.find(r_str) == 0;
    } catch (...) { return false; }
}

bool is_allowed_extension(string path) {
    string ext = fs::path(path).extension().string();
    for (const auto &e : ALLOWED_EXTENSIONS) {
        if (e == ext) return true;
    }
    return false;
}

// --- FILESYSTEM OPERATIONS ---

void list_directory(string relative_path) {
    fs::path target = fs::path(WORKSPACE_ROOT) / relative_path;
    if (!is_safe_path(target.string())) {
        cout << ">> [SECURITY]: ACCESS DENIED." << endl;
        return;
    }

    string contents = "FILES IN " + relative_path + ":\n";
    try {
        if (fs::exists(target) && fs::is_directory(target)) {
            for (const auto &entry : fs::directory_iterator(target)) {
                contents += entry.path().filename().string() + " | ";
            }
            cout << ">> [EXPLORER]: Listed " << relative_path << endl;
            ofstream f(MEMORY_FILE, ios::app);
            f << "OBSERVATION (File System): " << contents << endl;
        } else {
            cout << ">> [EXPLORER]: Path not found." << endl;
        }
    } catch (...) { cout << ">> [ERROR]: Access Error." << endl; }
}

void read_file_content(string relative_path) {
    fs::path target = fs::path(WORKSPACE_ROOT) / relative_path;

    if (!is_safe_path(target.string()) || !is_allowed_extension(target.string())) {
        cout << ">> [SECURITY]: READ BLOCKED." << endl;
        return;
    }

    try {
        if (fs::file_size(target) > MAX_READ_SIZE) {
            cout << ">> [SECURITY]: READ BLOCKED (File too large)." << endl;
            return;
        }
    } catch (...) { return; }

    ifstream f(target);
    if (f.is_open()) {
        stringstream buffer;
        buffer << f.rdbuf();
        string content = buffer.str();

        cout << ">> [READER]: Read " << relative_path << endl;

        ofstream mem(MEMORY_FILE, ios::app);
        mem << "OBSERVATION (File Read): " << relative_path
            << " | SIZE: " << content.size() << " bytes | SUMMARY_REQUIRED" << endl;
    } else {
        cout << ">> [ERROR]: Cannot read file." << endl;
    }
}

// --- V17 NEW FEATURE: THE WRITER ✍️ ---
bool get_human_confirmation(string action); // Forward declaration

void write_file(string relative_path, string content) {
    fs::path target = fs::path(WORKSPACE_ROOT) / relative_path;

    // 1. Security Checks
    if (!is_safe_path(target.string())) {
        cout << ">> [SECURITY]: WRITE BLOCKED. Path unsafe." << endl;
        return;
    }
    if (!is_allowed_extension(target.string())) {
        cout << ">> [SECURITY]: WRITE BLOCKED. Extension not allowed." << endl;
        return;
    }

    // 2. Human Confirmation for Overwrites
    string status = fs::exists(target) ? "OVERWRITE" : "CREATE";
    if (!get_human_confirmation(status + " file: " + relative_path)) {
        cout << ">> [SECURITY]: Operation cancelled by user." << endl;
        return;
    }

    // 3. Execution
    ofstream f(target);
    if (f.is_open()) {
        f << content;
        f.close();
        cout << ">> [WRITER]: Successfully wrote to " << relative_path << endl;
        log_action("WRITE", relative_path);
        
        ofstream mem(MEMORY_FILE, ios::app);
        mem << "OBSERVATION: Successfully wrote file " << relative_path << endl;
    } else {
        cout << ">> [ERROR]: Write failed." << endl;
    }
}

// --- HARDWARE & INPUT UTILS ---

void speak(string t) {
    string safe_t = "";
    for (char c : t) {
        if (c == '\'') safe_t += "''";
        else if (c != '\n') safe_t += c;
    }
    string c = "powershell -Command \"Add-Type -AssemblyName System.Speech; (New-Object System.Speech.Synthesis.SpeechSynthesizer).Speak('" + safe_t + "');\"";
    system(c.c_str());
}

void click_mouse_safe(int x, int y) {
    int screen_w = GetSystemMetrics(SM_CXSCREEN);
    int screen_h = GetSystemMetrics(SM_CYSCREEN);
    if (x < 0 || x > screen_w || y < 0 || y > screen_h) return;
    SetCursorPos(x, y); Sleep(50);
    INPUT i[2] = {}; i[0].type = INPUT_MOUSE; i[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN; 
    i[1].type = INPUT_MOUSE; i[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
    SendInput(2, i, sizeof(INPUT));
    cout << ">> [HANDS]: Clicked " << x << "," << y << endl;
}

void type_text_safe(string text) {
    for (unsigned char c : text) {
        INPUT inputs[2] = {};
        inputs[0].type = INPUT_KEYBOARD; inputs[0].ki.wScan = c; inputs[0].ki.dwFlags = KEYEVENTF_UNICODE;
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
    f << "import pyautogui, subprocess; "
         "pyautogui.screenshot('screen_memory.png'); subprocess.run([r'"
      << TESSERACT_PATH
      << "', 'screen_memory.png', 'stdout'], capture_output=True)";
    f.close();
    system("python titan_vision.py > titan_vision_data.txt");
    ifstream r("titan_vision_data.txt"); string l, res = "";
    while (getline(r, l)) res += l + " ";
    return res.empty() ? "Nothing" : res;
}

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
    generate_hearing_script(); system("python titan_ears.py");
    ifstream file(VOICE_INPUT_FILE); string line, t = "";
    if (file.is_open()) { while (getline(file, line)) t += line + " "; file.close(); }
    return t.empty() ? "" : t;
}

// --- CORE EXECUTION ---

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
    SYSTEM_POWER_STATUS sps;
    if (GetSystemPowerStatus(&sps)) return to_string((int)sps.BatteryLifePercent) + "%";
    return "Unknown";
}

string load_recent_memory() {
    ifstream f(MEMORY_FILE);
    string l, m = "";
    vector<string> h;
    while (getline(f, l)) h.push_back(l);
    size_t s = (h.size() > 8) ? h.size() - 8 : 0;
    for (size_t i = s; i < h.size(); i++) m += h[i] + "\n";
    return m;
}

void execute_single_action(string action_line) {
    if (action_line == "NONE" || action_line.empty()) return;

    // Fix for multiline "WRITE" commands being prefixed with whitespace
    size_t first_char = action_line.find_first_not_of(" \n\r\t");
    if (first_char != string::npos) action_line = action_line.substr(first_char);

    cout << ">> [EXECUTOR]: Processing Action..." << endl;

    if (action_line.rfind("LIST:", 0) == 0) {
        list_directory(action_line.substr(5));
    } else if (action_line.rfind("READ:", 0) == 0) {
        read_file_content(action_line.substr(5));
    } else if (action_line.rfind("WRITE", 0) == 0) { 
        // ROBUST PARSING: Handles "WRITE:" and "WRITE|"
        // Find the first pipe '|' which separates filename from content
        size_t first_pipe = action_line.find('|');
        if (first_pipe != string::npos) {
            string fname_part = action_line.substr(0, first_pipe);
            // Extract filename from "WRITE:filename" or "WRITE|filename"
            size_t separator = fname_part.find_first_of(":|");
            if (separator != string::npos) {
                string fname = fname_part.substr(separator + 1);
                string fcontent = action_line.substr(first_pipe + 1);
                write_file(fname, fcontent);
            }
        } else {
            cout << ">> [EXECUTOR]: Malformed WRITE command (Missing '|')." << endl;
        }
    } else if (action_line.rfind("CMD:", 0) == 0) {
        exec_cmd_safe(action_line.substr(4));
    } else if (action_line.rfind("TYPE:", 0) == 0) {
        type_text_safe(action_line.substr(5));
    } else if (action_line.rfind("CLICK:", 0) == 0) {
        string coords = action_line.substr(6);
        size_t comma = coords.find(',');
        if (comma != string::npos) {
            try {
                int x = stoi(coords.substr(0, comma));
                int y = stoi(coords.substr(comma + 1));
                click_mouse_safe(x, y);
            } catch (...) { cout << ">> [EXECUTOR]: Invalid CLICK coordinates." << endl; }
        }
    } else if (action_line == "WATCH") {
        capture_screen();
        string s = run_python_vision();
        size_t len = s.size() > 50 ? 50 : s.size();
        cout << ">> [SEES]: " << s.substr(0, len) << "..." << endl;
        ofstream vm(MEMORY_FILE, ios::app);
        vm << "OBSERVATION (Vision): " << s << endl;
    } else if (action_line.rfind("SYS:", 0) == 0) {
        system_power(action_line.substr(4));
    } else {
        cout << ">> [EXECUTOR]: Unknown command format." << endl;
    }
}
int main() {
    // Startup Checks
    if (!fs::exists(WORKSPACE_ROOT)) {
        fs::create_directory(WORKSPACE_ROOT);
        cout << ">> [INIT]: Created Workspace at " << WORKSPACE_ROOT << endl;
    }
    
    remove("titan_ears.py"); remove("titan_capture.py"); remove("titan_vision.py");

    httplib::Client cli("http://localhost:11434"); cli.set_read_timeout(120);
    
    cout << ">> [TITAN V17] CREATOR MODE ONLINE." << endl;
    cout << ">> [SECURITY]: Write access enabled (Sandboxed)." << endl;
    speak("Titan V17 Online.");

    while (true) {
        cout << "\n>> Commander: ";
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

        {
            ofstream m(MEMORY_FILE, ios::app);
            m << "USER: " << user_input << endl;
        }

        string strict_prompt = R"(You are Titan V17. You can now CREATE files.

RULES:
1. To create code, use WRITE.
2. Format: WRITE:filename|content
3. Do not assume file execution.

RESPONSE FORMAT:
PLAN: [Reasoning]
ACTION: [Command]

ALLOWED ACTIONS:
- LIST:<relative_path>
- READ:<relative_path>
- WRITE:<filename>|<content>
- CMD:<command>
- TYPE:<text> | CLICK:<x,y> | WATCH | SYS:LOCK
- NONE)";

        string system_instruction = strict_prompt + "\n\nSYSTEM STATE:\nWorkspace: " + WORKSPACE_ROOT + 
                                    "\nBattery: " + get_battery_status() + 
                                    "\nRecent Memory:\n" + load_recent_memory();

        json req = {{"model", "llama3.2"},
                    {"prompt", system_instruction + "\nUser: " + user_input},
                    {"stream", false}};
        
        auto res = cli.Post("/api/generate", req.dump(), "application/json");

        if (res && res->status == 200) {
            try {
                json response = json::parse(res->body);
                string answer = response["response"];

                cout << ">> [TITAN]: " << answer << endl;
                
                ofstream m2(MEMORY_FILE, ios::app);
                m2 << "TITAN: " << answer << endl;

                // Parse ACTION (Multiline Fix)
                string action = "NONE";
                size_t action_pos = answer.find("ACTION:");
                if (action_pos != string::npos) {
                    // Capture EVERYTHING after ACTION: (including newlines)
                    string full_action = answer.substr(action_pos + 7);
                    
                    // Trim leading whitespace only
                    size_t first_real = full_action.find_first_not_of(" \t\r\n");
                    if (first_real != string::npos) {
                        action = full_action.substr(first_real);
                    }
                }

                if (action != "NONE") {
                    execute_single_action(action);
                } else {
                    cout << ">> [SYSTEM]: No executable action found." << endl;
                }

            } catch (const exception &e) {
                cout << ">> [ERROR] System Stability Alert: " << e.what() << endl;
            }
        }
    }
    return 0;
}