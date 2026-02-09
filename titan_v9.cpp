#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <ctime>
#include <windows.h> // REQUIRED FOR MOUSE/KEYBOARD CONTROL
#include "httplib.h"
#include "json.hpp"

using namespace std;
using json = nlohmann::json;

const string MEMORY_FILE = "titan_memory.txt";
const string PYTHON_OUTPUT = "titan_vision_data.txt";

// =============================================================
//  MODULE 1: THE HANDS (Motor Control System) 🖐️
// =============================================================

// 1. Move Mouse & Click
void click_mouse(int x, int y) {
    // Move
    SetCursorPos(x, y);
    Sleep(100); // Wait for move

    // Click
    INPUT inputs[2] = {};
    
    inputs[0].type = INPUT_MOUSE;
    inputs[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;

    inputs[1].type = INPUT_MOUSE;
    inputs[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;

    SendInput(2, inputs, sizeof(INPUT));
    cout << ">> [HANDS]: Clicked at (" << x << ", " << y << ")" << endl;
}

// 2. Type Text (Keyboard Injection)
void type_text(string text) {
    cout << ">> [HANDS]: Typing..." << endl;
    for (char c : text) {
        INPUT inputs[2] = {};
        SHORT vk = VkKeyScan(c); // Get keycode for character

        // Press
        inputs[0].type = INPUT_KEYBOARD;
        inputs[0].ki.wVk = LOBYTE(vk);

        // Release
        inputs[1].type = INPUT_KEYBOARD;
        inputs[1].ki.wVk = LOBYTE(vk);
        inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;

        SendInput(2, inputs, sizeof(INPUT));
        Sleep(50); // Human-like typing speed
    }
}

// =============================================================
//  MODULE 2: THE RETINA (Screen Capture) 👁️
// =============================================================
void capture_screen() {
    cout << ">> [TITAN]: SNAPSHOT. Capturing visual data..." << endl;
    string python_code = 
        "import pyautogui\n"
        "try:\n"
        "   screenshot = pyautogui.screenshot()\n"
        "   screenshot.save('screen_memory.png')\n"
        "   print('SUCCESS')\n"
        "except Exception as e:\n"
        "   print('FAILED')\n";
    ofstream file("titan_capture.py");
    file << python_code;
    file.close();
    system("python titan_capture.py");
}

// =============================================================
//  MODULE 3: VISION BRIDGE (OCR) 📷
// =============================================================
string run_python_vision(string image_path) {
    if (image_path == "screen_memory.png") cout << ">> [TITAN]: Analyzing Screen..." << endl;
    
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
        "    result = subprocess.run([tesseract_cmd, temp_image, 'stdout'], capture_output=True)\n"
        "    final_text = ''\n"
        "    if result.stdout:\n"
        "        final_text = result.stdout.decode('utf-8', errors='ignore').strip()\n"
        "    if not final_text: final_text = 'No text found.'\n"
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
    return "Error.";
}

// =============================================================
//  MODULE 4: UTILS (Memory & Voice)
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
    for (char c : text) { if (c == '\'') safe_text += "''"; else safe_text += c; }
    string command = "powershell -Command \"Add-Type -AssemblyName System.Speech; (New-Object System.Speech.Synthesis.SpeechSynthesizer).Speak('" + safe_text + "');\"";
    system(command.c_str());
}

// =============================================================
//  MAIN ENGINE (Brain) 🧠
// =============================================================
int main() {
    httplib::Client cli("http://localhost:11434");

    cout << ">> [TITAN V9] Motor Control Systems Online." << endl;
    speak("Titan V9 online. I have hands.");

    while(true) {
        cout << "\n>> Commander: ";
        string user_input;
        getline(cin, user_input); 
        
        if (user_input == "exit") break;
        save_memory("USER", user_input);

        // --- PROMPT ENGINEERING (Updated with Hands) ---
        string system_instruction = 
            "Context: You are Titan V9. You can SEE the screen and CONTROL the MOUSE/KEYBOARD.\n"
            "Recent Chat:\n" + load_recent_memory() + 
            "\nTOOLS:\n"
            "1. To type text, reply ONLY: TYPE:text_here\n"
            "2. To click screen, reply ONLY: CLICK:x,y (Example: CLICK:500,500)\n"
            "3. To look at screen, reply ONLY: WATCH\n"
            "4. To open app, reply ONLY: CMD:command\n"
            "5. Otherwise, reply normally.\n";
        
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
            if (answer.find("TYPE:") != string::npos) {
                string text = answer.substr(answer.find("TYPE:") + 5);
                speak("Typing.");
                type_text(text);
            }
            else if (answer.find("CLICK:") != string::npos) {
                // Parse X,Y
                string coords = answer.substr(answer.find("CLICK:") + 6);
                int comma = coords.find(",");
                if (comma != string::npos) {
                    int x = stoi(coords.substr(0, comma));
                    int y = stoi(coords.substr(comma + 1));
                    speak("Clicking.");
                    click_mouse(x, y);
                }
            }
            else if (answer.find("WATCH") != string::npos) {
                capture_screen(); 
                string seen_text = run_python_vision("screen_memory.png");
                cout << ">> [TITAN SEES]: " << seen_text.substr(0, 100) << "..." << endl; 
                speak("I see your screen.");
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