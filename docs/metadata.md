# Lightroom metadata fields

The plugin writes a minimal custom metadata set after each critique run.

- `ppaCritiqueStatus`: current preflight status returned by the service.
- `ppaCritiqueCategory`: critique category submitted by the plugin.
- `ppaCritiqueClassification`: aggregate result classification.
- `ppaCritiqueMeritProbability`: aggregate merit probability value.
- `ppaCritiqueConfidence`: aggregate confidence value.
- `ppaCritiqueSemanticSummary`: semantic-stage summary text.
- `ppaCritiqueSemanticVote`: semantic-stage vote from the first judge/model response.
- `ppaCritiqueSemanticVoteConfidence`: confidence for the semantic vote.
- `ppaCritiqueSemanticRationale`: rationale text for the semantic vote.
- `ppaCritiqueSemanticStrengths`: comma-separated semantic strengths.
- `ppaCritiqueSemanticImprovements`: comma-separated semantic improvements.
- `ppaCritiqueLastAnalyzedAt`: UTC timestamp of the last critique submission.
- `ppaCritiqueSemanticProvider`: semantic provider used for the run.
- `ppaCritiqueModel`: semantic model name reported by the service.

## Update lifecycle

1. The user selects a single photo and runs `PPA Critique...`.
2. The plugin exports a temporary JPEG rendition and submits the critique request.
3. When the service responds, the plugin updates all metadata fields inside a catalog write-access block.
4. The plugin then shows the returned summary to the user.
