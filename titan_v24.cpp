#define _CRT_SECURE_NO_WARNINGS
#pragma comment(lib, "ws2_32.lib")

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
#include <atomic>
#include <cstdint>

#include "httplib.h"
#include "json.hpp"
#include "protocol.h" 

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
const int LLM_TIMEOUT_SEC = 180;    
const int MAX_AGENT_TURNS = 15; 

const vector<string> ALLOWED_EXTENSIONS = {
    ".txt", ".md", ".cpp", ".hpp", ".h", ".py", ".json", ".log",
    ".js", ".html", ".css", ".bat", ".ps1", ".java", ".cs", ".xml",
    ".yaml", ".yml", ".ini", ".cfg", ".sh", ".rb", ".go"
};

struct QueuedCommand {
    Message msg;
    chrono::steady_clock::time_point queued_time;
};

queue<QueuedCommand> command_queue; 
mutex queue_mutex;
condition_variable queue_cv;
atomic<bool> titan_running{true};
uint64_t mySessionID = 0;

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
        if (worker_thread.joinable()) worker_thread.join();
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
    if (f.is_open()) f << "[" << time_str << "] [" << action_type << "] " << details << endl;
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
    } catch (...) { return false; }
}

bool is_allowed_extension(const string &path) {
    string ext = fs::path(path).extension().string();
    transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    for (const auto &e : ALLOWED_EXTENSIONS) if (e == ext) return true;
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
    for (size_t i = start; i < lines.size(); i++) result += lines[i] + "\n";
    return result;
}

bool get_human_confirmation(const string &action) {
    cout << "\n>> [WARNING] The agent is requesting a DANGEROUS action: " << action << endl;
    cout << ">> Do you approve? (y/n): ";
    string input;
    getline(cin, input);
    return (!input.empty() && (input[0] == 'y' || input[0] == 'Y'));
}

string sanitize_python_string(const string& input) {
    string output;
    for (char c : input) {
        if (c == '\\') output += "\\\\";
        else if (c == '\'') output += "\\'";
        else if (c == '\n') output += "\\n";
        else if (c == '\r') output += "\\r";
        else output += c;
    }
    return output;
}

string list_directory(const string &relative_path) {
    fs::path target = fs::path(WORKSPACE_ROOT) / relative_path;
    if (!is_safe_path(target.string())) return "ACCESS DENIED";
    string contents;
    try {
        if (fs::exists(target) && fs::is_directory(target)) {
            for (const auto &entry : fs::directory_iterator(target)) {
                string type = entry.is_directory() ? "[DIR] " : "[FILE]";
                contents += type + " " + entry.path().filename().string() + "\n";
            }
            return contents;
        } 
        return "Path not found";
    } catch (...) { return "Access Error"; }
}

string read_file_content(const string &relative_path) {
    fs::path target = fs::path(WORKSPACE_ROOT) / relative_path;
    if (!is_safe_path(target.string()) || !is_allowed_extension(target.string())) return "READ BLOCKED";
    try { if (fs::file_size(target) > MAX_READ_SIZE) return "File too large"; } catch (...) { return "Error checking file"; }

    ifstream f(target);
    if (f.is_open()) {
        stringstream buffer;
        buffer << f.rdbuf();
        return buffer.str();
    }
    return "Cannot read file";
}

bool write_file(const string &relative_path, const string &content) {
    fs::path target = fs::path(WORKSPACE_ROOT) / relative_path;
    if (!is_safe_path(target.string()) || !is_allowed_extension(target.string())) return false;
    fs::path parent = target.parent_path();
    if (!fs::exists(parent)) { try { fs::create_directories(parent); } catch (...) { return false; } }
    
    ofstream f(target);
    if (f.is_open()) {
        f << content;
        f.close();
        log_action("WRITE", relative_path);
        return true;
    }
    return false;
}

string run_command_capture(const string &command) {
    string lcase = command;
    transform(lcase.begin(), lcase.end(), lcase.begin(), ::tolower);
    if (lcase.find("shutdown") != string::npos || lcase.find("restart") != string::npos || 
        lcase.find("format") != string::npos || lcase.find("del /s") != string::npos ||
        lcase.find("rm -rf") != string::npos || lcase.find("rd /s") != string::npos) {
        return "EXIT:1\nERROR: Command blocked by safety policy. Use explicit SYS actions if needed.";
    }

    string full_cmd = "cd /d \"" + WORKSPACE_ROOT + "\" && " + command + " 2>&1";
    FILE *pipe = _popen(full_cmd.c_str(), "r");
    if (!pipe) return "EXIT:1\nERROR: Failed to execute command";

    string result;
    char buffer[512];
    while (fgets(buffer, sizeof(buffer), pipe)) result += buffer;
    int exit_code = _pclose(pipe);

    if (result.size() > 3000) result = result.substr(0, 1500) + "\n... [TRUNCATED] ...\n" + result.substr(result.size() - 500);
    return "EXIT:" + to_string(exit_code) + "\n" + result;
}

void speak(const string &t) {
    ofstream f("titan_speak.ps1");
    f << "Add-Type -AssemblyName System.Speech\n"
      << "$synth = New-Object System.Speech.Synthesis.SpeechSynthesizer\n"
      << "$synth.Speak(@'\n" << t << "\n'@)\n";
    f.close();
    system("powershell -ExecutionPolicy Bypass -File titan_speak.ps1");
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
    f << "import pyautogui\ntry:\n    pyautogui.screenshot().save(r'" << WORKSPACE_ROOT << "\\screen_memory.png')\nexcept Exception as e:\n    print('Screenshot failed:', e)\n";
    f.close();
    system("py titan_capture.py");
}

string get_battery_status() {
    SYSTEM_POWER_STATUS sps;
    if (GetSystemPowerStatus(&sps)) return to_string((int)sps.BatteryLifePercent) + "%";
    return "Unknown";
}

void system_power(const string &action) {
    if (action == "LOCK" && get_human_confirmation("LOCK PC")) LockWorkStation();
}

struct Action {
    string type;    
    string target;  
    string content; 
};

vector<Action> parse_actions(const string &response) {
    vector<Action> actions;
    size_t pos = 0;

    while (pos < response.size()) {
        size_t action_start = response.find("ACTION:", pos);
        if (action_start == string::npos) break;

        action_start += 7; 
        while (action_start < response.size() && (response[action_start] == ' ' || response[action_start] == '\t')) action_start++;
        if (action_start >= response.size()) break;

        string remaining = response.substr(action_start);
        Action act;

        size_t end_of_type = remaining.find_first_of(" :\n\r");
        string raw_type = remaining.substr(0, end_of_type);
        raw_type.erase(remove(raw_type.begin(), raw_type.end(), ' '), raw_type.end());

        if (raw_type == "WRITE") {
            act.type = "WRITE";
            size_t colon_pos = remaining.find(':');
            if (colon_pos == string::npos) { pos = action_start + 5; continue; }
            
            size_t end_of_target = remaining.find_first_of("\n\r<", colon_pos);
            if (end_of_target == string::npos) end_of_target = remaining.size();
            act.target = remaining.substr(colon_pos + 1, end_of_target - colon_pos - 1);
            act.target.erase(0, act.target.find_first_not_of(" \t\r\n"));
            act.target.erase(act.target.find_last_not_of(" \t\r\n") + 1);

            size_t code_open = remaining.find("<<<CODE");
            size_t code_close = remaining.find("CODE>>>");
            size_t close_tag_length = 7;

            if (code_close == string::npos && code_open != string::npos) {
                code_close = remaining.find(">>>", code_open + 7);
                close_tag_length = 3;
            }

            if (code_open != string::npos && code_close != string::npos && code_close > code_open) {
                size_t content_start = code_open + 7;
                act.content = remaining.substr(content_start, code_close - content_start);
                act.content.erase(0, act.content.find_first_not_of("\r\n"));
                act.content.erase(act.content.find_last_not_of("\r\n") + 1);
                pos = action_start + code_close + close_tag_length;
                actions.push_back(act);
            } else {
                act.type = "PARSE_ERROR";
                act.target = "Missing or malformed block. You MUST open with <<<CODE and close with CODE>>>. Try again.";
                pos = action_start + end_of_target;
                actions.push_back(act);
            }
        }
        else if (raw_type == "CMD" || raw_type == "LIST" || raw_type == "READ" || raw_type == "TYPE" || raw_type == "CLICK" || raw_type == "SYS" || raw_type == "SPEAK" || raw_type == "SEARCH") {
            act.type = raw_type; 
            
            size_t start_of_target = remaining.find_first_of(":") + 1;
            size_t end_of_target = remaining.find_first_of("\n\r");
            if (end_of_target == string::npos) end_of_target = remaining.size();

            act.target = remaining.substr(start_of_target, end_of_target - start_of_target);
            act.target.erase(0, act.target.find_first_not_of(" \t\r"));
            act.target.erase(act.target.find_last_not_of(" \t\r") + 1);
            
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
        } else {
            size_t next_line = remaining.find('\n');
            pos = (next_line != string::npos) ? action_start + next_line : response.size();
        }
    }
    return actions;
}

string execute_action(const Action &act) {
    if (act.type == "WRITE") {
        return write_file(act.target, act.content) ? "SUCCESS: Wrote " + act.target : "FAILED: Could not write " + act.target;
    } else if (act.type == "PARSE_ERROR") {
        return "ERROR: " + act.target;
    } else if (act.type == "CMD") {
        return run_command_capture(act.target);
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
            try { click_mouse_safe(stoi(act.target.substr(0, comma)), stoi(act.target.substr(comma + 1))); return "SUCCESS: Clicked"; } 
            catch (...) { return "FAILED: Invalid coordinates"; }
        }
        return "FAILED: Invalid CLICK format";
    } else if (act.type == "WATCH") {
        capture_screen();
        return "Saw: " + run_command_capture("\"" + TESSERACT_PATH + "\" screen_memory.png stdout");
    } else if (act.type == "SYS") {
        system_power(act.target);
        return "System: " + act.target;
    } else if (act.type == "SEARCH") { 
        string safe_query = sanitize_python_string(act.target); 
        
        ofstream f("titan_search.py");
        f << "import sys, urllib.request, urllib.parse, re\n"
          << "sys.stdout.reconfigure(encoding='utf-8')\n"
          << "try:\n"
          << "    url = 'https://html.duckduckgo.com/html/'\n"
          << "    data = urllib.parse.urlencode({'q': '" << safe_query << "'}).encode('utf-8')\n"
          << "    req = urllib.request.Request(url, data=data, headers={'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64)'})\n"
          << "    html = urllib.request.urlopen(req).read().decode('utf-8')\n"
          << "    results = re.findall(r'<a class=\"result__url\" href=\"[^\"]+\">(.*?)</a>.*?<a class=\"result__snippet[^\"]*\"[^>]*>(.*?)</a>', html, re.IGNORECASE | re.DOTALL)\n"
          << "    for title, snippet in results[:3]:\n"
          << "        print(f\"Title: {re.sub(r'<[^>]+>', '', title).strip()}\\nBody: {re.sub(r'<[^>]+>', '', snippet).strip()}\\n\")\n"
          << "except Exception as e:\n"
          << "    print(f\"Search error: {e}\")\n";
        f.close();

        system("py titan_search.py > titan_search_data.txt 2>&1");
        
        ifstream r("titan_search_data.txt");
        string l, res;
        while (getline(r, l)) res += l + "\n";
        
        cout << ">> [DEBUG SEARCH DATA]:\n" << res << endl;
        
        if (res.empty() || res.find_first_not_of(" \t\r\n") == string::npos) {
            return "Search failed or returned no results. Try a simpler, broader query without quotes.";
        }
        
        return res;
    }
    return "Unknown action type: " + act.type;
}

string build_system_prompt() {
    return R"(You are Titan V24, an autonomous AI agent integrated into a multiplayer chat network.

RULES:
1. You are an autonomous agent. If you need information or need to affect the system, use an ACTION.
2. If you use an ACTION, STOP typing immediately after it. The system will provide an OBSERVATION.
3. NEVER use CMD:curl to search the web. You MUST use the built-in SEARCH:<query> action.
4. SEARCH QUERIES MUST BE SIMPLE. Do NOT use quotation marks.
5. NEVER fabricate an OBSERVATION yourself. Wait for the system to provide it.
6. NEVER lock, shutdown, or restart the PC unless the user EXPLICITLY asks you to.
7. When using the WRITE action, you MUST wrap the file content exactly inside <<<CODE and CODE>>> tags.
8. You will receive [RECENT CHAT HISTORY IN THIS ROOM]. Use this ONLY for context to understand the user's current request. Do not reply to old messages.
9. Once you have finished your task, output your final answer to the user starting with RESULT:

FORMAT:
PLAN: [Reasoning for what to do next based on chat context]
ACTION: [ActionType]:[Target]
(Stop here and wait for system to provide OBSERVATION)
RESULT: [Your final answer to the user]

ACTIONS:
- WRITE:<filename>
<<<CODE
[Code here]
CODE>>>
- CMD:<shell command>
- LIST:<relative_path>
- READ:<filename>
- WATCH (Takes a screenshot and reads text via OCR)
- SPEAK:<text>
- SYS:LOCK
- CLICK:x,y
- TYPE:text
- SEARCH:<query>
)";
}   

string call_llm(httplib::Client &cli, const string &full_prompt) {
    json req = {
        {"model", "qwen2.5-coder:7b"}, 
        {"prompt", full_prompt},
        {"stream", false},
        {"options", {
            {"stop", {"OBSERVATION:", "OBSERVATION"}},
            {"temperature", 0.4},        
            {"repeat_penalty", 1.1},     
            {"num_predict", 1024},
            {"num_ctx", 8192} 
        }}
    };

    cout << ">> [THINKING]..." << endl;
    auto res = cli.Post("/api/generate", req.dump(), "application/json");

    if (res && res->status == 200) {
        try { return json::parse(res->body)["response"]; } 
        catch (const exception &e) { return "ERROR_INTERNAL: " + string(e.what()); }
    }
    return "ERROR_NETWORK: No response (status: " + to_string(res ? res->status : -1) + ")";
}

void NetworkListener(SOCKET titanSocket) {
    char buffer[4096];
    string tcp_buffer = "";
    
    while (titan_running) {
        memset(buffer, 0, 4096);
        int bytes = recv(titanSocket, buffer, 4096, 0);
        
        if (bytes <= 0) {
            cout << ">> [NETWORK] Disconnected from server." << endl;
            titan_running = false;
            queue_cv.notify_all(); 
            break;
        }

        tcp_buffer.append(buffer, bytes);
        
        while (true) {
            size_t end_pos = tcp_buffer.find("\n[END]");
            if (end_pos == string::npos) break; 
            
            string raw_data = tcp_buffer.substr(0, end_pos + 6);
            tcp_buffer.erase(0, end_pos + 6);

            Message msg = ParseMessage(raw_data);

            if (msg.protocol == PROTOCOL_VERSION) {
                
                if (msg.payload == PayloadType::SESSION_ACCEPT) {
                    mySessionID = msg.session_id;
                    cout << "[AUTH] Identity Verified. Session ID: " << mySessionID << "\n> " << flush;
                    continue;
                }
                else if (msg.payload == PayloadType::SESSION_REJECT) {
                    cout << "\n[REJECTED] " << msg.body << endl;
                    titan_running = false;
                    queue_cv.notify_all();
                    break;
                }
                
                if (msg.payload == PayloadType::FILE_CHUNK) {
                    log_action("IGNORE", "FILE_CHUNK ignored");
                    continue;
                }

                if (msg.payload == PayloadType::FILE_META) {
                    cout << "\n>> [SYSTEM] Ignored FILE_META from " << msg.from << endl;
                    continue; 
                }

                if (msg.payload == PayloadType::TEXT && msg.to == "ALL") {
                    append_memory("[PUBLIC] " + msg.from + ": " + msg.body);
                }
                
                else if (msg.payload == PayloadType::COMMAND && msg.to == "titan") {
                    cout << "\n>> [NETWORK] Received command from @" << msg.from << endl;
                    
                    unique_lock<mutex> lock(queue_mutex);
                    if (command_queue.size() >= 32) {
                        cout << ">> [WARNING] Command queue full. Dropped message from " << msg.from << endl;
                        lock.unlock();
                        continue; 
                    }

                    QueuedCommand qc;
                    qc.msg = msg;
                    qc.queued_time = chrono::steady_clock::now();

                    command_queue.push(qc); 
                    lock.unlock();
                    
                    queue_cv.notify_one(); 
                }
            }
        }
    }
}

int main() {
    SetConsoleOutputCP(CP_UTF8);
    
    if (!fs::exists(WORKSPACE_ROOT)) {
        fs::create_directories(WORKSPACE_ROOT);
        cout << ">> [INIT]: Created Workspace at " << WORKSPACE_ROOT << endl;
    }

    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    SOCKET titanSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1"); 
    serverAddr.sin_port = htons(8080);

    if (connect(titanSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        cout << ">> [FATAL] Cannot connect to chat server." << endl;
        WSACleanup();
        return 1;
    }

    Message join_msg;
    join_msg.protocol = PROTOCOL_VERSION;
    join_msg.payload = PayloadType::SYSTEM;
    join_msg.from = "titan";
    join_msg.to = "server";
    join_msg.session_id = 0;
    join_msg.body = "JOIN";
    
    string join_packet = SerializeMessage(join_msg);
    send(titanSocket, join_packet.c_str(), join_packet.length(), 0);

    thread listener(NetworkListener, titanSocket);

    // Block until auth finishes
    while (mySessionID == 0 && titan_running) {
        this_thread::sleep_for(chrono::milliseconds(50));
    }

    if (!titan_running) {
        closesocket(titanSocket);
        WSACleanup();
        if (listener.joinable()) listener.join();
        return 1;
    }

    httplib::Client cli("http://localhost:11434");
    cli.set_read_timeout(LLM_TIMEOUT_SEC);

    cout << "\n=== TITAN V24 NETWORK AGENT ONLINE ===\n";
    cout << ">> Architecture: Asynchronous Network Node + ReAct Loop\n";
    cout << ">> Listening for commands on network...\n";

    while (titan_running) {
        
        unique_lock<mutex> lock(queue_mutex);
        queue_cv.wait(lock, []{ return !command_queue.empty() || !titan_running; });
        
        if (!titan_running) break;

        QueuedCommand incoming_qc = command_queue.front();
        command_queue.pop();
        lock.unlock();

        auto exec_start_time = chrono::steady_clock::now();
        auto queue_wait_time = chrono::duration_cast<chrono::microseconds>(exec_start_time - incoming_qc.queued_time).count();
        perf_logger.log_metric("NETWORK_QUEUE_WAIT", queue_wait_time);

        Message incoming_msg = incoming_qc.msg;
        string user_input = incoming_msg.body;
        string sender = incoming_msg.from; 

        append_memory("USER (@" + sender + "): " + user_input);

        string session_context = build_system_prompt() + 
            "\n\nSYSTEM STATE:\nWorkspace: " + WORKSPACE_ROOT + 
            "\nBattery: " + get_battery_status() + 
            "\nRecent Memory:\n" + load_recent_memory() +
            "\nUser Request Payload:\n" + user_input;

        int agent_turn = 0;
        string final_result = "Task completed, but I didn't generate a text response."; 
        
        while (agent_turn < MAX_AGENT_TURNS) {
            auto llm_start = chrono::steady_clock::now();
            string answer = call_llm(cli, session_context);
            auto llm_end = chrono::steady_clock::now();
            perf_logger.log_metric("LLM_INFERENCE_TURN_" + to_string(agent_turn), chrono::duration_cast<chrono::microseconds>(llm_end - llm_start).count());

            if (answer.rfind("ERROR_", 0) == 0) {
                final_result = "Encountered a critical error: " + answer;
                cout << ">> [ERROR]: " << answer << endl;
                break;
            }

            cout << "\n>> [TITAN]:\n" << answer << endl;

            vector<Action> actions = parse_actions(answer);
            
            if (actions.empty()) {
                size_t result_pos = answer.find("RESULT:");
                
                if (result_pos != string::npos) {
                    final_result = answer.substr(result_pos + 7);
                    final_result.erase(0, final_result.find_first_not_of(" \t\r\n")); 
                    append_memory("TITAN: " + final_result);
                    break; 
                } else {
                    cout << ">> [SYSTEM WARNING]: Qwen dropped format. Forcing correction." << endl;
                    session_context += "\n" + answer + "\nOBSERVATION: ERROR - You must output either an ACTION: or a RESULT:. Try again.\n";
                    agent_turn++;
                    continue; 
                }
            }

            string combined_observations = "";

            for (size_t i = 0; i < actions.size(); i++) {
                cout << "\n>> Action " << (i + 1) << "/" << actions.size() << ": " << actions[i].type;
                if (!actions[i].target.empty() && actions[i].type != "WATCH") cout << " -> " << actions[i].target;
                cout << endl;

                auto act_start = chrono::steady_clock::now();
                string result = execute_action(actions[i]);
                auto act_end = chrono::steady_clock::now();
                perf_logger.log_metric("EXECUTE_" + actions[i].type, chrono::duration_cast<chrono::microseconds>(act_end - act_start).count());

                combined_observations += "\nOBSERVATION:\n" + result + "\n";
            }

            if (combined_observations.find("SUCCESS") != string::npos) {
                session_context += "\n" + answer + combined_observations + "\nAction successful. You MUST output RESULT: now to finish the task.\n";
            } else {
                session_context += "\n" + answer + combined_observations + "\nNext PLAN or RESULT:\n";
            }
            agent_turn++;
            
            if (agent_turn >= MAX_AGENT_TURNS) {
                final_result = "I reached my maximum action limit while trying to complete this task.";
                cout << "\n>> [SYSTEM]: Titan reached max recursion limit." << endl;
            }
        }

        auto exec_end_time = chrono::steady_clock::now();
        auto total_mission_time = chrono::duration_cast<chrono::microseconds>(exec_end_time - exec_start_time).count();
        perf_logger.log_metric("TOTAL_MISSION_TIME", total_mission_time);

        Message reply_msg;
        reply_msg.protocol = PROTOCOL_VERSION;
        reply_msg.payload = PayloadType::TEXT;
        reply_msg.from = "titan";
        reply_msg.to = sender;
        reply_msg.session_id = mySessionID; // Identity injected
        reply_msg.body = final_result;

        string reply_packet = SerializeMessage(reply_msg);
        send(titanSocket, reply_packet.c_str(), reply_packet.length(), 0);
        
        cout << "\n>> [NETWORK] Reply dispatched to @" << sender << ". Waiting for next command...\n";
    }

    closesocket(titanSocket);
    WSACleanup();
    if (listener.joinable()) listener.join(); 
    cout << "\n>> Shutting down." << endl;
    return 0;
}