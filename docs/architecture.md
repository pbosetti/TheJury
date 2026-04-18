# Architecture

The Jury remains split into two bootstrap components:

- a Lightroom Classic plugin that handles selection, export, HTTP orchestration, result display, and metadata persistence;
- a local C++20 service that owns request validation, provider selection, preflight output, semantic stubs, and aggregate result shaping.

## Service boundaries

The service exposes only local HTTP endpoints on `127.0.0.1:6464`.
`/health` is a lightweight liveness probe, `/v1/capabilities` reports available semantic backends, and `/v1/critique` accepts the critique payload consumed by the plugin.

## Preflight and semantic separation

The critique flow is modeled as three independent stages:

1. `StubPreflightEngine` produces deterministic technical checks.
2. A `SemanticProvider` implementation is chosen through `SemanticProviderFactory`.
3. `SimpleAggregateEngine` combines the preflight and optional semantic outputs into the final report.

This keeps semantic evaluation optional while ensuring the service can always return a valid critique payload.

## Provider abstraction

The service compiles against the `SemanticProvider` interface and ships two bootstrap implementations:

- `DisabledSemanticProvider` for deterministic local-only critiques;
- `OllamaProvider` for the planned local-first VLM backend.

`OllamaClient` is kept as a separate adapter so future HTTP integration does not leak backend-specific details into the critique pipeline.
