# Titan Core (V19) - The Observer (Multimodal C++ Agent)

Titan is a local, C++ based AI agent that interacts with the OS through a secure sandbox.
**Current Version:** V19 (The Observer)
**Model:** Llama 3.2 (via Ollama)

## 👁️ V19 Architecture: Multimodal Perception
Titan V19 introduces **Computer Vision**. It is no longer restricted to text input; it can actively observe the user's screen to gather context, read error dialogs, and analyze UI elements.

### Key Capabilities
* **Vision Module (`WATCH`):** Uses Python (`pyautogui`) to capture the screen and Tesseract OCR to extract visual text directly into the LLM's memory bank.
* **Auto-Recovery:** If a generated script crashes, Titan reads the error trace and triggers a self-repair loop.
* **Execution Engine:** Runs scripts via `_popen` to capture `stdout` and `stderr`.
* **Safe Creator Mode:** Sandboxed environment for writing and testing code safely.

## 🛠️ Build Instructions
Requires a C++ compiler (MinGW/G++), Windows libraries, Python (pyautogui), and Tesseract OCR.

```bash
g++ titan_v19.cpp -o titan_v19.exe -lws2_32 -lgdi32 -std=c++17 -D_WIN32_WINNT=0x0A00