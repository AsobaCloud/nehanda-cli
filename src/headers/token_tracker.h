/* token_tracker.h: shared token usage normalisation and cost estimation */
#ifndef DEC_TOKEN_TRACKER_H
#define DEC_TOKEN_TRACKER_H 1

/* Per-request token usage, normalised across providers */
typedef struct
{
   int input_tokens;
   int output_tokens;
   int cache_write_tokens; /* Anthropic: cache_creation_input_tokens */
   int cache_read_tokens;  /* Anthropic: cache_read_input_tokens */
} token_usage_t;

/* Estimate cost in USD for a single request.
 * model is matched by substring against an internal pricing table.
 * Returns 0.0 when the model is unknown (no pricing data). */
double token_estimate_cost(const char *model, const token_usage_t *usage);

#endif /* DEC_TOKEN_TRACKER_H */
