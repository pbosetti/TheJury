# Lightroom Classic PPA Critique Plugin

Bootstrap instructions and technical specification for a **Lightroom Classic plugin** plus **C++ companion service** that evaluates selected photos against the **12 PPA merit criteria**, with a strict separation between:

1. **Technical preflight** (always local, deterministic)
2. **Semantic evaluation** (optional; local or remote provider)

This document is intended as a starting point for a VSCode + Codex project.

---

## 1. Project goals

The system should:

- operate from Lightroom Classic on one or more selected photos;
- export a normalized analysis rendition of each photo;
- run a **local technical preflight**;
- optionally run a **semantic critique** using either:
  - a **local semantic provider**, or
  - a **remote semantic provider**;
- return:
  - per-criterion scores,
  - warnings/errors,
  - aggregate classification,
  - rationale and suggestions,
  - ranking across selected images;
- write the result back into Lightroom custom metadata.

The product is a **critique assistant**, not an official PPA judging system.

---

## 2. Architectural principles

### 2.1 Separation of concerns

The architecture is split into four layers:

- **Lightroom plugin (Lua)**: orchestration, UI, export, metadata persistence
- **Companion service (C++)**: local API server and execution engine
- **Preflight engine (C++)**: deterministic technical checks
- **Semantic engine (provider abstraction)**: optional semantic scoring, local or remote

### 2.2 Local-first

Default behavior:

- images never leave the local machine unless the user explicitly enables remote semantic evaluation;
- technical preflight always stays local;
- semantic provider is pluggable;
- the default local semantic provider should be **Ollama-based**.

### 2.3 Stable contracts

The Lightroom plugin must communicate only with a stable **JSON API** exposed by the companion service.
The semantic providers must implement a common internal C++ interface.

### 2.4 Deterministic core + probabilistic optional layer

- Preflight must be reproducible and explainable.
- Semantic evaluation may be probabilistic, but must always expose confidence and disagreement.

---

## 3. High-level architecture

```text
Lightroom Classic Plugin (Lua)
    |
    |-- selection handling
    |-- export normalized JPEG for analysis
    |-- UI / user options
    |-- metadata read/write
    |-- cache key computation
    |
    '-- HTTP on localhost
            |
            v
      Companion Service (C++)
            |
            |-- Job Manager
            |-- Preflight Engine (always local)
            |-- Semantic Engine
            |      |-- Disabled provider
            |      |-- Local provider
            |      |     '-- Ollama adapter (default)
            |      '-- Remote provider
            |-- Aggregator
            |-- Cache / persistence
            '-- JSON API
```

---

## 4. Repository layout

Recommended monorepo structure:

```text
lightroom-ppa-critique/
  README.md
  LICENSE
  .gitignore
  .editorconfig
  CMakeLists.txt
  docs/
    architecture.md
    api.md
    metadata.md
    evaluation-rubric.md
    ollama-provider.md
  plugin/
    PpaCritique.lrplugin/
      Info.lua
      Init.lua
      PluginInfoProvider.lua
      ExportServiceProvider.lua
      MetadataDefinition.lua
      Manager.lua
      ApiClient.lua
      Json.lua
      Views/
        CritiqueDialog.lua
        ResultDialog.lua
      Utils/
        Paths.lua
        Hash.lua
        Logger.lua
  service/
    CMakeLists.txt
    src/
      main.cpp
      app/
        Application.cpp
        Application.hpp
        Config.cpp
        Config.hpp
      api/
        HttpServer.cpp
        HttpServer.hpp
        Routes.cpp
        Routes.hpp
        JsonSchemas.hpp
      jobs/
        JobManager.cpp
        JobManager.hpp
      core/
        PhotoContext.hpp
        AnalysisResult.hpp
        AggregateResult.hpp
      preflight/
        PreflightEngine.cpp
        PreflightEngine.hpp
        ComplianceChecks.cpp
        ComplianceChecks.hpp
        TechnicalMetrics.cpp
        TechnicalMetrics.hpp
        CompositionMetrics.cpp
        CompositionMetrics.hpp
      semantic/
        SemanticProvider.hpp
        DisabledSemanticProvider.cpp
        DisabledSemanticProvider.hpp
        LocalSemanticProvider.cpp
        LocalSemanticProvider.hpp
        OllamaProvider.cpp
        OllamaProvider.hpp
        RemoteSemanticProvider.cpp
        RemoteSemanticProvider.hpp
        VirtualJudge.cpp
        VirtualJudge.hpp
        prompts/
          mir12-system.txt
          mir12-judge-technical.txt
          mir12-judge-impact.txt
          mir12-judge-composition.txt
          mir12-judge-balanced.txt
          mir12-judge-category.txt
      aggregate/
        Aggregator.cpp
        Aggregator.hpp
      storage/
        CacheStore.cpp
        CacheStore.hpp
        AuditLog.cpp
        AuditLog.hpp
      util/
        Hash.cpp
        Hash.hpp
        Files.cpp
        Files.hpp
        Time.cpp
        Time.hpp
        Log.cpp
        Log.hpp
      external/
        OllamaClient.cpp
        OllamaClient.hpp
    include/
    tests/
      unit/
      integration/
  examples/
    sample_requests/
    sample_reports/
    ollama/
      critique-schema.json
      sample-ollama-request.json
      sample-ollama-response.json
```

---

## 5. Technology choices

### 5.1 Lightroom plugin

- Language: **Lua**
- Target: Lightroom Classic SDK
- Responsibility: orchestration only

### 5.2 Companion service

Recommended C++ stack:

- Language: **C++20**
- Build system: **CMake**
- Package manager: **vcpkg** or Conan
- JSON: `nlohmann/json`
- HTTP server: `cpp-httplib`, Crow, Pistache, or Boost.Beast
- Logging: `spdlog`
- Tests: `Catch2` or `GoogleTest`
- Hashing: SHA-256 library
- Optional image stack:
  - OpenCV for image metrics and saliency primitives
  - LittleCMS for ICC/profile inspection if needed
  - Exiv2 for metadata inspection if needed
  - libjpeg-turbo for JPEG-level handling if needed
- Persistence:
  - SQLite for cache and audit logs

Suggested balance for MVP:

- `cpp-httplib`
- `nlohmann/json`
- `spdlog`
- `SQLite3`
- `OpenCV`
- `Catch2`

### 5.3 Local semantic runtime

Recommended default:

- **Ollama** as the local model installer and serving layer
- **Qwen2.5-VL 7B** as the default local semantic judge
- **Qwen2.5-VL 3B** as the fallback for lighter machines

Design rule:

- Ollama is a **deployment/runtime backend**, not the center of the architecture.
- All business logic, rubric logic, aggregation, caching, and audit behavior remain inside the C++ companion service.

---

## 6. Component responsibilities

## 6.1 Lightroom plugin (Lua)

The plugin must:

- discover current photo selection;
- collect Lightroom-side metadata available from the catalog;
- render/export a normalized JPEG for analysis;
- build request payloads;
- submit jobs to the companion service;
- poll or await completion;
- show results in dialogs/panels;
- write back custom metadata;
- support batch ranking.

The plugin must **not** contain image-analysis logic beyond trivial validation.

### Core Lua modules

#### `Manager.lua`
Coordinates end-to-end flow.

#### `ApiClient.lua`
Handles localhost HTTP calls to the companion service.

#### `MetadataDefinition.lua`
Defines custom metadata fields visible in Lightroom.

#### `ExportServiceProvider.lua`
Creates the analysis rendition.

#### `CritiqueDialog.lua`
Prompts for category, mode, semantic provider, and output options.

#### `ResultDialog.lua`
Shows per-photo results, warnings, ranking, and suggestions.

---

## 6.2 Companion service (C++)

The service must:

- expose localhost HTTP endpoints;
- validate requests;
- manage batch jobs;
- run preflight synchronously or asynchronously;
- optionally run semantic evaluation;
- aggregate results;
- cache results by image fingerprint + settings fingerprint;
- persist audit information.

The service should support both:

- command-line foreground mode for development;
- background service mode for production.

### Internal subsystems

- **API layer**: request validation, DTOs, error mapping
- **Preflight engine**: technical and compliance analysis
- **Semantic engine**: provider selection and orchestration
- **Aggregator**: final classification and confidence synthesis
- **Storage**: cache, audit, configuration
- **External adapters**: Ollama client, remote provider client

---

## 7. Analysis pipeline

## 7.1 Stage A: technical preflight (mandatory, local)

This stage is always executed.

### Inputs

- normalized JPEG path
- photo metadata
- category
- plugin options

### Outputs

- compliance checks
- technical metrics
- composition metrics
- warnings/errors
- normalized preflight score block

### Typical checks

#### Compliance
- readable file
- JPEG format
- dimensions within configured target
- embedded ICC/profile status
- suspicious front signature/watermark heuristic
- category-specific rule flags if inferable

#### Technical metrics
- exposure balance
- clipping ratios
- sharpness/focus proxy
- noise estimate
- contrast / microcontrast estimate
- color cast / balance proxies
- edge distraction score

#### Composition proxies
- saliency map
- center-of-interest strength
- border tension / edge attraction
- balance / asymmetry proxies
- subject isolation proxy

### Important rule

This stage must return usable output even when semantic evaluation is disabled.

---

## 7.2 Stage B: semantic evaluation (optional)

This stage can be:

- disabled;
- local;
- remote.

### Inputs

- normalized JPEG
- category
- preflight output
- mode (`mir12` initially)
- optional user notes

### Outputs

- per-criterion semantic scores
- virtual judge panel outputs
- rationale
- actionable suggestions
- confidence and disagreement

### Criteria coverage

The 12 PPA criteria are modeled as rubric dimensions:

- impact
- creativity
- style
- composition
- presentation
- center_of_interest
- lighting
- subject_matter
- color_balance
- technical_excellence
- technique
- story_telling

Not all criteria are equally objective. The semantic engine should explicitly indicate confidence per criterion.

### Local semantic strategy

The local semantic strategy should use:

- **one primary VLM judge** exposed through Ollama;
- **multiple virtual judges** implemented through rubric/prompt variants;
- **strict JSON schema output**;
- **post-processing and normalization in C++**.

Default model roles:

- `qwen2.5vl:7b` -> default judge
- `qwen2.5vl:3b` -> lighter fallback

Do not depend on free-form text as the primary machine interface. The model must return schema-constrained JSON.

---

## 7.3 Stage C: aggregation

The aggregator combines preflight and semantic outputs into a final critique.

### Outputs

- final classification (for example A/B/C/D style internal mapping)
- merit probability
- overall confidence
- disagreement index
- dominant strengths
- dominant weaknesses
- ranked suggestions

### Aggregation rules

Recommended rules:

- hard compliance failures can cap the final result;
- severe technical defects can limit merit probability;
- semantic output can dominate high-level critique, but not override hard technical failures;
- disagreement across virtual judges must be surfaced.

---

## 8. Virtual judge model

The system should emulate a panel rather than a single scorer.

### Recommended initial implementation

Five virtual judges:

1. **Technical conservative**
2. **Impact/storytelling focused**
3. **Composition/style focused**
4. **Balanced generalist**
5. **Category-aware specialist**

### Design note

These do **not** need to be five different ML models.
They can be implemented as:

- one provider with five rubric variants,
- one provider with five prompts/templates,
- one provider plus five aggregation masks,
- or a mixture of local and remote sources.

### Minimum per-judge output

```json
{
  "judge_id": "J3",
  "vote": "B",
  "scores": {
    "impact": 78,
    "composition": 72,
    "story_telling": 63
  },
  "confidence": 0.66,
  "rationale": "Strong central subject and clean tonal control, but weak narrative closure."
}
```

---

## 9. Ollama integration model

Ollama should be the **default local semantic runtime**.

### 9.1 Role of Ollama

Use Ollama for:

- model installation and local lifecycle;
- local HTTP serving on the same machine;
- image + prompt inference;
- schema-constrained JSON generation.

Do not use Ollama for:

- business rules;
- aggregation logic;
- cache policy;
- metadata mapping;
- plugin/service orchestration.

### 9.2 Default model choices

Recommended defaults:

- `qwen2.5vl:7b` -> main local semantic judge
- `qwen2.5vl:3b` -> fallback for lower-memory systems

### 9.3 Ollama runtime assumptions

The C++ service should assume:

- Ollama is reachable on localhost;
- model availability can be queried;
- a model may be absent and require install;
- local semantic mode should degrade gracefully if Ollama is unavailable.

### 9.4 Provider abstraction

The C++ service must never expose Ollama-specific details to the Lightroom plugin.
The plugin only knows `semantic_mode = disabled | local | remote`.

---

## 10. API design

The plugin talks only to the companion service.

Base URL:

```text
http://127.0.0.1:48721
```

## 10.1 Health

### `GET /health`

Response:

```json
{
  "status": "ok",
  "version": "0.1.0",
  "semantic_modes": ["disabled", "local", "remote"]
}
```

## 10.2 Run critique

### `POST /v1/critique`

Request:

```json
{
  "request_id": "uuid",
  "photo": {
    "source_id": "lr-photo-id",
    "render_path": "/absolute/path/to/export.jpg",
    "fingerprint": "sha256..."
  },
  "context": {
    "category": "illustrative",
    "mode": "mir12",
    "user_notes": "optional",
    "semantic_mode": "local"
  },
  "metadata": {
    "width": 3840,
    "height": 2160,
    "icc_profile": "sRGB IEC61966-2.1",
    "keywords": ["portrait", "studio"],
    "develop": {
      "crop": true,
      "bw": false,
      "local_adjustments": true
    }
  },
  "options": {
    "run_preflight": true,
    "run_semantic": true,
    "write_audit_log": true,
    "use_cache": true
  }
}
```

Response:

```json
{
  "request_id": "uuid",
  "preflight": {
    "status": "pass_with_warnings",
    "checks": [
      {"id": "file_readable", "result": "pass"},
      {"id": "jpeg", "result": "pass"},
      {"id": "icc_profile", "result": "pass"},
      {"id": "watermark_suspect", "result": "warn"}
    ],
    "metrics": {
      "focus": 0.76,
      "noise": 0.28,
      "highlight_clipping": 0.11,
      "shadow_clipping": 0.19,
      "edge_distraction": 0.44,
      "center_of_interest": 0.80,
      "composition_balance": 0.68
    },
    "summary": "Technically solid with moderate edge distractions."
  },
  "semantic": {
    "enabled": true,
    "provider": "local",
    "model": "qwen2.5vl:7b",
    "runtime": "ollama",
    "panel": [
      {"judge_id": "J1", "vote": "B", "confidence": 0.70},
      {"judge_id": "J2", "vote": "B", "confidence": 0.62},
      {"judge_id": "J3", "vote": "C", "confidence": 0.66},
      {"judge_id": "J4", "vote": "B", "confidence": 0.64},
      {"judge_id": "J5", "vote": "B", "confidence": 0.71}
    ],
    "criteria": {
      "impact": 77,
      "creativity": 70,
      "style": 75,
      "composition": 72,
      "presentation": 66,
      "center_of_interest": 80,
      "lighting": 74,
      "subject_matter": 73,
      "color_balance": 79,
      "technical_excellence": 71,
      "technique": 69,
      "story_telling": 65
    },
    "suggestions": [
      "Reduce bright edge distractions on the right side.",
      "Tighten crop to strengthen impact.",
      "Presentation could be cleaner for competition intent."
    ]
  },
  "aggregate": {
    "classification": "B",
    "merit_probability": 0.71,
    "confidence": 0.64,
    "disagreement": 0.21,
    "strengths": ["center_of_interest", "color_balance"],
    "weaknesses": ["presentation", "story_telling"],
    "summary": "Strong subject emphasis and color control; narrative and finish are less convincing."
  }
}
```

## 10.3 Batch critique

### `POST /v1/critique/batch`

Runs the same logic on multiple photos and returns ranking.

## 10.4 Capabilities

### `GET /v1/capabilities`

Returns provider availability, model availability, runtime availability, and feature flags.

Suggested fields:

```json
{
  "semantic_modes": ["disabled", "local", "remote"],
  "local_runtime": {
    "name": "ollama",
    "available": true,
    "endpoint": "http://127.0.0.1:11434",
    "models": [
      {"name": "qwen2.5vl:7b", "installed": true},
      {"name": "qwen2.5vl:3b", "installed": false}
    ]
  }
}
```

## 10.5 Cache inspection

### `GET /v1/cache/stats`

Optional debugging endpoint.

---

## 11. Internal C++ interfaces

## 11.1 Semantic provider interface

```cpp
class SemanticProvider {
public:
    virtual ~SemanticProvider() = default;

    virtual std::string name() const = 0;
    virtual bool available() const = 0;

    virtual SemanticResult evaluate(
        const PhotoContext& context,
        const PreflightResult& preflight
    ) = 0;
};
```

Implementations:

```cpp
class DisabledSemanticProvider final : public SemanticProvider { ... };
class LocalSemanticProvider final : public SemanticProvider { ... };
class OllamaProvider final : public SemanticProvider { ... };
class RemoteSemanticProvider final : public SemanticProvider { ... };
```

## 11.2 Ollama client abstraction

```cpp
struct OllamaRequest {
    std::string model;
    std::string prompt;
    std::filesystem::path imagePath;
    nlohmann::json formatSchema;
    double temperature = 0.2;
};

struct OllamaResponse {
    bool ok = false;
    std::string model;
    std::string rawText;
    nlohmann::json parsed;
    std::string error;
};

class OllamaClient {
public:
    explicit OllamaClient(std::string baseUrl);

    bool reachable() const;
    std::vector<std::string> installedModels() const;
    bool hasModel(const std::string& modelName) const;

    OllamaResponse generateStructured(const OllamaRequest& request);
};
```

## 11.3 Preflight interface

```cpp
class PreflightEngine {
public:
    PreflightResult run(const PhotoContext& context);
};
```

## 11.4 Aggregator interface

```cpp
class Aggregator {
public:
    AggregateResult combine(
        const PreflightResult& preflight,
        const std::optional<SemanticResult>& semantic
    ) const;
};
```

---

## 12. Core data models

## 12.1 `PhotoContext`

```cpp
struct PhotoContext {
    std::string sourceId;
    std::filesystem::path renderPath;
    std::string fingerprint;
    std::string category;
    std::string mode;
    std::vector<std::string> keywords;
    int width = 0;
    int height = 0;
    std::string iccProfile;
    bool crop = false;
    bool bw = false;
    bool localAdjustments = false;
    std::string userNotes;
};
```

## 12.2 `PreflightResult`

```cpp
struct CheckResult {
    std::string id;
    std::string result;   // pass, warn, fail
    std::string message;
};

struct PreflightMetrics {
    double focus = 0.0;
    double noise = 0.0;
    double highlightClipping = 0.0;
    double shadowClipping = 0.0;
    double edgeDistraction = 0.0;
    double centerOfInterest = 0.0;
    double compositionBalance = 0.0;
};

struct PreflightResult {
    std::string status;
    std::vector<CheckResult> checks;
    PreflightMetrics metrics;
    std::string summary;
};
```

## 12.3 `SemanticResult`

```cpp
struct JudgeVote {
    std::string judgeId;
    std::string vote;
    double confidence = 0.0;
    std::map<std::string, int> scores;
    std::string rationale;
};

struct SemanticResult {
    bool enabled = false;
    std::string provider;
    std::string runtime;
    std::string model;
    std::vector<JudgeVote> panel;
    std::map<std::string, int> criteria;
    std::vector<std::string> suggestions;
};
```

## 12.4 `AggregateResult`

```cpp
struct AggregateResult {
    std::string classification;
    double meritProbability = 0.0;
    double confidence = 0.0;
    double disagreement = 0.0;
    std::vector<std::string> strengths;
    std::vector<std::string> weaknesses;
    std::string summary;
};
```

---

## 13. Lightroom custom metadata

Suggested custom metadata fields:

- `ppaCritique.status`
- `ppaCritique.category`
- `ppaCritique.mode`
- `ppaCritique.preflightStatus`
- `ppaCritique.classification`
- `ppaCritique.meritProbability`
- `ppaCritique.confidence`
- `ppaCritique.disagreement`
- `ppaCritique.impact`
- `ppaCritique.creativity`
- `ppaCritique.style`
- `ppaCritique.composition`
- `ppaCritique.presentation`
- `ppaCritique.centerOfInterest`
- `ppaCritique.lighting`
- `ppaCritique.subjectMatter`
- `ppaCritique.colorBalance`
- `ppaCritique.technicalExcellence`
- `ppaCritique.technique`
- `ppaCritique.storyTelling`
- `ppaCritique.backendUsed`
- `ppaCritique.lastAnalyzedAt`

Store large rationale text externally in cache/audit storage rather than inside metadata.

---

## 14. Cache design

Cache key should depend on:

- image fingerprint of the exported analysis rendition;
- category;
- mode;
- semantic provider;
- semantic provider version/model version;
- preflight version;
- rubric version.

Suggested cache key:

```text
sha256(render_fingerprint + category + mode + semantic_provider + semantic_version + rubric_version)
```

Use SQLite tables:

- `analysis_cache`
- `analysis_audit`
- `service_config`

---

## 15. Error handling

The service must distinguish clearly between:

- invalid request;
- image read failure;
- preflight failure;
- semantic provider unavailable;
- remote provider timeout;
- partial result available.

The plugin UI should show partial results whenever possible.

Example policy:

- if preflight succeeds and semantic fails, still show preflight report;
- if semantic provider is unavailable, downgrade to preflight-only mode;
- if cache hit exists but provider is unavailable, optionally show cached semantic result with staleness flag;
- if Ollama is installed but the configured model is missing, expose this through `/v1/capabilities` and downgrade gracefully.

---

## 16. Security and privacy

### Defaults

- bind the service only to `127.0.0.1`;
- no remote exposure by default;
- remote semantic mode must be explicit opt-in;
- log whether semantic mode was `disabled`, `local`, or `remote`.

### Optional hardening

- ephemeral local auth token between plugin and service;
- request size limits;
- sanitized path handling;
- redaction of sensitive metadata in audit logs.

---

## 17. Recommended implementation phases

## Phase 0: repository bootstrap

Deliverables:

- monorepo skeleton
- CMake bootstrap
- basic Lightroom plugin skeleton
- localhost health endpoint

## Phase 1: preflight MVP

Deliverables:

- JPEG path ingestion
- compliance checks
- basic technical metrics
- JSON results in plugin UI
- write Lightroom metadata

## Phase 2: batch and ranking

Deliverables:

- multi-photo batch critique
- ranking logic
- cache support
- result dialog with sorting/filtering

## Phase 3: semantic provider abstraction

Deliverables:

- `SemanticProvider` interface
- disabled provider
- stub local provider
- stub remote provider
- capability reporting

## Phase 4: local semantic engine with Ollama

Deliverables:

- `OllamaClient`
- `OllamaProvider`
- local judge panel scaffolding
- rubric templates
- confidence/disagreement support
- per-criterion outputs
- schema-constrained JSON parsing

## Phase 5: remote semantic engine

Deliverables:

- opt-in remote provider
- timeouts/retries
- explicit privacy UI

## Phase 6: calibration and rubric refinement

Deliverables:

- weight tuning
- virtual judge tuning
- category-aware logic
- more useful critique language

---

## 18. Development tasks for Codex

Use these tasks as initial prompts/work items.

### Task 1: bootstrap repository

Create:

- monorepo skeleton
- root `CMakeLists.txt`
- service `CMakeLists.txt`
- minimal `main.cpp`
- plugin skeleton with `Info.lua`, `Init.lua`, `PluginInfoProvider.lua`

### Task 2: implement localhost health server

Implement in C++:

- HTTP server bound to `127.0.0.1:48721`
- `GET /health`
- JSON response with version and semantic modes

### Task 3: implement `POST /v1/critique` stub

Return a synthetic JSON payload shaped exactly like the target API.

### Task 4: implement Lua `ApiClient`

Implement:

- health check
- critique request submission
- JSON response parsing
- error handling

### Task 5: implement Lightroom dialog flow

Implement:

- category selection
- semantic mode selection (`disabled`, `local`, `remote`)
- per-photo result view

### Task 6: implement metadata definitions

Create custom metadata fields and write back aggregate results.

### Task 7: implement preflight engine MVP

Implement:

- file readability
- JPEG validation
- dimensions
- ICC/profile presence
- basic clipping metrics
- basic focus proxy

### Task 8: implement cache layer

Implement SQLite-backed cache for analysis results.

### Task 9: implement semantic provider interface and Ollama adapter

Implement:

- `SemanticProvider`
- `DisabledSemanticProvider`
- `LocalSemanticProvider`
- `OllamaClient`
- `OllamaProvider`
- stub `RemoteSemanticProvider`
- `/v1/capabilities` with runtime/model inspection

### Task 10: implement batch ranking

Sort selected images by:

1. classification
2. merit probability
3. confidence
4. lower disagreement preferred

### Task 11: implement structured Ollama request/response path

Implement:

- critique JSON schema file
- prompt templates for each virtual judge
- request builder for image + prompt + schema
- response normalization into `SemanticResult`
- fallback handling for invalid model output

---

## 19. Coding guidelines

### C++

- prefer RAII;
- keep API DTOs separate from internal domain objects where helpful;
- isolate third-party dependencies behind small wrappers;
- keep image-analysis functions pure where possible;
- use `std::optional`, `std::variant`, and strong enums where appropriate;
- avoid pushing business rules into route handlers.

### Lua

- keep plugin modules small;
- isolate Lightroom SDK calls from business orchestration;
- centralize API calls in one module;
- centralize metadata field names in one module.

---

## 20. Testing strategy

### Unit tests

C++:

- request validation
- aggregation rules
- cache key generation
- preflight metric functions
- provider selection logic
- Ollama request builder
- Ollama response normalization

Lua:

- payload construction
- metadata mapping
- response parsing

### Integration tests

- plugin -> localhost health
- plugin -> critique stub
- C++ API -> JSON schema conformance
- cache hit/miss behavior
- C++ service -> Ollama health/model discovery

### Golden tests

Maintain a small set of reference images and expected JSON outputs for regression checks.

---

## 21. Suggested first milestone

A good first milestone is:

- Lightroom command on selected photos
- local export of analysis JPEG
- `POST /v1/critique` to localhost
- synthetic preflight result from C++ service
- result dialog in Lightroom
- metadata write-back

Do **not** start with real semantic scoring.
Start with the full pipeline and stable contracts first.

After that, the second milestone should be:

- Ollama runtime detection
- `/v1/capabilities`
- local provider selection
- stubbed `OllamaProvider` returning normalized synthetic semantic output

Only then move to real local semantic prompting.

---

## 22. Prompt template for VSCode / Codex

Use the following as a starting instruction inside Codex:

```text
Build a monorepo for a Lightroom Classic plugin and a C++20 companion service.

Requirements:
- The Lightroom Classic plugin must be written in Lua and act only as an orchestrator.
- The companion service must be written in C++20 with CMake.
- Expose an HTTP API on localhost:48721.
- Implement GET /health, GET /v1/capabilities, and POST /v1/critique.
- POST /v1/critique should initially return a stub JSON payload matching the documented schema.
- The local semantic runtime must be designed around Ollama, but hidden behind a provider abstraction.
- Add an OllamaClient and an OllamaProvider.
- Model defaults: qwen2.5vl:7b primary, qwen2.5vl:3b fallback.
- Add a clean repository layout with plugin/, service/, docs/, examples/.
- Add spdlog, nlohmann/json, and Catch2.
- Write clean, minimal, compilable code with comments only where useful.
- Prefer simple, maintainable code over framework-heavy designs.
- Include a README with build and run instructions.
```

---

## 23. Immediate next implementation step

Start by implementing, in this order:

1. repository skeleton;
2. C++ localhost health server;
3. `GET /v1/capabilities` with static stub runtime info;
4. stub critique endpoint;
5. Lua API client;
6. Lightroom dialog and metadata write-back;
7. preflight MVP;
8. Ollama client wrapper;
9. Ollama-backed local provider.

This order stabilizes the interfaces before any complex scoring logic is introduced.
