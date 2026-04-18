# TheJury

Bootstrap monorepo for a Lightroom Classic PPA critique workflow.

## Current status

This repository currently contains milestone 1 of the bootstrap plan:

- C++20 companion service built with CMake
- local HTTP endpoints on `127.0.0.1:6464`
- stub critique request/response models
- basic tests for JSON parsing and HTTP routes

## Repository layout

- `service/` - C++20 localhost companion service
- `docs/` - milestone notes and API summary
- `examples/` - sample critique payloads
- `plugin/` - reserved for later Lightroom plugin milestones

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

Then query:

- `GET http://127.0.0.1:6464/health`
- `GET http://127.0.0.1:6464/v1/capabilities`
- `POST http://127.0.0.1:6464/v1/critique`
