# Architecture

Milestone 1 establishes the local companion service as the system boundary for future Lightroom integration.

- Lightroom will remain an orchestration-only plugin.
- The C++ service owns the HTTP API and critique payload schema.
- Preflight and semantic execution are still stubbed, but the route contract already reflects the planned split.
- Provider capability reporting includes `disabled` and `ollama` to preserve the intended local-first architecture.
