# Titan Core (v8) 🏗️👁️🖥️

**Titan is a C++ based AI Agent powered by Llama 3.2.**
It is designed to be a self-extending system that can see, speak, and build its own tools.

## 🚀 Current Capabilities (Day 26)

### 1. **The Watcher (Screen Awareness)** 👁️
Titan can now see your desktop screen.
* **Mechanism:** Auto-generates a Python script using `pyautogui` to capture the screen, then feeds the image to the Vision System.
* **Usage:** Just say *"Look at my screen"* or *"WATCH"*.
* **Result:** Titan reads the active window, error messages, or websites visible on your monitor.

### 2. **Vision System** 📷
Titan can "see" image files.
* **Pipeline:** C++ -> Python (Pillow) -> Tesseract OCR -> C++ Memory.
* **Sanitization:** Automatically cleans and converts images (BMP) to bypass format errors.
* **Usage:** *"Read the text in test.png"*

### 3. **The Python Bridge (The Architect)** 🏗️
Titan can write its own Python scripts to solve problems it cannot do in C++.
* Example: *"Draw a graph of linear regression"* -> Titan writes `graph.py`, executes it, and presents the image.

### 4. **Core Systems** 🧠
* **Brain:** Llama 3.2 (via Ollama API).
* **Voice:** Windows TTS (Powershell injection).
* **Memory:** Short-term conversational context (`titan_memory.txt`).

## 🛠️ Tech Stack
* **Language:** C++ (Standard 17)
* **AI Backend:** Ollama (Llama 3.2)
* **Vision:** Tesseract OCR + Python Pillow + PyAutoGUI
* **Communication:** HTTP (REST API)

## 📦 How to Run
1.  **Install Prerequisites:**
    * Ollama (running `llama3.2`)
    * g++ Compiler
    * Tesseract OCR
    * Python 3.x
    * **Libraries:** `pip install pytesseract pillow pyautogui`
2.  **Compile:**
    ```bash
    g++ titan_v8.cpp -o titan_v8.exe -lws2_32 -std=c++17 -D_WIN32_WINNT=0x0A00
    ```
3.  **Run:**
    ```bash
    ./titan_v8.exe
    ```

---
*Built publicly by @Hassan_Builds. Day 26.*