# Titan Core: A High-Performance Hybrid Messaging Protocol.

## Vision
A decentralized, local-first C++ engine bridging P2P sockets and strict memory-bounded LLM reasoning, designed to scale into a full Telegram/Discord alternative.

## How to Build
```bash
mkdir build
cd build
cmake ..
make
```

## Architecture & Core Guarantees
- TCP Server Flow: Asynchronous and strict message routing through the decentralized P2P TCP network.
- Context-Bounding Rules: Memory-bounded LLM reasoning ensuring localized state limits natively without buffer overflows.

## Roadmap (What's Next)
- **Phase 1**: Core Socket Messaging & Local Agent Execution (Current).
- **Phase 2**: OpenSSL Integration for End-to-End Encryption (E2EE).
- **Phase 3**: Multi-Agent Cloud Orchestration via Claude API integration.
- **Phase 4**: Decentralized Server/Guild Architecture (Platform Scaling).