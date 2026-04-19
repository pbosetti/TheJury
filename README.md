# TheJury

Bootstrap monorepo for a Lightroom Classic PPA critique workflow.

## Current status

This repository now contains a working bootstrap across the first planned vertical-slice milestones:

- C++20 companion service built with CMake
- local HTTP endpoints on `127.0.0.1:6464`
- TOML-backed service configuration loaded next to the executable
- real Ollama-backed semantic provider with fallback model support
- Lightroom plugin skeleton for menu wiring, export orchestration, service calls, result display, and metadata persistence
- basic tests for JSON parsing and HTTP routes

## Repository layout

- `service/` - C++20 localhost companion service
- `plugin/` - Lightroom Classic plugin bundle skeleton
- `docs/` - bootstrap notes for architecture, API, and Lightroom metadata
- `examples/` - sample critique payloads

## Build the service

```bash
cmake -S /home/runner/work/TheJury/TheJury -B /home/runner/work/TheJury/TheJury/build
cmake --build /home/runner/work/TheJury/TheJury/build
ctest --test-dir /home/runner/work/TheJury/TheJury/build --output-on-failure
```

## Run the service

```bash
/home/runner/work/TheJury/TheJury/build/service/ppa_service
```

The build copies `ppa_service.toml.example` to `build/service/ppa_service.toml`. Edit that file to point at your local Ollama runtime if needed.

Then query:

- `GET http://127.0.0.1:6464/health`
- `GET http://127.0.0.1:6464/v1/capabilities`
- `POST http://127.0.0.1:6464/v1/critique`

## Lightroom plugin bootstrap

The plugin bundle lives in `plugin/PpaCritique.lrplugin` and currently focuses on a single-photo critique flow:

1. register the `PPA Critique...` menu command;
2. optionally register `PPA Critique with Semantic Analysis...` for the Ollama-backed path;
3. export a temporary JPEG rendition for the selected photo;
4. submit the critique request to the local service;
5. show the returned summary; and
6. persist key critique fields into custom Lightroom metadata.

This remains an early prototype intended for incremental implementation, not a finished product.
