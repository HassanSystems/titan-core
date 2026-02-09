# Titan Core (v9) 🏗️👁️🖐️

**Titan is a C++ based AI Agent powered by Llama 3.2.**
It is designed to be a self-extending system that can see, speak, touch, and build its own tools.

## 🚀 Current Capabilities (Day 27)

### 1. **The Hands (Motor Control)** 🖐️
Titan can physically control the Mouse and Keyboard using the Win32 API.
* **Mechanism:** Direct Hardware Injection (`SendInput`).
* **Usage:** *"Type 'Hello World'"* or *"Click 500, 500"*.
* **Result:** Titan takes control of the cursor and types ghost text into any active window (Notepad, VS Code, Browser).

### 2. **The Watcher (Screen Awareness)** 👁️
Titan can see your desktop screen.
* **Mechanism:** Auto-generates a Python script to capture the screen -> OCR -> Text Analysis.
* **Usage:** *"Look at my screen"*.

### 3. **The Python Bridge (The Architect)** 🏗️
Titan can write its own Python scripts to solve complex problems.
* **Usage:** *"Draw a graph of linear regression"*.

### 4. **Core Systems** 🧠
* **Brain:** Llama 3.2 (via Ollama API).
* **Voice:** Windows TTS (Powershell injection).
* **Memory:** Short-term conversational context (`titan_memory.txt`).

## 🛠️ Tech Stack
* **Language:** C++ (Standard 17)
* **AI Backend:** Ollama (Llama 3.2)
* **OS Integration:** Win32 API (User32, Kernel32)
* **Vision:** Tesseract OCR + Python Pillow
* **Communication:** HTTP (REST API)

## 📦 How to Run
1.  **Install Prerequisites:**
    * Ollama (running `llama3.2`)
    * g++ Compiler
    * Python 3.x (`pip install pyautogui pillow pytesseract`)
2.  **Compile:**
    ```bash
    g++ titan_v9.cpp -o titan_v9.exe -lws2_32 -std=c++17 -D_WIN32_WINNT=0x0A00
    ```
3.  **Run:**
    ```bash
    ./titan_v9.exe
    ```

---
*Built publicly by @Hassan_Builds. Day 27.*