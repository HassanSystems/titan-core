# Titan Core (V20) - The Navigator (Multimodal C++ Agent)

Titan is a local, C++ based AI agent that interacts with the OS and the internet through a secure sandbox.

**Current Version:** V20 (Day 40 - Telemetry Update)
**Model:** Llama 3.2 (via Ollama)

## 🧠 V20 Architecture: Perception, Web Search & Telemetry
Titan V20 expands the agent's capabilities to the internet while maintaining a strictly controlled, hallucination-free C++ execution loop. It now includes a ground-truth telemetry engine to monitor thread health and lock contention.

### Key Capabilities
* **Async Telemetry Engine:** A custom, thread-safe background logger utilizing `std::chrono::steady_clock`. It measures microsecond execution latency across LLM inference and OS bridges without blocking the main event loop.
* **Dynamic Web Search (`SEARCH`):** The C++ engine dynamically writes and executes Python bridges to scrape DuckDuckGo for live internet data, feeding pristine UTF-8 text directly back into the LLM's memory.
* **The "Hard Stop" Protocol:** A strict C++ execution break that physically halts the agent's loop while waiting for external environment data (Vision/Search). This physically prevents autoregressive hallucination.
* **Vision Module (`WATCH`):** Uses Python (`pyautogui`) to capture the screen into the Workspace and Tesseract OCR to extract visual text directly into the agent's context.
* **Auto-Recovery:** If a generated script or command crashes, Titan reads the `stderr` trace and triggers an autonomous self-repair coding loop.

## 🛠️ Build Instructions
Requires a C++ compiler (MinGW/G++), Windows libraries, Python, Tesseract OCR, and the DuckDuckGo search library.

**1. Install Python Dependencies:**
```bash
python -m pip install duckduckgo-search pyautogui sounddevice scipy whisper
# Optional: Install matplotlib if you want to run the telemetry graphing script
python -m pip install matplotlib