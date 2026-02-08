#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <ctime>
#include <windows.h>
#include "httplib.h"
#include "json.hpp"

using namespace std;
using json = nlohmann::json;

const string MEMORY_FILE = "titan_memory.txt";
const string PYTHON_OUTPUT = "titan_vision_data.txt";

// --- 1. THE RETINA (Auto-Generated Screenshot Tool) ---
void capture_screen() {
    cout << ">> [TITAN]: SNAPSHOT. Capturing visual data..." << endl;
    
    // Titan writes its own Python tool to take a screenshot
    string python_code = 
        "import pyautogui\n"
        "import os\n"
        "try:\n"
        "   screenshot = pyautogui.screenshot()\n"
        "   screenshot.save('screen_memory.png')\n"
        "   print('SUCCESS')\n"
        "except Exception as e:\n"
        "   print('FAILED: ' + str(e))\n";

    ofstream file("titan_capture.py");
    file << python_code;
    file.close();

    system("python titan_capture.py");
}

// --- 2. THE VISION BRIDGE (Fixed: Safe Decoding) ---
string run_python_vision(string image_path) {
    if (image_path == "screen_memory.png") {
        cout << ">> [TITAN]: Analyzing Screen Memory..." << endl;
    } else {
        cout << ">> [TITAN]: Analyzing File [" << image_path << "]..." << endl;
    }
    
    string clean_path = "";
    for (char c : image_path) { if (isalnum(c) || c == '.' || c == '_') clean_path += c; }

    string python_code = 
        "import os\n"
        "import subprocess\n"
        "from PIL import Image\n" 
        "\n"
        "tesseract_cmd = r'C:\\Program Files\\Tesseract-OCR\\tesseract.exe'\n"
        "original_image = r'" + clean_path + "'\n"
        "temp_image = 'titan_temp.bmp'\n"
        "\n"
        "try:\n"
        "    if not os.path.exists(original_image):\n"
        "        if os.path.exists('screen_memory.png'): original_image = 'screen_memory.png'\n"
        "        else: raise Exception('File not found')\n"
        "\n"
        "    img = Image.open(original_image)\n"
        "    img.save(temp_image, 'BMP')\n"
        "\n"
        "    # FIX: Capture RAW BYTES (text=False) to prevent crashing on weird characters\n"
        "    result = subprocess.run([tesseract_cmd, temp_image, 'stdout'], capture_output=True)\n"
        "    \n"
        "    # SAFELY DECODE: Ignore errors if Windows can't read a specific character\n"
        "    final_text = ''\n"
        "    if result.stdout:\n"
        "        final_text = result.stdout.decode('utf-8', errors='ignore').strip()\n"
        "    \n"
        "    if not final_text: final_text = 'Image captured. No text found.'\n"
        "    \n"
        "    with open('" + PYTHON_OUTPUT + "', 'w', encoding='utf-8') as f: f.write(final_text)\n"
        "    if os.path.exists(temp_image): os.remove(temp_image)\n"
        "\n"
        "except Exception as e:\n"
        "    with open('" + PYTHON_OUTPUT + "', 'w') as f: f.write('Error: ' + str(e))\n";

    ofstream file("titan_vision.py");
    file << python_code;
    file.close();

    system("python titan_vision.py");

    ifstream result_file(PYTHON_OUTPUT);
    string result, line;
    if (result_file.is_open()) {
        while (getline(result_file, line)) result += line + " ";
        result_file.close();
        return result;
    }
    return "Error: Vision Output Failed.";
}

// --- 3. PYTHON SCRIPTING ---
void run_python_script(string code) {
    while (code.length() > 0 && (code[0] == ' ' || code[0] == '\n')) code = code.substr(1);
    ofstream file("titan_script.py");
    file << code;
    file.close();
    cout << ">> [TITAN]: Executing Python Logic..." << endl;
    system("python titan_script.py");
}

// --- 4. HELPER SYSTEMS ---
string get_time() {
    time_t now = time(0);
    tm *ltm = localtime(&now);
    char buffer[80];
    strftime(buffer, 80, "%I:%M %p", ltm);
    return string(buffer);
}

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

// --- 5. MAIN ENGINE ---
int main() {
    httplib::Client cli("http://localhost:11434");

    cout << ">> [TITAN V8] Vision & Retina Systems Online." << endl;
    speak("Titan V8 online. I can see your screen.");

    while(true) {
        cout << "\n>> Commander: ";
        string user_input;
        getline(cin, user_input); 
        
        if (user_input == "exit") break;
        save_memory("USER", user_input);

        // --- PROMPT ENGINEERING ---
        string system_instruction = 
            "Context: You are Titan V8. You can SEE files and the USER'S SCREEN.\n"
            "Recent Chat:\n" + load_recent_memory() + 
            "\nTOOLS:\n"
            "1. To look at the screen, reply ONLY: WATCH\n"
            "2. To read a specific file, reply ONLY: VISION:filename.png\n"
            "3. To run python, reply ONLY: PYTHON: <code>\n"
            "4. Reply normally otherwise.\n";
        
        json request_body = {
            {"model", "llama3.2"},
            {"prompt", system_instruction + "\nUser: " + user_input}, 
            {"stream", false} 
        };

        auto res = cli.Post("/api/generate", request_body.dump(), "application/json");

        if (res && res->status == 200) {
            json response = json::parse(res->body);
            string answer = response["response"];
            
            // --- PARSER ---
            if (answer.find("WATCH") != string::npos) {
                // 1. Take Screenshot
                capture_screen(); 
                // 2. Read Screenshot
                string seen_text = run_python_vision("screen_memory.png");
                
                cout << ">> [TITAN SEES]: " << seen_text.substr(0, 100) << "..." << endl; 
                speak("I have captured your screen.");
                save_memory("TITAN (SEES)", seen_text);
            }
            else if (answer.find("VISION:") != string::npos) {
                string filename = answer.substr(answer.find("VISION:") + 7);
                while (filename.length() > 0 && (filename[0] == ' ' || filename[0] == '\n')) filename = filename.substr(1);
                while (filename.length() > 0 && (filename.back() == ' ' || filename.back() == '\n')) filename.pop_back();

                string seen_text = run_python_vision(filename);
                cout << ">> [VISION DATA]: " << seen_text << endl;
                speak("I have analyzed the image.");
                save_memory("TITAN (VISION)", seen_text);
            }
            else if (answer.find("PYTHON:") != string::npos) {
                run_python_script(answer.substr(answer.find("PYTHON:") + 7));
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