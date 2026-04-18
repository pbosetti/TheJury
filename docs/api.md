# API

## `GET /health`

Returns:

```json
{
  "status": "ok"
}
```

## `GET /v1/capabilities`

Returns the current stub provider configuration for the local service.

## `POST /v1/critique`

Accepts the bootstrap critique payload and returns a stub response with runtime, preflight, and aggregate fields.

Invalid JSON or missing required fields return HTTP 400 with an `invalid_request` payload.
