# Titan V12: The Hardware-Integrated AI Agent 🛡️🔋

**Titan is a C++ based AI Agent powered by Llama 3.2.**
Unlike standard chatbots, Titan has "Hands" (mouse/keyboard), "Ears" (Whisper), "Eyes" (Tesseract OCR), and a **Safety Core** that prevents dangerous actions.

## 🚀 New in V12 (The Safety Update)

### 1. **The Safety Core (Firewall)** 🛡️
Titan now includes a middleware layer that filters every AI command before execution.
* **Blacklist:** Automatically blocks dangerous commands like `shutdown`, `rm`, `del`, or `format`.
* **Bounds Checking:** Prevents mouse clicks outside the visible screen area.
* **Audit Logging:** Every physical action is recorded in `titan_actions.log` for transparency.

### 2. **Hardware Control (The Body)** 🔋
Titan interacts directly with the PC hardware via Windows API.
* **Battery Awareness:** "What is my battery level?" -> Reads system power status.
* **Volume Control:** "Mute volume" / "Volume Up" -> Controls system audio.
* **Security:** "Lock the PC" -> Instantly locks the workstation.

### 3. **Robust Architecture** 🏗️
* **Crash Prevention:** Added `try/catch` blocks to handle AI hallucinations without crashing.
* **Unicode Support:** Fixed typing engine to support symbols and capitalization.

## 🛠️ Tech Stack
* **Core:** C++ (Standard 17)
* **AI Backend:** Ollama (Llama 3.2)
* **Hearing:** Python + OpenAI Whisper (Base Model)
* **Vision:** Tesseract OCR + Python
* **OS Integration:** Win32 API (User32, Kernel32, PowerProf)

## 📦 How to Run

### 1. Prerequisites
* [Ollama](https://ollama.ai/) (Model: `llama3.2`)
* G++ Compiler (MinGW)
* Python 3.x (`pip install pyautogui sounddevice scipy openai-whisper`)
* Tesseract OCR (Default Path: `C:\Program Files\Tesseract-OCR`)

### 2. Compile
```bash
g++ titan_v12.cpp -o titan_v12.exe -lws2_32 -std=c++17 -D_WIN32_WINNT=0x0A00