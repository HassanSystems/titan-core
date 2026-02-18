# Titan Core (V18) - Self-Healing C++ AI Agent

Titan is a local, C++ based AI agent that interacts with the OS through a secure sandbox.
**Current Version:** V18 (The Engineer)
**Model:** Llama 3.2 (via Ollama)

## 🧬 V18 Architecture: The "OODA" Loop
Titan V18 moves beyond simple command execution to a full **Observe-Orient-Decide-Act** loop.
It creates code, executes it, watches for crashes, and rewrites the code to fix bugs automatically.

### Key Capabilities
* **Safe Creator Mode:** Can write files (`.py`, `.cpp`, `.md`, etc.) within `C:\TitanWorkspace`.
* **Execution Engine:** Runs scripts via `_popen` to capture `stdout` and `stderr`.
* **Auto-Recovery:** If a script crashes (non-zero exit code), Titan analyzes the error trace and triggers a self-repair loop (up to 2 retries).
* **Multi-Action:** Can plan, write, and run in a single turn.

## 🛠️ Build Instructions
Requires a C++ compiler (MinGW/G++) and Windows libraries.

```bash
g++ titan_v18.cpp -o titan_v18.exe -lws2_32 -lgdi32 -std=c++17 -D_WIN32_WINNT=0x0A00