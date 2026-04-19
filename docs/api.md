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
The response reports the `disabled` provider as available and the `ollama` provider with the configured model list. Ollama availability reflects whether the local runtime responds successfully.

## `POST /v1/critique`

Accepts the bootstrap critique payload and returns a structured response with runtime, preflight, semantic, and aggregate fields.
When `options.run_semantic` is `false`, `semantic` is `null` and the aggregate summary stays deterministic.
When the `ollama` provider is selected, the service sends the exported image and critique prompt to the local Ollama runtime and maps the structured result back into the existing semantic fields.
The aggregate object includes both the coarse `classification` and a finer `merit_score` in the `0..100` range for ranking similar images.

## Error handling

Invalid JSON or missing required fields return HTTP 400 with an `invalid_request` payload:

```json
{
  "error": "invalid_request",
  "message": "..."
}
```

Semantic-provider failures return non-2xx payloads such as `semantic_unavailable`, `semantic_failed`, or `semantic_invalid_response`.
