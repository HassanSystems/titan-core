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

// --- 1. THE VISION BRIDGE (The Sanitizer) ---
string run_python_vision(string image_path) {
    cout << ">> [TITAN]: Activating Vision Modules..." << endl;
    
    // CLEANER: Remove any non-alphanumeric chars from filename (security)
    string clean_path = "";
    for (char c : image_path) {
        if (isalnum(c) || c == '.') clean_path += c;
    }

    // THE MAGIC SCRIPT
    // 1. Opens the image with Pillow (which we know works).
    // 2. Saves it as a temporary .bmp (Tesseract LOVES .bmp).
    // 3. Runs Tesseract on the clean .bmp file.
    string python_code = 
        "import os\n"
        "import subprocess\n"
        "from PIL import Image\n" // Use Pillow to clean the file
        "\n"
        "tesseract_cmd = r'C:\\Program Files\\Tesseract-OCR\\tesseract.exe'\n"
        "original_image = r'" + clean_path + "'\n"
        "temp_image = 'titan_temp.bmp'\n"
        "\n"
        "try:\n"
        "    # STEP 1: Find the file (Case insensitive check)\n"
        "    if not os.path.exists(original_image):\n"
        "        if os.path.exists('test.PNG'): original_image = 'test.PNG'\n"
        "        elif os.path.exists('test.png'): original_image = 'test.png'\n"
        "        else:\n"
        "            raise Exception('File not found')\n"
        "\n"
        "    # STEP 2: Sanitize (Convert to BMP)\n"
        "    img = Image.open(original_image)\n"
        "    img.save(temp_image, 'BMP')\n"
        "\n"
        "    # STEP 3: Run Tesseract on the clean BMP\n"
        "    # We use subprocess to call the exe directly\n"
        "    result = subprocess.run([tesseract_cmd, temp_image, 'stdout'], capture_output=True, text=True)\n"
        "\n"
        "    # STEP 4: Cleanup and Save\n"
        "    final_text = result.stdout.strip()\n"
        "    if not final_text: final_text = 'Image read, but no text found.'\n"
        "    \n"
        "    with open('" + PYTHON_OUTPUT + "', 'w', encoding='utf-8') as f: f.write(final_text)\n"
        "    if os.path.exists(temp_image): os.remove(temp_image)\n"
        "\n"
        "except Exception as e:\n"
        "    with open('" + PYTHON_OUTPUT + "', 'w') as f: f.write('Error: ' + str(e))\n";

    // Save script
    ofstream file("titan_vision.py");
    file << python_code;
    file.close();

    // Execute
    system("python titan_vision.py");

    // Read Result back into C++
    ifstream result_file(PYTHON_OUTPUT);
    string result, line;
    if (result_file.is_open()) {
        while (getline(result_file, line)) result += line + " ";
        result_file.close();
        return result;
    }
    return "Error: Vision Output Failed.";
}

// --- 2. PYTHON SCRIPTING ---
void run_python_script(string code) {
    while (code.length() > 0 && (code[0] == ' ' || code[0] == '\n')) {
        code = code.substr(1);
    }
    ofstream file("titan_script.py");
    file << code;
    file.close();
    cout << ">> [TITAN]: Executing Python Logic..." << endl;
    system("python titan_script.py");
}

// --- 3. HELPER SYSTEMS ---
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

// --- 4. MAIN ENGINE ---
int main() {
    httplib::Client cli("http://localhost:11434");

    cout << ">> [TITAN V7] Vision Systems Online." << endl;
    speak("Titan V7 online. Vision ready.");

    while(true) {
        cout << "\n>> Commander: ";
        string user_input;
        getline(cin, user_input); 
        
        if (user_input == "exit") break;
        save_memory("USER", user_input);

        // --- PROMPT ENGINEERING ---
        string system_instruction = 
            "Context: You are Titan. You can SEE images using Python.\n"
            "Recent Chat:\n" + load_recent_memory() + 
            "\nTOOLS:\n"
            "1. To read an image, reply ONLY: VISION:filename.png\n"
            "2. To run python, reply ONLY: PYTHON: <code>\n"
            "3. Reply normally otherwise.\n";
        
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
            if (answer.find("VISION:") != string::npos) {
                // Extract filename
                string filename = answer.substr(answer.find("VISION:") + 7);
                
                // CLEANER: Remove spaces/newlines
                while (filename.length() > 0 && (filename[0] == ' ' || filename[0] == '\n' || filename[0] == '\r')) {
                    filename = filename.substr(1);
                }
                while (filename.length() > 0 && (filename.back() == ' ' || filename.back() == '\n' || filename.back() == '\r')) {
                    filename.pop_back();
                }

                cout << ">> [TITAN]: Analyzing image: [" << filename << "]..." << endl;
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