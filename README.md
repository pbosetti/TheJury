# The Jury

The Jury is a Lightroom Classic plugin for running a local PPA-oriented image critique workflow on selected photos.

It is designed to stay local:

- Lightroom exports a temporary JPEG from the selected photo.
- A bundled local companion service evaluates the request on `127.0.0.1`.
- The plugin shows the result and writes the key fields back into Lightroom metadata.

## Features

The plugin currently supports:

- running `PPA Critique...` from Lightroom Classic
- optional Ollama-backed semantic analysis
- automatic start and stop of the bundled local service
- a Plug-in Manager settings pane for service/runtime status and configuration
- writing critique output into custom Lightroom metadata fields

## Scope

The plugin is aimed at photographers who want a local critique assistant inside Lightroom rather than a separate standalone application.

Current scope:

- local-only workflow
- Lightroom Classic 13 or newer
- single-machine use
- critique orchestration and metadata write-back

Not in scope:

- cloud hosting
- remote multi-user service deployment
- direct image judging inside Lightroom without the local companion service

## Requirements

- Adobe Lightroom Classic 13 or newer
- macOS or Windows
- a local build of this repository as described in [INSTALL.md](/Users/p4010/Develop/TheJudge/INSTALL.md:1)
- optional: a local Ollama installation for semantic analysis

## How To Use It

1. Build the project as described in [INSTALL.md](/Users/p4010/Develop/TheJudge/INSTALL.md:1).
2. Load `plugin/PpaCritique.lrplugin` in Lightroom Classic through `File > Plug-in Manager...`.
3. Open the plugin settings pane in Plug-in Manager and confirm the managed service shows as reachable.
4. Optionally adjust Ollama URL, model, timeout, and plugin defaults.
5. Select one or more photos in Lightroom.
6. Run `Library > Plug-in Extras > PPA Critique...`.

The plugin will automatically:

- start the bundled local service if needed
- export a temporary rendition
- submit the critique request
- persist the returned result into Lightroom metadata

## Semantic Analysis

The default critique flow does not require Ollama.

If you want semantic analysis:

- install and run a local Ollama instance
- ensure the configured model supports image input
- configure the Ollama URL and model in the plugin settings pane

## Installation

See [INSTALL.md](/Users/p4010/Develop/TheJudge/INSTALL.md:1) for platform-specific setup and build steps.

## Local Operation

- The service is bundled and managed by the Lightroom plugin; users do not need to launch it manually.
- The service is local-only and binds to `127.0.0.1:6464`.
