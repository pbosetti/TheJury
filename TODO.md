# Feature Evaluation and Recommended Development Order

This document evaluates the proposed features against the current codebase and recommends the order in which to build them.

Current baseline:
- The Lightroom plugin already exports one selected photo, sends it to the local service, writes response fields back into Lightroom metadata, and can optionally run the Ollama semantic stage.
- The service already exposes a local HTTP API, reads TOML configuration, and supports one semantic provider with one semantic result per critique.

All items below are technically doable. The main differences are implementation cost, architectural risk, and whether they unlock later work.

## Recommended order

1. `[DONE] Critique Category`
2. `[DONE] Improve progress feedback`
3. `[DONE] Allow multiple photo selection`
4. `[DONE] Plugin settings`
5. `[DONE] Support higher resolution merit index`
6. `Use multiple jurors`
7. `macOS: make the service a menu bar app`
8. `Windows: make the service a tray area element`

## 1. [DONE] Critique Category

Status: `Done`.

Why first:
- The plugin already sends `category = 'illustrative'` in [`CritiqueMenu.lua`](plugin/PpaCritique.lrplugin/CritiqueMenu.lua).
- The metadata schema already contains `ppaCritiqueCategory`.
- This is a small change with immediate user value and no service redesign.

Possible implementation:
- Define the supported PPA categories in one Lua table.
- Before running the critique, present a simple popup menu or custom dialog listing those categories.
- Preselect the current `ppaCritiqueCategory` plugin metadata value if present.
- Write the chosen category back to metadata before or after the request.
- Send the chosen category in the existing request payload instead of hardcoding `illustrative`.

Technical notes:
- The simplest implementation is plugin-only.
- No API change is required unless category-specific service behavior is added later.

## 2. [DONE] Improve progress feedback

Status: `Done`.

Why second:
- The current flow is linear and already runs inside an async Lightroom task.
- This feature becomes more important before batch processing is added.
- It also reduces dependence on the final popup dialog.

Possible implementation:
- Introduce `LrProgressScope` in the critique command.
- Update progress through these phases:
  - collect metadata
  - export JPEG
  - submit request
  - wait for response
  - write metadata
- Show a determinate progress bar for batch mode later, and a short indeterminate phase for the service call if needed.
- Suppress `ResultDialog.show(...)` on success by default.
- Keep popup dialogs only for failures, cancellation, or invalid responses.

Technical notes:
- This should be done before multi-photo support, otherwise batch runs will feel opaque.
- The same progress abstraction can be reused when multiple photos are processed.

## 3. [DONE] Allow multiple photo selection

Status: `Done`.

Why third:
- The plugin currently hard-requires exactly one selected photo in [`Utils.lua`](plugin/PpaCritique.lrplugin/Utils.lua).
- The feature is straightforward conceptually, but it needs progress reporting and better error handling first.

Possible implementation:
- Replace `Utils.getSelectedPhoto()` with a helper that returns all target photos.
- If zero photos are selected, show a warning.
- Iterate over the selected photos inside one async task.
- For each photo:
  - export JPEG
  - build request
  - submit to service
  - write metadata back to that photo
- Use `LrProgressScope` to report `n / total`.
- Allow cancellation between photos.
- At the end, show either:
  - no popup on full success, or
  - a brief summary dialog listing failures.

Technical notes:
- Avoid one popup per photo.
- The service can stay single-request for now; batch support can remain plugin-side.
- This may require small refactoring to separate “process one photo” from the current command body.

## 4. [DONE] Plugin settings

Status: `Done`.

Why fourth:
- Users will want model selection and defaults once semantic analysis is used more often.
- The risky part is not the Lightroom UI, but deciding where the source of truth for settings lives.

Possible implementation:
- Add a Lightroom Plug-in Manager panel using `LrPluginInfoProvider` and `LrView`.
- Expose settings such as:
  - default semantic mode
  - preferred Ollama model
  - fallback model
  - service URL if you later want it configurable
- Populate the model list from `GET /v1/capabilities`.
- Save plugin-side settings via `LrPrefs`.

Recommended architecture:
- Do not make the plugin edit the service TOML file directly as the first implementation.
- Instead, add a service config API such as:
  - `GET /v1/config`
  - `PUT /v1/config`
- Let the service remain the owner of `ppa_service.toml`, and let the plugin talk to that API.

Technical notes:
- Editing TOML directly from Lightroom is possible, but it couples the plugin to the service deployment path and file format.
- A config API is cleaner and also helps future tray/menu-bar apps.

## 5. [DONE] Support higher resolution merit index

Status: `Done`.

Why fifth:
- The current aggregate output already includes `classification`, `merit_probability`, and `confidence`.
- A more granular score is a natural extension once the basic workflow is stable.

Possible implementation:
- Add one or more numeric fields to the aggregate response, for example:
  - `merit_score`
  - `technical_score`
  - `impact_score`
  - `ranking_score`
- Keep the current PPA-style classification unchanged for compatibility.
- Derive the finer score from existing preflight and semantic signals first.
- Write the new score fields into Lightroom custom metadata.
- Show them in the result dialog and later use them for sorting or filtering.

Technical notes:
- This is mostly a service-side design task.
- The main risk is scoring semantics, not implementation complexity.
- Start with one stable numeric index before adding multiple sub-scores.

## 6. Use multiple jurors

Status: `Doable`, but higher risk and noticeably more design work.

Why sixth:
- The current semantic model and metadata schema assume one semantic summary and effectively one vote.
- A five-juror panel is feasible, but it pushes the response model beyond the current single-result shape.

Possible implementation:
- Extend `ppa_service.toml` with an array of juror definitions, for example:
  - juror name
  - personality prompt
  - weight
- Run one semantic pass per juror, either:
  - sequentially with separate prompts, or
  - as one structured prompt asking the model to return five distinct juror opinions.
- Aggregate the panel into:
  - one final classification
  - one combined summary
  - per-juror rationale data
- Store the aggregate in the existing fields and optionally add new metadata fields for panel details.

Recommended implementation order inside this feature:
1. Add service-side support for multiple jurors with a fixed internal default panel.
2. Add TOML customization only after the response format is stable.
3. Add Lightroom display of per-juror details last.

Technical notes:
- This will likely increase latency significantly.
- It also creates a schema question: whether Lightroom metadata should store only the combined result or all five juror outputs.
- This feature becomes much easier after plugin settings and richer scoring exist.

## 7. macOS: make the service a menu bar app

Status: `Doable`, high effort, platform-specific.

Why seventh:
- The UI is feasible, but it depends on having service status and config endpoints worth showing.
- Without a proper runtime status API, the menu bar app would mostly be a process launcher.

Possible implementation:
- Build a small macOS status-bar app that:
  - launches or stops `ppa_service`
  - displays current provider and model
  - shows whether the service is reachable
  - shows the number of in-flight critique jobs
- Add service endpoints first, such as:
  - `GET /health`
  - `GET /v1/capabilities`
  - `GET /v1/status` for queue depth, active jobs, current model
- Package the service and launcher together.

Technical notes:
- The cleanest macOS implementation is likely a small Swift menu-bar app, even though the service remains C++.
- Packaging, code signing, and auto-start behavior will take more time than the UI itself.
- This should be treated as productization work, not core critique logic.

## 8. Windows: make the service a tray area element

Status: `Doable`, high effort, platform-specific.

Why eighth:
- It solves the same problem as the macOS menu-bar app but with a separate implementation stack.
- It should reuse the same service-side status and config endpoints introduced for macOS.

Possible implementation:
- Build a small Windows tray application using Win32 or WinUI.
- Capabilities:
  - start and stop the service
  - show service health
  - show current Ollama model
  - show current queue depth or active job count
- Reuse the same service HTTP status/config APIs proposed for macOS.

Technical notes:
- This should follow the macOS launcher, not precede it.
- Cross-platform logic can be shared in the service, but the tray UI itself is OS-specific.

## Shared enablers worth planning early

These are not separate feature requests, but they would reduce rework:

- Add a small service status endpoint with queue and active-job information.
  - This helps progress reporting, tray/menu-bar apps, and debugging.
- Add a service config API instead of making the plugin edit TOML directly.
  - This helps plugin settings and desktop launcher apps.
- Refactor the plugin command into reusable steps:
  - collect selected photos
  - process one photo
  - update progress
  - handle errors
  - write metadata
  - This makes category selection, batching, and no-popup completion much easier to maintain.

## Final recommendation

The best near-term sequence is:

1. make category user-selectable
2. add proper progress handling and reduce popup usage
3. support batch selection
4. add settings through a plugin UI backed by service APIs

After that, move to scoring and multi-juror semantics. The platform launcher apps should come last, because they depend more on service management and packaging than on critique logic itself.
