# Titan Core (V24)

Titan is a local-first, networked AI execution engine that converts natural language commands into deterministic system actions using a guarded ReAct loop.

## What Titan Is
Titan is an autonomous C++ agent that:
- Receives commands over TCP
- Queues and schedules execution asynchronously
- Uses a local LLM for planning
- Enforces strict action and output contracts at the engine layer

## What Titan Is NOT
- Not a chatbot
- Not prompt-based automation
- Not sandboxed
- Not production-secure
- Not multi-agent (yet)

## High-Level Architecture
Human Client
→ TCP Server
→ Titan Network Listener
→ Command Queue
→ ReAct Loop
→ Tool Execution
→ Observation Injection
→ Final Result

## Core Guarantees
- Exactly one action per LLM turn
- No fabricated observations
- No busy-waiting (0% CPU idle)
- Explicit tool invocation only
- Human confirmation for dangerous actions

## Tools
- SEARCH: DuckDuckGo HTML scraping
- READ / WRITE: Workspace-limited filesystem access
- CMD: Restricted shell execution
- WATCH: Screenshot + OCR
- CLICK / TYPE: OS input simulation
- SPEAK: Text-to-speech
- SYS: Lock workstation (explicit approval)

## Current Status
Titan V24 is a research system focused on:
- LLM output enforcement
- Deterministic autonomy
- Local-first execution

This is NOT production hardened.