# Titan Core (v7) 🏗️👁️

**Titan is a C++ based AI Agent powered by Llama 3.2.**
It is designed to be a self-extending system that can see, speak, and build its own tools.

## 🚀 Current Capabilities (Day 25)

### 1. **Vision System (New)** 👁️
Titan can now "see" images.
* **Pipeline:** C++ -> Python (Pillow) -> Tesseract OCR -> C++ Memory.
* **Sanitization:** Automatically cleans and converts images (BMP) to bypass format errors.
* **Usage:** Just say *"Read the text in test.png"*

### 2. **The Python Bridge (The Architect)** 🏗️
Titan can write its own Python scripts to solve problems it cannot do in C++.
* Example: *"Draw a graph of linear regression"* -> Titan writes `graph.py`, executes it, and presents the image.

### 3. **Core Systems** 🧠
* **Brain:** Llama 3.2 (via Ollama API).
* **Voice:** Windows TTS (Powershell injection).
* **Memory:** Short-term conversational context (`titan_memory.txt`).
* **Filesystem:** Can read/write files and execute system commands.

## 🛠️ Tech Stack
* **Language:** C++ (Standard 17)
* **AI Backend:** Ollama (Llama 3.2)
* **Vision:** Tesseract OCR + Python Pillow
* **Communication:** HTTP (REST API)

## 📦 How to Run
1.  **Install Prerequisites:**
    * Ollama (running `llama3.2`)
    * g++ Compiler
    * Tesseract OCR
    * Python 3.x (`pip install pytesseract pillow`)
2.  **Compile:**
    ```bash
    g++ titan_v7.cpp -o titan_v7.exe -lws2_32 -std=c++17 -D_WIN32_WINNT=0x0A00
    ```
3.  **Run:**
    ```bash
    ./titan_v7.exe
    ```

---
*Built publicly by @Hassan_Builds. Day 25.*