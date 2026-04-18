# API

## `GET /health`

Returns:

```json
{
  "status": "ok"
}
```

## `GET /v1/capabilities`

Returns the current provider configuration for the local service.
The bootstrap response reports the `disabled` provider as available and the `ollama` provider as present with the configured model list.

## `POST /v1/critique`

Accepts the bootstrap critique payload and returns a structured response with runtime, preflight, semantic, and aggregate fields.
When `options.run_semantic` is `false`, `semantic` is `null` and the aggregate summary stays deterministic.
When the `ollama` provider is selected, the service returns a stub semantic block that demonstrates the provider wiring without performing real model inference.

## Error handling

Invalid JSON or missing required fields return HTTP 400 with an `invalid_request` payload:

```json
{
  "error": "invalid_request",
  "message": "..."
}
```
