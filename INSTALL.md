# Install

These instructions cover building and loading The Jury for Lightroom Classic.

The plugin now manages its bundled local service automatically. You do not need to start `ppa_service` manually in a terminal.

## Prerequisites

Common requirements:

- `git`
- CMake 3.21 or newer
- Ninja
- a C++20-capable compiler
- an internet connection for the first CMake configure step

Lightroom requirements:

- Adobe Lightroom Classic 13.0 or newer

Optional semantic-analysis requirements:

- a local Ollama installation
- at least one image-capable model available to Ollama

The first configure step downloads third-party dependencies with `FetchContent`.

## macOS

### 1. Install build tools

Install Xcode Command Line Tools:

```bash
xcode-select --install
```

Install CMake and Ninja if needed:

```bash
brew install cmake ninja
```

### 2. Configure and build

From the repository root:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

This build:

- creates the service binaries under `build/service/`
- copies the managed service files into `plugin/PpaCritique.lrplugin/bin/macos-arm64/` or `plugin/PpaCritique.lrplugin/bin/macos-x86_64/`

### 3. Load the plugin in Lightroom

1. Open Lightroom Classic.
2. Open `File > Plug-in Manager...`.
3. Click `Add`.
4. Select `plugin/PpaCritique.lrplugin`.
5. Confirm that the plugin loads as `The Jury`.

### 4. Check plugin settings

In the Plug-in Manager pane:

1. Confirm the plugin version shown in the settings pane.
2. Wait for the managed service runtime section to show the service as reachable.
3. If you use Ollama, set:
   - Ollama URL
   - primary model
   - fallback model
   - timeout

### 5. Run the plugin

1. Select one or more photos in Lightroom.
2. Run `Library > Plug-in Extras > PPA Critique...`.
3. The plugin will automatically start the local service if it is not already running.
4. After the critique finishes, Lightroom metadata fields such as critique status and classification should be updated on the selected photo(s).

## Windows

### 1. Install build tools

Install:

- Git for Windows
- CMake 3.21 or newer
- Ninja
- Visual Studio 2022 Build Tools with the C++ workload

Open a developer shell with the MSVC environment loaded, for example:

- `x64 Native Tools Command Prompt for VS 2022`

### 2. Configure and build

From the repository root:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

This build:

- creates the service binaries under `build\service\`
- copies the managed service files into `plugin\PpaCritique.lrplugin\bin\windows-x86_64\`

### 3. Load the plugin in Lightroom

1. Open Lightroom Classic.
2. Open `File > Plug-in Manager...`.
3. Click `Add`.
4. Select `plugin\PpaCritique.lrplugin`.
5. Confirm that the plugin loads as `The Jury`.

### 4. Check plugin settings

In the Plug-in Manager pane:

1. Confirm the plugin version shown in the settings pane.
2. Wait for the managed service runtime section to show the service as reachable.
3. If you use Ollama, set:
   - Ollama URL
   - primary model
   - fallback model
   - timeout

### 5. Run the plugin

1. Select one or more photos in Lightroom.
2. Run `Library > Plug-in Extras > PPA Critique...`.
3. The plugin will automatically start the local service if it is not already running.
4. After the critique finishes, Lightroom metadata fields should be updated on the selected photo(s).

## Tests

From the repository root:

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

`ctest` does not require a separately running `ppa_service` process. The test suite starts its own local server instances.

## Notes

- Ollama is optional for the basic critique flow.
- The service is local-only and binds to `127.0.0.1:6464`.
- The plugin manages start and stop of the bundled service automatically.

## Troubleshooting

- If the Plug-in Manager runtime section never becomes reachable, rebuild the project and reload the plugin so the latest bundled service binaries are present in `plugin/PpaCritique.lrplugin/bin/`.
- If startup reports that the service could not bind `127.0.0.1:6464`, another process is already using that port.
- If semantic analysis fails, confirm that Ollama is running and that the configured model supports image input.
