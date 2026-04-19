# Install

These instructions cover the current bootstrap:

- the local C++ service in `service/`
- the Lightroom Classic plugin in `plugin/PpaCritique.lrplugin`

The service listens on `127.0.0.1:6464`. The plugin talks to that local endpoint.

## Common prerequisites

- `git`
- CMake 3.21 or newer
- Ninja
- a C++20-capable compiler
- an internet connection for the first CMake configure step

The first configure downloads third-party dependencies with `FetchContent`:

- `cpp-httplib`
- `nlohmann/json`
- `Catch2`
- `toml++`

Lightroom-specific prerequisites:

- Adobe Lightroom Classic 13.0 or newer
- one photo selected when running the plugin command

## macOS

### 1. Install build tools

Install Xcode Command Line Tools:

```bash
xcode-select --install
```

Install CMake and Ninja if they are not already available. Homebrew is the simplest option:

```bash
brew install cmake ninja
```

### 2. Configure and build

From the repository root:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The service binary will be created at:

```text
build/service/ppa_service
```

The build also creates:

```text
build/service/ppa_service.toml
```

That runtime config is copied from [ppa_service.toml.example](/Users/p4010/Develop/TheJudge/ppa_service.toml.example:1). Edit `build/service/ppa_service.toml` if your Ollama endpoint, model, or timeout differ from the defaults.

### 3. Run the service

Start the companion service in a terminal and leave it running while testing the plugin:

```bash
./build/service/ppa_service
```

Expected startup message:

```text
ppa_service listening on http://127.0.0.1:6464
```

### 4. Verify the service

In another terminal:

```bash
curl http://127.0.0.1:6464/health
curl http://127.0.0.1:6464/v1/capabilities
```

The health endpoint should return:

```json
{"status":"ok"}
```

### 5. Load the Lightroom plugin

1. Open Lightroom Classic.
2. Open `File > Plug-in Manager...`.
3. Click `Add`.
4. Select `plugin/PpaCritique.lrplugin` from this repository.
5. Confirm that the plugin loads as `The Jury`.

### 6. Run the plugin

1. Select exactly one photo in Lightroom.
2. Start the command `PPA Critique...`.
3. The plugin exports a temporary JPEG, submits it to the local service, and shows a result dialog.
4. Lightroom metadata fields such as `PPA Critique Status` and `PPA Critique Classification` should be updated on the photo.
5. Use `PPA Critique with Semantic Analysis...` to opt into the Ollama-backed semantic stage.

## Windows

### 1. Install build tools

Install:

- Git for Windows
- CMake 3.21 or newer
- Ninja
- Visual Studio 2022 Build Tools with the C++ workload

Open a developer shell that has the MSVC environment loaded, for example:

- `x64 Native Tools Command Prompt for VS 2022`

### 2. Configure and build

From the repository root:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The service binary will be created at:

```text
build\service\ppa_service.exe
```

The build also creates:

```text
build\service\ppa_service.toml
```

That runtime config is copied from [ppa_service.toml.example](/Users/p4010/Develop/TheJudge/ppa_service.toml.example:1). Edit `build\service\ppa_service.toml` if your Ollama endpoint, model, or timeout differ from the defaults.

### 3. Run the service

Start the companion service in a terminal and leave it running while testing the plugin:

```powershell
.\build\service\ppa_service.exe
```

Expected startup message:

```text
ppa_service listening on http://127.0.0.1:6464
```

### 4. Verify the service

In another terminal:

```powershell
curl.exe http://127.0.0.1:6464/health
curl.exe http://127.0.0.1:6464/v1/capabilities
```

### 5. Load the Lightroom plugin

1. Open Lightroom Classic.
2. Open `File > Plug-in Manager...`.
3. Click `Add`.
4. Select `plugin\PpaCritique.lrplugin` from this repository.
5. Confirm that the plugin loads as `The Jury`.

### 6. Run the plugin

1. Select exactly one photo in Lightroom.
2. Start the command `PPA Critique...`.
3. The plugin exports a temporary JPEG, submits it to the local service, and shows a result dialog.
4. Lightroom metadata fields should be updated on the photo after the request completes.
5. Use `PPA Critique with Semantic Analysis...` to opt into the Ollama-backed semantic stage.

## Tests

Build and run the test suite from the repository root:

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

`ctest` does not require a separately running `ppa_service` process. The HTTP tests start their own local server.

## Notes

- Ollama is not required for the default `PPA Critique...` flow.
- `PPA Critique with Semantic Analysis...` requires a reachable local Ollama runtime and a model that accepts images.
- The service is local-only and binds to `127.0.0.1`.

## Troubleshooting

- If `ppa_service` reports `failed to bind http server`, confirm that nothing else is already listening on `127.0.0.1:6464`.
- If `ctest` reports `failed to bind test server`, confirm that the current machine or sandbox allows opening localhost listeners.
