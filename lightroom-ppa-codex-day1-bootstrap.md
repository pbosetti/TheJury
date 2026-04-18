# Lightroom Classic PPA Critique Plugin - Day 1 Bootstrap for VSCode + Codex

## Purpose

Use this document as the initial bootstrap instruction for Codex in VSCode.
The project is a monorepo for a Lightroom Classic plugin and a local companion service in C++20.
The system analyzes one or more selected photographs and evaluates them against the PPA 12 Elements of a Merit Image.

The architecture is deliberately split into two stages:

1. **Preflight engine**: always local, deterministic, non-generative.
2. **Semantic engine**: optional, local-first, with Ollama as the default local runtime.

The Lightroom plugin is only an orchestrator. It must not contain image-analysis logic beyond coordination, UI, metadata, and export.

---

## Product goals

Build a first working vertical slice with these capabilities:

- Lightroom Classic plugin command visible in the UI.
- Export of a selected photo to a temporary JPEG rendition for analysis.
- Local C++ companion service exposing HTTP endpoints on localhost.
- Health check and capabilities endpoints.
- Stub critique endpoint returning valid structured JSON.
- Lightroom plugin calling the service and showing the returned report.
- Persistence of basic critique fields into custom Lightroom metadata.
- Ollama provider defined in the C++ architecture, even if initially mocked.

Do **not** attempt to implement full PPA scoring logic on day 1.
Do **not** attempt to build a polished UI on day 1.
Do **not** couple the plugin directly to Ollama.

---

## Non-negotiable architectural constraints

1. The plugin and the service must be separate components.
2. The preflight stage must be independent from the semantic stage.
3. The semantic stage must be behind a provider abstraction.
4. Ollama is the default local semantic backend, but it is not the architectural center.
5. The service owns all business logic, schema validation, aggregation, and normalization.
6. The plugin only orchestrates export, request submission, result display, and metadata persistence.
7. The project must be buildable with CMake.
8. Keep the HTTP API local-only in the first iteration.

---

## Repository layout to generate

Create a monorepo with this initial structure:

```text
lightroom-ppa-critique/
  README.md
  .gitignore
  CMakeLists.txt
  cmake/
  docs/
    architecture.md
    api.md
    metadata.md
  plugin/
    PpaCritique.lrplugin/
      Info.lua
      PluginInit.lua
      CritiqueMenu.lua
      ServiceClient.lua
      MetadataDefinition.lua
      ResultDialog.lua
      Utils.lua
  service/
    CMakeLists.txt
    include/
      ppa/
        api/
        core/
        model/
        preflight/
        semantic/
        aggregate/
        storage/
    src/
      main.cpp
      api/
      core/
      preflight/
      semantic/
      aggregate/
      storage/
    tests/
  examples/
    request.sample.json
    response.sample.json
```

---

## Technology choices

### Lightroom plugin

- Language: Lua
- Target: Lightroom Classic SDK
- Responsibilities:
  - register menu command
  - inspect current selection
  - export selected photo to temp JPEG
  - call local service over HTTP
  - show returned report
  - persist selected fields into custom metadata

### Companion service

- Language: C++20
- Build system: CMake
- HTTP library: choose one lightweight library and keep the adapter isolated
- JSON library: choose one common modern library and use it consistently
- Test framework: add one lightweight unit-test framework
- Logging: simple structured logging
- Storage: design for SQLite later, but no DB required for day 1

### Local semantic runtime

- Default runtime: Ollama
- Default local model target: `qwen2.5vl:7b`
- Light fallback target: `qwen2.5vl:3b`
- Access pattern: localhost HTTP from C++ companion service

---

## First implementation targets

### Milestone 1 - skeleton and build

Generate:

- root `CMakeLists.txt`
- service `CMakeLists.txt`
- compilable C++ service with `main.cpp`
- a simple HTTP server on `127.0.0.1:6464`
- endpoint `GET /health`
- endpoint `GET /v1/capabilities`
- endpoint `POST /v1/critique`

Acceptance criteria:

- project configures and builds successfully
- service starts locally
- `/health` returns JSON `{ "status": "ok" }`
- `/v1/capabilities` returns a JSON object describing available providers
- `/v1/critique` accepts a valid JSON payload and returns a stub response

### Milestone 2 - plugin wiring

Generate:

- Lightroom plugin bundle with menu entry `PPA Critique...`
- service client in Lua for local HTTP calls
- stub dialog or log output of the returned critique
- metadata definition file with a minimal custom schema

Acceptance criteria:

- plugin loads in Lightroom Classic
- menu command is visible
- selecting one photo and running the command triggers a local service call
- the service response is visible to the user in some form

### Milestone 3 - export pipeline

Generate:

- export selected photo to temporary JPEG for analysis
- package a request JSON with file path and metadata
- submit the request to `/v1/critique`

Acceptance criteria:

- temporary JPEG is generated
- valid request reaches the service
- service returns structured critique JSON

### Milestone 4 - provider abstraction

Generate:

- `SemanticProvider` interface
- `DisabledSemanticProvider`
- `OllamaProvider` stub
- `SemanticProviderFactory`

Acceptance criteria:

- critique flow compiles against the abstraction
- provider selection can be configured
- Ollama provider is present even if initially mocked

---

## HTTP API contract - initial version

### GET /health

Response:

```json
{
  "status": "ok"
}
```

### GET /v1/capabilities

Response example:

```json
{
  "service": "ppa-companion",
  "version": "0.1.0",
  "semantic": {
    "enabled": true,
    "default_provider": "ollama",
    "providers": [
      {
        "name": "disabled",
        "available": true
      },
      {
        "name": "ollama",
        "available": false,
        "models": ["qwen2.5vl:7b", "qwen2.5vl:3b"]
      }
    ]
  }
}
```

### POST /v1/critique

Request example:

```json
{
  "image": {
    "path": "/tmp/ppa-critique/photo-001.jpg"
  },
  "photo": {
    "id": "lr-photo-001",
    "file_name": "photo-001.jpg"
  },
  "category": "illustrative",
  "mode": "mir12",
  "options": {
    "run_preflight": true,
    "run_semantic": false,
    "semantic_provider": "disabled"
  },
  "metadata": {
    "width": 3840,
    "height": 2160,
    "icc_profile": "sRGB",
    "keywords": ["portrait"]
  }
}
```

Response example:

```json
{
  "request_id": "stub-0001",
  "runtime": {
    "semantic_provider": "disabled",
    "model": ""
  },
  "preflight": {
    "status": "pass",
    "checks": [
      {
        "id": "dimensions",
        "result": "pass",
        "message": "stub"
      }
    ],
    "technical_scores": {
      "technical_excellence": 0.0,
      "color_balance": 0.0,
      "lighting": 0.0,
      "composition": 0.0
    }
  },
  "semantic": null,
  "aggregate": {
    "classification": "C",
    "merit_probability": 0.0,
    "confidence": 0.0,
    "summary": "stub critique response"
  }
}
```

---

## Domain model to generate in C++

Create initial C++ structs or classes for these concepts:

- `CritiqueRequest`
- `CritiqueResponse`
- `ImageInput`
- `PhotoInfo`
- `CritiqueOptions`
- `PreflightReport`
- `PreflightCheck`
- `TechnicalScores`
- `SemanticResult`
- `JudgeVote`
- `AggregateResult`
- `RuntimeInfo`

Requirements:

- keep them serializable to/from JSON
- keep them small and explicit
- avoid premature inheritance except for provider abstractions

---

## Required provider interfaces

Generate these interfaces:

```cpp
class PreflightEngine {
public:
    virtual ~PreflightEngine() = default;
    virtual PreflightReport run(const CritiqueRequest& request) = 0;
};

class SemanticProvider {
public:
    virtual ~SemanticProvider() = default;
    virtual SemanticResult evaluate(
        const CritiqueRequest& request,
        const PreflightReport& preflight) = 0;
};

class AggregateEngine {
public:
    virtual ~AggregateEngine() = default;
    virtual AggregateResult combine(
        const PreflightReport& preflight,
        const std::optional<SemanticResult>& semantic) = 0;
};
```

Then generate concrete initial implementations:

- `StubPreflightEngine`
- `DisabledSemanticProvider`
- `OllamaProvider`
- `SimpleAggregateEngine`

---

## Ollama integration design

Do not tightly bind business logic to Ollama payloads.
Generate an adapter such as:

- `OllamaClient`
- `OllamaProvider`

Responsibilities:

### OllamaClient

- low-level HTTP calls to local Ollama API
- timeouts and basic error handling
- model availability check
- optional structured output support

### OllamaProvider

- transform internal semantic request into Ollama prompt + image payload
- call `OllamaClient`
- normalize the response into `SemanticResult`
- isolate all Ollama-specific details

Day 1 implementation target:

- provider exists and compiles
- capability probe implemented or stubbed cleanly
- real semantic generation can remain TODO if necessary

---

## Lightroom metadata fields to define

Generate a minimal custom metadata schema with these fields:

- `ppaCritiqueStatus`
- `ppaCritiqueCategory`
- `ppaCritiqueClassification`
- `ppaCritiqueMeritProbability`
- `ppaCritiqueConfidence`
- `ppaCritiqueLastAnalyzedAt`
- `ppaCritiqueSemanticProvider`
- `ppaCritiqueModel`

Requirements:

- fields must be writable by the plugin
- values should be updated after each critique run

---

## Lightroom plugin behavior to generate first

The plugin should initially support only **single-photo critique**, even if the long-term product supports multiple images.

Generate this flow:

1. user selects one photo
2. user launches `PPA Critique...`
3. plugin exports a temporary JPEG rendition
4. plugin builds request JSON
5. plugin calls local service
6. plugin receives critique JSON
7. plugin shows a minimal result dialog or log summary
8. plugin writes selected result fields into custom metadata

Do not build batch orchestration yet.

---

## Coding standards

### C++

- use C++20
- prefer clear interfaces over deep class hierarchies
- keep headers minimal
- avoid macros except where required
- use RAII
- keep transport, domain, and provider logic separate
- keep stub implementations simple and testable

### Lua

- keep modules small
- isolate HTTP code in one file
- isolate Lightroom SDK calls behind helper functions where reasonable
- avoid embedding business logic in UI code

---

## Testing expectations

Generate at least these tests on day 1:

### Service tests

- request JSON can be parsed into `CritiqueRequest`
- stub aggregate result serializes correctly
- `/health` returns expected payload
- `/v1/capabilities` returns a provider list

### Plugin checks

- no formal unit tests required on day 1
- but code structure should make request construction and response handling easy to test later

---

## Documentation to generate

Create these docs during bootstrap:

### `README.md`

Include:

- project overview
- repository structure
- build instructions for the service
- how the Lightroom plugin fits in
- status as early prototype

### `docs/architecture.md`

Include:

- plugin/service separation
- preflight vs semantic separation
- provider abstraction
- Ollama as default local runtime backend

### `docs/api.md`

Include:

- endpoint definitions
- request/response examples
- error handling conventions

### `docs/metadata.md`

Include:

- custom Lightroom metadata fields
- intended meanings
- update lifecycle

---

## Things Codex should avoid doing

- Do not invent unsupported Lightroom SDK APIs.
- Do not hardwire the plugin directly to Ollama.
- Do not implement full image-scoring heuristics yet.
- Do not add speculative ML dependencies unless they are explicitly isolated and optional.
- Do not over-engineer authentication for localhost-only endpoints.
- Do not try to solve cross-platform packaging on day 1.

---

## Final output expected from Codex in the first pass

Generate a compilable initial monorepo with:

- service skeleton in C++20
- local HTTP endpoints
- domain model and provider abstractions
- stub Ollama integration layer
- Lightroom plugin skeleton in Lua
- metadata definition file
- minimal docs
- example request/response JSON files

The result should be a clean bootstrap suitable for incremental implementation, not a fake complete product.

---

## Suggested prompt to give Codex

Use the following instruction as the first task:

```text
Create a monorepo named lightroom-ppa-critique for a Lightroom Classic plugin and a local C++20 companion service.

Constraints:
- Lightroom plugin in Lua under plugin/PpaCritique.lrplugin
- local service in C++20 under service/
- CMake build
- service listens on 127.0.0.1:6464
- endpoints: GET /health, GET /v1/capabilities, POST /v1/critique
- clear separation between preflight and semantic stages
- semantic stage behind a SemanticProvider abstraction
- include DisabledSemanticProvider and OllamaProvider stubs
- define serializable domain models for critique request/response
- plugin provides a menu command, exports one selected image to temp JPEG, calls the service, and writes basic result fields into custom metadata
- generate README.md and docs/architecture.md docs
- keep all implementations minimal but compilable
- do not over-engineer

Produce all source files with sensible contents and comments where needed.
```

Then, after the first pass, use this second task:

```text
Refine the generated bootstrap to improve code quality, tighten CMake targets, add basic tests for JSON serialization and HTTP health/capabilities handlers, and make sure all stub classes compile cleanly with C++20.
```

