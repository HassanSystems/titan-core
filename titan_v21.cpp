// TITAN CORE - V22 (Hardened Linear Execution, Async Telemetry, Llama 3.2)
// Architecture: Strict C++ Execution Engine -> Dynamic Python Bridges -> LLM
#define _CRT_SECURE_NO_WARNINGS

#include <winsock2.h>
#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <chrono>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>

#include "httplib.h"
#include "json.hpp"

using namespace std;
using json = nlohmann::json;
namespace fs = std::filesystem;

const string WORKSPACE_ROOT = "C:\\TitanWorkspace";
const string MEMORY_FILE = "titan_memory.txt";
const string ACTION_LOG_FILE = "titan_actions.log";
const string VOICE_INPUT_FILE = "titan_voice_input.txt";
const string TESSERACT_PATH = R"(C:\Program Files\Tesseract-OCR\tesseract.exe)";

const size_t MAX_READ_SIZE = 10000; 
const size_t MAX_MEMORY_LINES = 30; 
const int MAX_RETRY_ATTEMPTS = 2;   
const int LLM_TIMEOUT_SEC = 180;    

const vector<string> ALLOWED_EXTENSIONS = {
    ".txt", ".md", ".cpp", ".hpp", ".h", ".py", ".json", ".log",
    ".js", ".html", ".css", ".bat", ".ps1", ".java", ".cs", ".xml",
    ".yaml", ".yml", ".ini", ".cfg", ".sh", ".rb", ".go"
};

// ... [TELEMETRY LOGGER AND HELPER FUNCTIONS REMAIN EXACTLY THE SAME] ...
class TelemetryLogger {
private:
    std::ofstream log_file;
    std::queue<std::string> message_queue;
    std::mutex queue_mutex;
    std::condition_variable cv;
    std::thread worker_thread;
    bool running;

    void process_queue() {
        while (running) {
            std::unique_lock<std::mutex> lock(queue_mutex);
            cv.wait(lock, [this]() { return !message_queue.empty() || !running; });

            while (!message_queue.empty()) {
                log_file << message_queue.front() << "\n";
                message_queue.pop();
            }
            log_file.flush();
        }
    }
public:
    TelemetryLogger(const std::string& filename) : running(true) {
        log_file.open(filename, std::ios::app);
        worker_thread = std::thread(&TelemetryLogger::process_queue, this);
    }

    ~TelemetryLogger() {
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            running = false;
        }
        cv.notify_one();
        if (worker_thread.joinable()) {
            worker_thread.join();
        }
    }

    void log_metric(const std::string& metric_name, int64_t duration_us) {
        std::string entry = "[TELEMETRY] " + metric_name + " : " + std::to_string(duration_us) + " us";
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            message_queue.push(entry);
        }
        cv.notify_one();
    }
};

TelemetryLogger perf_logger("titan_telemetry.log");

void log_action(const string &action_type, const string &details) {
    ofstream f(ACTION_LOG_FILE, ios::app);
    time_t now = time(0);
    string time_str = ctime(&now);
    if (!time_str.empty()) time_str.pop_back();
    
    if (f.is_open()) {
        f << "[" << time_str << "] [" << action_type << "] " << details << endl;
    }
}

bool is_safe_path(const string &input_path) {
    try {
        fs::path p = fs::absolute(input_path);
        fs::path root = fs::absolute(WORKSPACE_ROOT);
        string p_str = p.string();
        string r_str = root.string();
        
        transform(p_str.begin(), p_str.end(), p_str.begin(), ::tolower);
        transform(r_str.begin(), r_str.end(), r_str.begin(), ::tolower);
        
        return p_str.find(r_str) == 0;
    } catch (...) {
        return false;
    }
}

bool is_allowed_extension(const string &path) {
    string ext = fs::path(path).extension().string();
    transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    for (const auto &e : ALLOWED_EXTENSIONS) {
        if (e == ext) return true;
    }
    return false;
}

void append_memory(const string &entry) {
    ofstream f(MEMORY_FILE, ios::app);
    if (f.is_open()) f << entry << endl;
}

string load_recent_memory() {
    ifstream f(MEMORY_FILE);
    string l;
    vector<string> lines;
    while (getline(f, l)) lines.push_back(l);

    size_t start = (lines.size() > MAX_MEMORY_LINES) ? lines.size() - MAX_MEMORY_LINES : 0;
    string result;
    for (size_t i = start; i < lines.size(); i++) {
        result += lines[i] + "\n";
    }
    return result;
}

bool get_human_confirmation(const string &action) {
    cout << "\n>> [CONFIRM]: " << action << " (y/n): ";
    string choice;
    getline(cin, choice);
    return (choice == "y" || choice == "Y" || choice == "yes" || choice == "YES");
}

string list_directory(const string &relative_path) {
    fs::path target = fs::path(WORKSPACE_ROOT) / relative_path;
    if (!is_safe_path(target.string())) {
        return "ACCESS DENIED";
    }

    string contents;
    try {
        if (fs::exists(target) && fs::is_directory(target)) {
            for (const auto &entry : fs::directory_iterator(target)) {
                string type = entry.is_directory() ? "[DIR] " : "[FILE]";
                contents += type + " " + entry.path().filename().string() + "\n";
            }
            cout << ">> [EXPLORER]: Listed " << relative_path << endl;
            append_memory("OBSERVATION (Dir " + relative_path + "): " + contents);
            return contents;
        } 
        return "Path not found";
    } catch (...) {
        return "Access Error";
    }
}

string read_file_content(const string &relative_path) {
    fs::path target = fs::path(WORKSPACE_ROOT) / relative_path;
    if (!is_safe_path(target.string()) || !is_allowed_extension(target.string())) {
        return "READ BLOCKED";
    }

    try {
        if (fs::file_size(target) > MAX_READ_SIZE) return "File too large";
    } catch (...) {
        return "Error checking file";
    }

    ifstream f(target);
    if (f.is_open()) {
        stringstream buffer;
        buffer << f.rdbuf();
        string content = buffer.str();
        cout << ">> [READER]: Read " << relative_path << " (" << content.size() << " bytes)" << endl;
        append_memory("OBSERVATION (Read " + relative_path + "):\n" + content);
        return content;
    }
    return "Cannot read file";
}

bool write_file(const string &relative_path, const string &content) {
    fs::path target = fs::path(WORKSPACE_ROOT) / relative_path;

    if (!is_safe_path(target.string())) {
        cout << ">> [SECURITY]: WRITE BLOCKED — path escapes workspace." << endl;
        return false;
    }
    if (!is_allowed_extension(target.string())) {
        cout << ">> [SECURITY]: WRITE BLOCKED — extension not allowed." << endl;
        return false;
    }

    fs::path parent = target.parent_path();
    if (!fs::exists(parent)) {
        try {
            fs::create_directories(parent);
        } catch (...) {
            return false;
        }
    }

    string status = fs::exists(target) ? "OVERWRITE" : "CREATE";
    if (!get_human_confirmation(status + " file: " + relative_path + " (" + to_string(content.size()) + " bytes)")) {
        cout << ">> [CANCELLED]: User declined." << endl;
        return false;
    }

    ofstream f(target);
    if (f.is_open()) {
        f << content;
        f.close();
        cout << ">> [WRITER]: Wrote " << relative_path << " (" << content.size() << " bytes)" << endl;
        log_action("WRITE", relative_path + " (" + to_string(content.size()) + " bytes)");
        append_memory("RESULT: Wrote file " + relative_path + " (" + to_string(content.size()) + " bytes)");
        return true;
    }
    return false;
}

string run_command_capture(const string &command) {
    string full_cmd = "cd /d \"" + WORKSPACE_ROOT + "\" && " + command + " 2>&1";
    
    FILE *pipe = _popen(full_cmd.c_str(), "r");
    if (!pipe) return "EXIT:1\nERROR: Failed to execute command";

    string result;
    char buffer[512];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        result += buffer;
    }
    int exit_code = _pclose(pipe);

    if (result.size() > 3000) {
        result = result.substr(0, 1500) + "\n... [TRUNCATED " +
                 to_string(result.size() - 2000) + " chars] ...\n" +
                 result.substr(result.size() - 500);
    }

    string status = (exit_code == 0) ? "SUCCESS" : "FAILED";
    cout << ">> [CMD " << status << "]: " << command << endl;

    if (!result.empty()) {
        string preview = result.substr(0, min((size_t)300, result.size()));
        cout << ">> [OUTPUT]:\n" << preview;
        if (result.size() > 300) cout << "\n... (truncated)";
        cout << endl;
    }

    return "EXIT:" + to_string(exit_code) + "\n" + result;
}

void speak(const string &t) {
    string safe_t;
    for (char c : t) {
        if (c == '\'') safe_t += "''";
        else if (c != '\n' && c != '\r') safe_t += c;
    }
    if (safe_t.size() > 200) safe_t = safe_t.substr(0, 200);
    
    string cmd = "powershell -Command \"Add-Type -AssemblyName System.Speech; (New-Object System.Speech.Synthesis.SpeechSynthesizer).Speak('" + safe_t + "');\"";
    system(cmd.c_str());
}

void click_mouse_safe(int x, int y) {
    int screen_w = GetSystemMetrics(SM_CXSCREEN);
    int screen_h = GetSystemMetrics(SM_CYSCREEN);
    
    if (x < 0 || x > screen_w || y < 0 || y > screen_h) return;
    
    SetCursorPos(x, y);
    Sleep(50);
    INPUT i[2] = {};
    i[0].type = INPUT_MOUSE; i[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    i[1].type = INPUT_MOUSE; i[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
    SendInput(2, i, sizeof(INPUT));
    cout << ">> [CLICK]: " << x << "," << y << endl;
}

void type_text_safe(const string &text) {
    for (unsigned char c : text) {
        INPUT inputs[2] = {};
        inputs[0].type = INPUT_KEYBOARD; inputs[0].ki.wScan = c; inputs[0].ki.dwFlags = KEYEVENTF_UNICODE;
        inputs[1].type = INPUT_KEYBOARD; inputs[1].ki.wScan = c; inputs[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
        SendInput(2, inputs, sizeof(INPUT));
        Sleep(10);
    }
}

void capture_screen() {
    ofstream f("titan_capture.py");
    // Force Python to save the image directly into the Workspace directory
    f << "import pyautogui\n"
      << "try:\n"
      << "    pyautogui.screenshot().save(r'" << WORKSPACE_ROOT << "\\screen_memory.png')\n"
      << "except Exception as e:\n"
      << "    print('Screenshot failed:', e)\n";
    f.close();
    system("python titan_capture.py");
}

string run_python_vision() {
    ofstream f("titan_vision.py");
    f << "import pyautogui, subprocess; pyautogui.screenshot('screen_memory.png'); "
         "subprocess.run([r'" << TESSERACT_PATH << "', 'screen_memory.png', 'stdout'], capture_output=True)";
    f.close();
    system("python titan_vision.py > titan_vision_data.txt");
    
    ifstream r("titan_vision_data.txt");
    string l, res;
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
    ofstream file("titan_ears.py");
    file << python_code;
    file.close();
}

string listen_to_user() {
    generate_hearing_script();
    system("python titan_ears.py");
    ifstream file(VOICE_INPUT_FILE);
    string line, t;
    if (file.is_open()) {
        while (getline(file, line)) t += line + " ";
        file.close();
    }
    return t;
}

string get_battery_status() {
    SYSTEM_POWER_STATUS sps;
    if (GetSystemPowerStatus(&sps)) return to_string((int)sps.BatteryLifePercent) + "%";
    return "Unknown";
}

void system_power(const string &action) {
    if (action == "LOCK" && get_human_confirmation("LOCK PC")) LockWorkStation();
}

// --- HARDENED PARSER ---
struct Action {
    string type;    
    string target;  
    string content; 
};

// This parser is built to survive Llama 3.2 formatting hallucinations.
// It explicitly hunts for specific tool keywords rather than relying on strict line breaks.
vector<Action> parse_actions(const string &response) {
    vector<Action> actions;
    size_t pos = 0;

    while (pos < response.size()) {
        size_t action_start = response.find("ACTION:", pos);
        if (action_start == string::npos) break;

        action_start += 7; // Skip "ACTION:"
        while (action_start < response.size() && (response[action_start] == ' ' || response[action_start] == '\t')) {
            action_start++;
        }

        if (action_start >= response.size()) break;

        string remaining = response.substr(action_start);
        Action act;

        // Extract the raw type string until a space, colon, or newline
        size_t end_of_type = remaining.find_first_of(" :\n\r");
        string raw_type = remaining.substr(0, end_of_type);
        
        // Clean the raw type
        raw_type.erase(remove(raw_type.begin(), raw_type.end(), ' '), raw_type.end());

        if (raw_type == "WRITE") {
            act.type = "WRITE";
            size_t colon_pos = remaining.find(':');
            if (colon_pos == string::npos) { pos = action_start + 5; continue; }
            
            size_t end_of_target = remaining.find_first_of("\n\r<", colon_pos);
            if (end_of_target == string::npos) end_of_target = remaining.size();
            
            act.target = remaining.substr(colon_pos + 1, end_of_target - colon_pos - 1);
            
            // Clean target
            act.target.erase(0, act.target.find_first_not_of(" \t\r\n"));
            act.target.erase(act.target.find_last_not_of(" \t\r\n") + 1);

            size_t code_open = remaining.find("<<<CODE");
            size_t code_close = remaining.find("CODE>>>");

            if (code_open != string::npos && code_close != string::npos && code_close > code_open) {
                size_t content_start = code_open + 7;
                act.content = remaining.substr(content_start, code_close - content_start);
                act.content.erase(0, act.content.find_first_not_of("\r\n"));
                act.content.erase(act.content.find_last_not_of("\r\n") + 1);
                pos = action_start + code_close + 7;
            } else {
                 pos = action_start + end_of_target;
            }
            actions.push_back(act);
        }
        else if (raw_type == "CMD" || raw_type == "LIST" || raw_type == "READ" || raw_type == "TYPE" || raw_type == "CLICK" || raw_type == "SYS" || raw_type == "SPEAK" || raw_type == "SEARCH") {
            act.type = raw_type;
            if (raw_type == "SEARCH") act.type = "SEARCH:"; // Append colon for search handler
            
            size_t start_of_target = remaining.find_first_of(":") + 1;
            size_t end_of_target = remaining.find_first_of("\n\r");
            if (end_of_target == string::npos) end_of_target = remaining.size();

            act.target = remaining.substr(start_of_target, end_of_target - start_of_target);
            
            // Clean Target
            act.target.erase(0, act.target.find_first_not_of(" \t\r"));
            act.target.erase(act.target.find_last_not_of(" \t\r") + 1);
            
            // Strip out inline comments from the target (e.g. "python hello.py # Run the script")
            size_t hash_pos = act.target.find('#');
            if(hash_pos != string::npos) {
                 act.target = act.target.substr(0, hash_pos);
                 act.target.erase(act.target.find_last_not_of(" \t") + 1);
            }

            pos = action_start + end_of_target;
            actions.push_back(act);
        }
        else if (raw_type == "WATCH") {
            act.type = "WATCH";
            pos = action_start + 5;
            actions.push_back(act);
        }
        else {
            // Unrecognized action, skip line
            size_t next_line = remaining.find('\n');
            pos = (next_line != string::npos) ? action_start + next_line : response.size();
        }
    }
    return actions;
}

string execute_action(const Action &act) {
    if (act.type == "WRITE") {
        return write_file(act.target, act.content) ? "SUCCESS: Wrote " + act.target : "FAILED: Could not write " + act.target;
    } else if (act.type == "CMD") {
        if (!get_human_confirmation("Run command: " + act.target)) return "CANCELLED by user";
        string output = run_command_capture(act.target);
        log_action("CMD", act.target);
        append_memory("CMD [" + act.target + "] -> " + output);
        return output;
    } else if (act.type == "LIST") {
        return list_directory(act.target);
    } else if (act.type == "READ") {
        return read_file_content(act.target);
    } else if (act.type == "TYPE") {
        type_text_safe(act.target);
        return "SUCCESS: Typed text";
    } else if (act.type == "CLICK") {
        size_t comma = act.target.find(',');
        if (comma != string::npos) {
            try {
                click_mouse_safe(stoi(act.target.substr(0, comma)), stoi(act.target.substr(comma + 1)));
                return "SUCCESS: Clicked " + act.target;
            } catch (...) { return "FAILED: Invalid coordinates"; }
        }
        return "FAILED: Invalid CLICK format";
    } else if (act.type == "WATCH") {
        capture_screen();
        string ocr_result = run_command_capture("\"" + TESSERACT_PATH + "\" screen_memory.png stdout");
        append_memory("VISUAL OBSERVATION (Screen Capture):\n" + ocr_result);
        return "Saw: " + ocr_result;
    } else if (act.type == "SYS") {
        system_power(act.target);
        return "System: " + act.target;
    } else if (act.type == "SPEAK") {
        speak(act.target);
        return "Spoke: " + act.target;
    } else if (act.type.find("SEARCH:") == 0) {
        string query = act.target; // Using act.target since parser was updated
        ofstream f("titan_search.py");
        f << "import sys, warnings\n"
          << "sys.stdout.reconfigure(encoding='utf-8')\n"
          << "warnings.filterwarnings('ignore')\n"
          << "from duckduckgo_search import DDGS\n"
          << "try:\n"
          << "    results = DDGS().text('" << query << "', max_results=3)\n"
          << "    for r in results:\n"
          << "        print(f\"Title: {r.get('title')}\\nBody: {r.get('body')}\\n\")\n"
          << "except Exception as e:\n"
          << "    print(f\"Search error: {e}\")\n";
        f.close();

        system("python titan_search.py > titan_search_data.txt 2> NUL");
        ifstream r("titan_search_data.txt");
        string l, res;
        while (getline(r, l)) {
            res += l + "\n";
        }
        
        append_memory("OBSERVATION (Web Search): \n" + res);
        return res;
    }
    return "Unknown action type: " + act.type;
}

string call_llm(httplib::Client &cli, const string &system_context, const string &user_msg) {
    json req = {
        {"model", "llama3.2"}, // Swapped back to the lightweight Llama
        {"prompt", system_context + "\nUser: " + user_msg},
        {"stream", false}
    };

    cout << ">> [THINKING]..." << endl;
    auto res = cli.Post("/api/generate", req.dump(), "application/json");

    if (res && res->status == 200) {
        try {
            return json::parse(res->body)["response"];
        } catch (const exception &e) {
            return "ERROR_INTERNAL: " + string(e.what());
        }
    }
    return "ERROR_NETWORK: No response (status: " + to_string(res ? res->status : -1) + ")";
}

string build_system_prompt() {
    return R"(You are Titan V22, a local, autonomous AI agent.

RULES:
1. You can output MULTIPLE actions. They will execute sequentially.
2. If an action fails, analyze the error and output a new action to fix it.
3. PERCEPTION: If the user says "look at my screen", you MUST use ACTION: WATCH first.
4. DO NOT output conversational filler. ONLY output actions.

FORMAT:
PLAN: [Reasoning]
ACTION: [ActionType]

ACTIONS:
- WRITE:<filename>
<<<CODE
[Code here]
CODE>>>
- CMD:<shell command>
- WATCH (Takes a screenshot and reads text via OCR)
- SPEAK:<text>
- SYS:LOCK
- SEARCH:<query> (Searches the web using ddgs and returns top 3 results)

EXAMPLES:
User: "hello world python"
PLAN: Create file and run it.
ACTION: WRITE:hello.py
<<<CODE
print("Hello, World!")
CODE>>>
ACTION: CMD:python hello.py
)";
}

int main() {
    SetConsoleOutputCP(CP_UTF8);
    
    if (!fs::exists(WORKSPACE_ROOT)) {
        fs::create_directories(WORKSPACE_ROOT);
        cout << ">> [INIT]: Created Workspace at " << WORKSPACE_ROOT << endl;
    }

    remove("titan_ears.py");
    remove("titan_capture.py");
    remove("titan_vision.py");
    remove("titan_vision_data.txt");
    remove("titan_search.py");
    remove("titan_search_data.txt");

    httplib::Client cli("http://localhost:11434");
    cli.set_read_timeout(LLM_TIMEOUT_SEC);

    cout << "\n=== TITAN V22 CREATOR MODE ===\n";
    cout << ">> Workspace: " << WORKSPACE_ROOT << endl;
    speak("Titan V22 Online.");

    while (true) {
        cout << "\n>> Commander: ";
        string user_input;
        getline(cin, user_input);

        if (user_input.empty()) continue;

        if (user_input == "LISTEN" || user_input == "listen") {
            string spoken = listen_to_user();
            if (spoken.length() > 1) {
                user_input = spoken;
                cout << ">> [VOICE]: " << user_input << endl;
            } else {
                cout << ">> [VOICE]: Nothing heard." << endl;
                continue;
            }
        }

        if (user_input == "exit" || user_input == "EXIT" || user_input == "quit") break;

        append_memory("USER: " + user_input);

        string system_context = build_system_prompt() + 
            "\n\nSYSTEM STATE:\nWorkspace: " + WORKSPACE_ROOT + 
            "\nBattery: " + get_battery_status() + 
            "\nRecent Memory:\n" + load_recent_memory();

        auto llm_start = chrono::steady_clock::now();
        string answer = call_llm(cli, system_context, user_input);
        auto llm_end = chrono::steady_clock::now();
        
        auto llm_duration = chrono::duration_cast<chrono::microseconds>(llm_end - llm_start).count();
        perf_logger.log_metric("LLM_INFERENCE", llm_duration);

        if (answer.rfind("ERROR_", 0) == 0) {
            cout << ">> [ERROR]: " << answer << endl;
            continue;
        }

        cout << "\n>> [TITAN]:\n" << answer << endl;
        append_memory("TITAN: " + answer);

        vector<Action> actions = parse_actions(answer);
        if (actions.empty()) {
            cout << ">> [SYSTEM]: No actions parsed. Waiting for input." << endl;
            continue;
        }

        string last_cmd_output;
        bool had_error = false;

        for (size_t i = 0; i < actions.size(); i++) {
            cout << "\n>> Action " << (i + 1) << "/" << actions.size() << ": " << actions[i].type;
            if (!actions[i].target.empty() && actions[i].type != "WATCH") {
                cout << " -> " << actions[i].target;
            }
            cout << endl;

            auto act_start = chrono::steady_clock::now();
            string result = execute_action(actions[i]);
            auto act_end = chrono::steady_clock::now();

            auto act_duration = chrono::duration_cast<chrono::microseconds>(act_end - act_start).count();
            perf_logger.log_metric("EXECUTE_" + actions[i].type, act_duration);

            if (actions[i].type == "CMD" && result.find("EXIT:0") == string::npos && result.find("CANCELLED") == string::npos) {
                had_error = true;
                last_cmd_output = result;
                cout << "\n>> [ERROR]: Command failed." << endl;
                break;
            }
            
            // Hard stop for perception commands to re-feed context
            if (actions[i].type == "WATCH" || actions[i].type.find("SEARCH:") == 0) {
                 break;
            }
        }

        int retry = 0;
        while (had_error && retry < MAX_RETRY_ATTEMPTS) {
            retry++;
            cout << "\n>> [AUTO-FIX] Attempt " << retry << endl;

            string fix_prompt = "Code error detected. Output:\n\n" + last_cmd_output + 
                                "\n\nAnalyze error, WRITE fixed code, and CMD to run it.";
            
            append_memory("SYSTEM: Auto-fix attempt " + to_string(retry));

            auto fix_llm_start = chrono::steady_clock::now();
            string fix_answer = call_llm(cli, system_context + "\nERROR:\n" + last_cmd_output, fix_prompt);
            auto fix_llm_end = chrono::steady_clock::now();
            
            auto fix_llm_duration = chrono::duration_cast<chrono::microseconds>(fix_llm_end - fix_llm_start).count();
            perf_logger.log_metric("LLM_INFERENCE_FIX", fix_llm_duration);
            
            cout << "\n>> [TITAN FIX]:\n" << fix_answer << endl;
            
            vector<Action> fix_actions = parse_actions(fix_answer);
            had_error = false;

            for (size_t i = 0; i < fix_actions.size(); i++) {
                const auto &act = fix_actions[i];
                cout << "\n>> Fix Action " << (i + 1) << "/" << fix_actions.size() << ": " << act.type;
                if (!act.target.empty() && act.type != "WATCH") {
                    cout << " -> " << act.target;
                }
                cout << endl;

                auto act_start = chrono::steady_clock::now();
                string result = execute_action(act);
                auto act_end = chrono::steady_clock::now();

                auto act_duration = chrono::duration_cast<chrono::microseconds>(act_end - act_start).count();
                perf_logger.log_metric("EXECUTE_" + act.type, act_duration);

                if (act.type == "CMD" && result.find("EXIT:0") == string::npos && result.find("CANCELLED") == string::npos) {
                    had_error = true;
                    last_cmd_output = result;
                    cout << "\n>> [ERROR]: Fix command failed." << endl;
                    break;
                }
            }
        }
    }

    cout << "\n>> Shutting down." << endl;
    return 0;
}