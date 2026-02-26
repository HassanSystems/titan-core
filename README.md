Titan Core (V23) — Asynchronous Networked AI Agent

Titan is a local, C++-based autonomous AI agent that executes system actions, performs web search, and communicates over a custom TCP/IP network.
It uses a guarded ReAct-style control loop driven by a locally hosted LLM.

Version: V23 (Day 44)
Model Backend: Qwen 2.5 Coder (7B) via Ollama
Platform: Windows (Winsock2, GDI32)

Overview

Titan V23 transitions the project from a single-threaded local loop to an asynchronous, network-connected agent.

The agent operates as a TCP client node:

Incoming commands are received over the network

Commands are queued using a producer–consumer model

Execution occurs only when work is available (idle at 0% CPU otherwise)

The system is designed to explore robust LLM control, not prompt-only autonomy.

Architecture
Asynchronous Command Pipeline

Network Thread:
Listens for TCP packets and enqueues commands

Execution Thread:
Blocks on std::condition_variable until work is available

Queue:
Thread-safe command queue (std::mutex, std::queue)

This prevents busy-waiting and allows deterministic scheduling.

Guarded ReAct Control Loop

Titan enforces strict output contracts at the C++ engine layer.

Format Trapdoor

The agent must emit exactly one action per turn

If the LLM:

Emits action + result together

Breaks syntax

Hallucinates observations
→ the engine rejects the output and injects a corrective system error

The model is forced to self-correct before execution continues.

Dynamic Stop Sequences

The inference backend is configured to hard-stop generation when the model:

Commits to a tool call

Fabricates an observation or result

This prevents multi-action hallucinations and runaway outputs.

Telemetry

A thread-safe background logger records:

NETWORK_QUEUE_WAIT — time spent waiting in the TCP queue

LLM_INFERENCE_TURN — per-turn inference latency (microseconds)

TOTAL_MISSION_TIME — end-to-end execution time

Tools & Capabilities
Web Search (SEARCH)

Executes DuckDuckGo queries via a Python bridge

Results are returned into the agent context

Includes C++ fallback logic if no results are found

Note: Execution is local and trusted. This is not sandboxed.

Screen Text Ingestion (WATCH)

Captures the screen using pyautogui

Extracts visible text using Tesseract OCR

Injects OCR output into the agent context

This is OCR-based text ingestion, not multimodal vision reasoning.

Build Requirements
System

Windows

MinGW / G++

Python 3.x

Tesseract OCR

Ollama

Python Dependencies
py -m pip install duckduckgo-search pyautogui sounddevice scipy whisper
Model Backend
ollama run qwen2.5-coder:7b
Compilation
# Network Server
g++ server.cpp -o server.exe -lws2_32 -std=c++17

# Human Client
g++ client.cpp -o client.exe -lws2_32 -std=c++17

# Titan Agent
g++ titan_v23.cpp -o titan_v23.exe -lws2_32 -lgdi32 -std=c++17 -D_WIN32_WINNT=0x0A00
Execution

Run each component in a separate terminal:

Start Server

server.exe

Boot Titan

titan_v23.exe

(The agent connects and idles.)

Start Human Client

client.exe
Example Command
@titan Search the web for the current price of Bitcoin and report the result.
Project Status

Titan is an experimental research system focused on:

LLM output enforcement

Deterministic agent execution

Local-first autonomy

This is not production-hardened, sandboxed, or secure against untrusted input.