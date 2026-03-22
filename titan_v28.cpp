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
#include <unordered_map>

#include "sqlite3.h"
#include "httplib.h"
#include "json.hpp"
#include "protocol.h" 

using namespace std;
using json = nlohmann::json;
namespace fs = std::filesystem;

const string WORKSPACE_ROOT = "C:\\TitanWorkspace";
const string ACTION_LOG_FILE = "titan_actions.log";
const string SECURITY_LOG_FILE = "titan_security.log";
const string VOICE_INPUT_FILE = "titan_voice_input.txt";
const string TESSERACT_PATH = R"(C:\Program Files\Tesseract-OCR\tesseract.exe)";

const size_t MAX_READ_SIZE = 10000; 
const size_t MAX_INPUT_CHARS = 500;  
const size_t MAX_MEMORY_CHARS = 4000; 
const int LLM_TIMEOUT_SEC = 180;    
const int MAX_AGENT_TURNS = 15; 
const int COMMAND_COOLDOWN_SEC = 15;

const vector<string> ALLOWED_EXTENSIONS = {
    ".txt", ".md", ".cpp", ".hpp", ".h", ".py", ".json", ".log",
    ".js", ".html", ".css", ".bat", ".ps1", ".java", ".cs", ".xml",
    ".yaml", ".yml", ".ini", ".cfg", ".sh", ".rb", ".go"
};

unordered_map<string, chrono::steady_clock::time_point> user_cooldowns;
sqlite3* db_connection = nullptr;

struct QueuedCommand {
    Message msg;
    chrono::steady_clock::time_point queued_time;
};

queue<QueuedCommand> command_queue; 
mutex queue_mutex;
condition_variable queue_cv;
atomic<bool> titan_running{true};
uint64_t mySessionID = 0;

string GenerateMessageID() {
    return to_string(time(0)) + "-" + to_string(rand() % 100000);
}

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

void log_security_violation(const string &violation_type, const string &details) {
    ofstream f(SECURITY_LOG_FILE, ios::app);
    time_t now = time(0);
    string time_str = ctime(&now);
    if (!time_str.empty()) time_str.pop_back();
    if (f.is_open()) f << "[" << time_str << "] [VIOLATION:" << violation_type << "] " << details << endl;
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
    string safe_entry = entry;
    if (safe_entry.length() > MAX_INPUT_CHARS) {
        safe_entry = safe_entry.substr(0, MAX_INPUT_CHARS) + "... [TRUNCATED]";
    }
    const char* sql = "INSERT INTO Memory (entry) VALUES (?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_connection, sql, -1, &stmt, 0) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, safe_entry.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

void save_trait(const string& key, const string& val) {
    const char* sql = "INSERT OR REPLACE INTO UserTraits (trait_key, trait_val) VALUES (?, ?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_connection, sql, -1, &stmt, 0) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, val.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

string load_traits() {
    string sql = "SELECT trait_key, trait_val FROM UserTraits;";
    sqlite3_stmt* stmt;
    string result = "--- PERMANENT USER TRAITS ---\n";
    bool has_traits = false;
    
    if (sqlite3_prepare_v2(db_connection, sql.c_str(), -1, &stmt, 0) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            has_traits = true;
            result += "- " + string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0))) + ": " + string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1))) + "\n";
        }
        sqlite3_finalize(stmt);
    }
    return has_traits ? result + "-----------------------------\n" : "";
}

string load_recent_memory(int limit = 15) {
    string sql = "SELECT timestamp, entry FROM Memory ORDER BY id DESC LIMIT " + to_string(limit) + ";";
    sqlite3_stmt* stmt;
    vector<string> lines;
    
    if (sqlite3_prepare_v2(db_connection, sql.c_str(), -1, &stmt, 0) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* ts = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            const char* txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            if (ts && txt) lines.push_back(string("[") + ts + "] " + txt);
        }
        sqlite3_finalize(stmt);
    }

    reverse(lines.begin(), lines.end());
    string result = "--- RECENT MEMORY ---\n";
    for (const auto& l : lines) result += l + "\n";
    return result + "---------------------\n";
}

void run_reflection() {
    cout << "\n>> [BACKGROUND] Running Memory Compression..." << endl;
    httplib::Client cli("http://localhost:11434");
    cli.set_read_timeout(LLM_TIMEOUT_SEC);

    string recent = load_recent_memory(20);
    string prompt = "Analyze these chat logs. Extract permanent facts about the user (name, OS, goals, preferences). Do NOT make things up. Output ONLY strict JSON: {\"traits\": [{\"key\": \"FactName\", \"val\": \"Details\"}]}.\nLogs:\n" + recent;

    json req = {{"model", "qwen2.5-coder:7b"}, {"prompt", prompt}, {"stream", false}, {"format", "json"}};
    auto res = cli.Post("/api/generate", req.dump(), "application/json");

    if (res && res->status == 200) {
        try {
            string output = json::parse(res->body)["response"];
            json parsed = json::parse(output);
            if (parsed.contains("traits") && parsed["traits"].is_array()) {
                for (auto& t : parsed["traits"]) {
                    if (t.contains("key") && t.contains("val")) {
                        save_trait(t["key"], t["val"]);
                        cout << ">> [TRAIT SAVED] " << t["key"] << ": " << t["val"] << endl;
                    }
                }
            }
        } catch (...) { cout << ">> [BACKGROUND] Reflection JSON Parse Failed." << endl; }
    }
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

struct AgentResponse {
    string plan;
    vector<Action> actions;
    string result;
    string error;
};

AgentResponse parse_agent_response(const string &response) {
    AgentResponse ar;
    try {
        size_t start = response.find('{');
        size_t end = response.find_last_of('}');
        if (start == string::npos || end == string::npos || start > end) {
            ar.error = "FATAL: Output is not a JSON object.";
            return ar;
        }
        
        string clean_json = response.substr(start, end - start + 1);
        json j = json::parse(clean_json);
        
        if (!j.contains("api_version") || j["api_version"] != "v1") {
            ar.error = "Missing or invalid api_version. Must be 'v1'.";
            return ar;
        }

        if (j.contains("plan") && j["plan"].is_string()) ar.plan = j["plan"];
        if (j.contains("result") && j["result"].is_string()) ar.result = j["result"];
        
        if (j.contains("actions") && j["actions"].is_array()) {
            for (const auto& item : j["actions"]) {
                Action act;
                if (item.contains("type") && item["type"].is_string()) act.type = item["type"];
                if (item.contains("target") && item["target"].is_string()) act.target = item["target"];
                if (item.contains("content") && item["content"].is_string()) act.content = item["content"];
                ar.actions.push_back(act);
            }
        }
    } catch (const exception &e) {
        ar.error = string("JSON Parse Exception: ") + e.what();
    }
    return ar;
}

string validate_action(const Action &act) {
    if (act.type == "WRITE" && (act.target.empty() || act.content.empty())) return "WRITE requires both 'target' and 'content'.";
    if ((act.type == "CMD" || act.type == "READ" || act.type == "TYPE" || act.type == "SEARCH" || act.type == "SYS" || act.type == "SPEAK") && act.target.empty()) return act.type + " requires a 'target'.";
    if (act.type == "CLICK" && act.target.find(',') == string::npos) return "CLICK requires coordinates in 'x,y' format.";
    if (act.type != "WRITE" && act.type != "CMD" && act.type != "LIST" && act.type != "READ" && act.type != "TYPE" && act.type != "CLICK" && act.type != "WATCH" && act.type != "SYS" && act.type != "SEARCH" && act.type != "SPEAK") return "Unknown action type: " + act.type;
    return "";
}

bool is_valid_result(const string &result, string &reason) {
    if (result.find("{\"") != string::npos || result.find("\"}") != string::npos) { reason = "Leaked JSON format"; return false; }
    if (result.find("<<<CODE") != string::npos) { reason = "Leaked internal code block tags"; return false; }
    if (result.find("CMD:") != string::npos || result.find("WRITE:") != string::npos || result.find("SEARCH:") != string::npos) { reason = "Leaked raw action commands"; return false; }
    return true;
}

string execute_action(const Action &act) {
    if (act.type == "WRITE") {
        return write_file(act.target, act.content) ? "SUCCESS: Wrote " + act.target : "FAILED: Could not write " + act.target;
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
        
        if (res.empty() || res.find_first_not_of(" \t\r\n") == string::npos) {
            return "Search failed or returned no results. Try a simpler, broader query without quotes.";
        }
        
        return res;
    }
    return "Unknown action type: " + act.type;
}

string build_system_prompt() {
    return R"(You are Titan V28, an autonomous AI agent integrated into a multiplayer chat network.

RULES:
1. You MUST respond ONLY with a perfectly formatted JSON object. Do not wrap the JSON in markdown code blocks.
2. Your JSON MUST strictly follow this exact schema:
{
    "api_version": "v1",
    "plan": "[Your reasoning based on chat context]",
    "actions": [
        {"type": "[ActionType]", "target": "[Target]", "content": "[Content if needed]"}
    ],
    "result": "[Final answer to the user. Leave empty if you are taking actions first.]"
}
3. Allowed ActionTypes: WRITE, CMD, LIST, READ, WATCH, SPEAK, SYS, CLICK, TYPE, SEARCH.
4. For the WRITE action, put the file path in "target" and the file contents directly in "content".
5. For the SEARCH action, put the query in "target" (NO quotes).
6. NEVER fabricate an OBSERVATION. Output your actions, and wait for the system to reply with the results.
7. If you have finished the task, populate the "result" string and leave "actions" empty.
)";
}   

string call_llm(httplib::Client &cli, const string &full_prompt) {
    json req = {
        {"model", "qwen2.5-coder:7b"}, 
        {"prompt", full_prompt},
        {"stream", false},
        {"format", "json"}, 
        {"options", {
            {"temperature", 0.1},        
            {"repeat_penalty", 1.1},     
            {"num_predict", 1024},
            {"num_ctx", 4096}
        }}
    };

    cout << ">> [THINKING in JSON]..." << endl;
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
                
                if (msg.payload == PayloadType::FILE_CHUNK) continue;
                if (msg.payload == PayloadType::FILE_META) continue; 

                if (msg.payload == PayloadType::TEXT && msg.to == "ALL") {
                    append_memory("[PUBLIC] " + msg.from + ": " + msg.body);
                }
                
                else if (msg.payload == PayloadType::COMMAND && msg.to == "titan") {
                    
                    // ---> ARQ FIX: SEND ACK IMMEDIATELY TO STOP CLIENT RETRIES <---
                    Message ackMsg;
                    ackMsg.protocol = PROTOCOL_VERSION;
                    ackMsg.message_id = GenerateMessageID();
                    ackMsg.payload = PayloadType::MESSAGE_ACK;
                    ackMsg.from = "titan";
                    ackMsg.to = msg.from;
                    ackMsg.session_id = mySessionID;
                    MsgAck m_ack;
                    m_ack.message_id = msg.message_id; 
                    ackMsg.body = SerializeMsgAck(m_ack);
                    
                    string ack_packet = SerializeMessage(ackMsg);
                    send(titanSocket, ack_packet.c_str(), ack_packet.length(), 0);

                    auto now = chrono::steady_clock::now();
                    if (user_cooldowns.count(msg.from)) {
                        auto seconds_passed = chrono::duration_cast<chrono::seconds>(now - user_cooldowns[msg.from]).count();
                        if (seconds_passed < COMMAND_COOLDOWN_SEC) {
                            cout << "\n>> [NETWORK] Spam detected. Instantly dropped command from @" << msg.from << endl;
                            string warn = "SYSTEM: Command rejected. Please wait " + to_string(COMMAND_COOLDOWN_SEC - seconds_passed) + "s before sending another.";
                            
                            Message reply_msg;
                            reply_msg.protocol = PROTOCOL_VERSION;
                            reply_msg.message_id = GenerateMessageID();
                            reply_msg.payload = PayloadType::TEXT;
                            reply_msg.from = "titan";
                            reply_msg.to = msg.from;
                            reply_msg.session_id = mySessionID; 
                            reply_msg.body = warn;
                            string reply_packet = SerializeMessage(reply_msg);
                            send(titanSocket, reply_packet.c_str(), reply_packet.length(), 0);
                            continue; 
                        }
                    }
                    user_cooldowns[msg.from] = now;

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
    srand(static_cast<unsigned int>(time(0))); 
    SetConsoleOutputCP(CP_UTF8);
    
    if (sqlite3_open("titan.db", &db_connection)) {
        cout << ">> [FATAL] Cannot open SQLite database: " << sqlite3_errmsg(db_connection) << endl;
        return 1;
    }

    const char* sql_mem = "CREATE TABLE IF NOT EXISTS Memory (id INTEGER PRIMARY KEY AUTOINCREMENT, timestamp DATETIME DEFAULT CURRENT_TIMESTAMP, entry TEXT);";
    sqlite3_exec(db_connection, sql_mem, 0, 0, 0);

    const char* sql_traits = "CREATE TABLE IF NOT EXISTS UserTraits (trait_key TEXT PRIMARY KEY, trait_val TEXT);";
    sqlite3_exec(db_connection, sql_traits, 0, 0, 0);

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
        sqlite3_close(db_connection);
        return 1;
    }

    Message join_msg;
    join_msg.protocol = PROTOCOL_VERSION;
    join_msg.message_id = GenerateMessageID();
    join_msg.payload = PayloadType::SYSTEM;
    join_msg.from = "titan";
    join_msg.to = "server";
    join_msg.session_id = 0;
    join_msg.body = "JOIN";
    
    string join_packet = SerializeMessage(join_msg);
    send(titanSocket, join_packet.c_str(), join_packet.length(), 0);

    thread listener(NetworkListener, titanSocket);

    while (mySessionID == 0 && titan_running) {
        this_thread::sleep_for(chrono::milliseconds(50));
    }

    if (!titan_running) {
        closesocket(titanSocket);
        WSACleanup();
        if (listener.joinable()) listener.join();
        sqlite3_close(db_connection);
        return 1;
    }

    httplib::Client cli("http://localhost:11434");
    cli.set_read_timeout(LLM_TIMEOUT_SEC);

    cout << "\n=== TITAN V28 NETWORK AGENT ONLINE ===\n";
    cout << ">> Architecture: Cognition Layer & Reflection Engine\n";
    cout << ">> Listening for commands on network...\n";

    int interaction_count = 0;

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
            "\n\n" + load_traits() +
            "\n" + load_recent_memory() +
            "\nUser Request Payload:\n" + user_input;

        int agent_turn = 0;
        string final_result = "Task completed, but I didn't generate a final text response."; 
        
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

            cout << "\n>> [TITAN RAW JSON]:\n" << answer << endl;

            AgentResponse ar = parse_agent_response(answer);

            if (!ar.error.empty()) {
                cout << ">> [SYSTEM WARNING]: Schema Validation Failed. Forcing correction.\n>> Reason: " << ar.error << endl;
                session_context += "\n" + answer + "\nOBSERVATION: ERROR - Schema validation failed. " + ar.error + "\nYou MUST output strictly valid JSON conforming to the schema.\n";
                agent_turn++;
                continue; 
            }

            if (!ar.result.empty()) {
                string reject_reason;
                if (!is_valid_result(ar.result, reject_reason)) {
                    log_security_violation("RESULT_FORMAT", "Reason: " + reject_reason + " | Result: " + ar.result);
                    cout << ">> [SECURITY] Blocked invalid result format: " << reject_reason << endl;
                    session_context += "\n" + answer + "\nOBSERVATION: ERROR - Your 'result' field violated safety rules: " + reject_reason + ". Do not leak JSON, system tags, or raw tool calls to the user. Rewrite the result naturally.\n";
                    agent_turn++;
                    continue;
                }

                final_result = ar.result;
                append_memory("TITAN: " + final_result);
                break; 
            }

            string combined_observations = "";

            for (size_t i = 0; i < ar.actions.size(); i++) {
                Action &act = ar.actions[i];
                cout << "\n>> Action " << (i + 1) << "/" << ar.actions.size() << ": " << act.type;
                if (!act.target.empty() && act.type != "WATCH") cout << " -> " << act.target;
                cout << endl;

                string validation_err = validate_action(act);
                if (!validation_err.empty()) {
                    log_security_violation("MALFORMED_ACTION", validation_err + " | Action: " + act.type);
                    combined_observations += "\nOBSERVATION for " + act.type + ":\nERROR: " + validation_err + "\n";
                    continue; 
                }

                auto act_start = chrono::steady_clock::now();
                string result = execute_action(act);
                auto act_end = chrono::steady_clock::now();
                perf_logger.log_metric("EXECUTE_" + act.type, chrono::duration_cast<chrono::microseconds>(act_end - act_start).count());

                combined_observations += "\nOBSERVATION for " + act.type + ":\n" + result + "\n";
            }

            string safe_answer = answer;
            if (safe_answer.length() > 1500) safe_answer = safe_answer.substr(0, 1500) + "\n... [JSON TRUNCATED TO SAVE CONTEXT]";
            session_context += "\n" + safe_answer + "\n" + combined_observations + "\nProvide next PLAN/ACTION or final RESULT.\n";
            
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
        reply_msg.message_id = GenerateMessageID();
        reply_msg.payload = PayloadType::TEXT;
        reply_msg.from = "titan";
        reply_msg.to = sender;
        reply_msg.session_id = mySessionID; 
        reply_msg.body = final_result;

        string reply_packet = SerializeMessage(reply_msg);
        send(titanSocket, reply_packet.c_str(), reply_packet.length(), 0);
        
        cout << "\n>> [NETWORK] Reply dispatched to @" << sender << ". Waiting for next command...\n";

        interaction_count++;
        if (interaction_count % 5 == 0) {
            thread reflection_thread(run_reflection);
            reflection_thread.detach();
        }
    }

    closesocket(titanSocket);
    WSACleanup();
    if (listener.joinable()) listener.join(); 
    sqlite3_close(db_connection);
    cout << "\n>> Shutting down." << endl;
    return 0;
}