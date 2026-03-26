# Titan Core
A high-performance C++ messaging engine bridging P2P sockets and strict memory-bounded LLM reasoning. Designed to scale into a decentralized Telegram/Discord alternative.

## 🧠 AI Engine Integration
Titan Core is designed as an LLM-agnostic framework, transitioning to a **Claude-first cloud orchestration model**. 

**Current Local Sandbox (Fallback Mode):**
For local offline testing, the agent hooks into Ollama via internal API bridging.
1. Install Ollama and start the server (`ollama serve`).
2. Pull the baseline sandbox model: `ollama pull qwen2.5-coder:7b`.

**Phase 3 Target (Claude API):**
The primary architecture is being optimized for Anthropic's Claude to handle asynchronous socket routing and multi-agent memory validation.

## 🛠️ How to Build (Cross-Platform CMake)
Titan Core uses CMake to ensure cross-platform compatibility across Windows, Linux, and macOS.

```bash
# 1. Clone the repository
git clone [https://github.com/HassanSystems/titan-core.git](https://github.com/HassanSystems/titan-core.git)
cd titan-core

# 2. Generate build files
mkdir build && cd build
cmake ..

# 3. Compile the binaries
cmake --build .
```
(Executables will be generated in the build/ directory: TitanServer, TitanClient, and TitanAgent).

## 🏗️ Architecture
**Local-First P2P**: Asynchronous C++ sockets for zero-latency terminal messaging.

**Hybrid AI Core**: Integrated LLM execution with strict context-bounding and pre-execution firewall validation.

## 🗺️ Roadmap
- **Phase 1**: Core Socket Messaging & Local Agent Execution
- **Phase 2**: OpenSSL Integration for E2EE
- **Phase 3**: Multi-Agent Cloud Orchestration (Claude API)
- **Phase 4**: Decentralized Server/Guild Architecture