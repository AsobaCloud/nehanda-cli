/* aimee_ir.h -- the canonical, protocol-NEUTRAL intermediate representation for
 * LLM requests/responses. FRONTEND adapters (client wire ↔ IR) and BACKEND adapters
 * (IR ↔ provider wire) both pivot through this; there is NO direct client-shape →
 * provider-shape translation. Design: docs/proposals/pending/aimee-canonical-ir.md
 * (roundtabled 2026-07-01).
 *
 * HYBRID by ruling: a typed core (semantics explicit + testable) PLUS a retained
 * `raw` cJSON sidecar on each object, so unknown provider fields survive a
 * same-protocol round-trip (a pure struct would silently drop them and break
 * prompt-cache / correctness). The typed core is the source of truth for the core
 * stages; the sidecar is for lossless same-protocol replay + observability.
 *
 * TRUST-BOUNDARY RULE: tool names and tool arguments are OPAQUE. Never normalize
 * case/whitespace, never reorder keys, never reformat numbers/Unicode -- mangling
 * them routes tool outputs to the wrong tool (also an injection surface). Store the
 * argument JSON verbatim (the original cJSON subtree). */
#ifndef DEC_AIMEE_IR_H
#define DEC_AIMEE_IR_H 1

#include <stddef.h>

struct cJSON;

/* Which wire protocol an adapter speaks (frontend: the client; backend: provider). */
typedef enum
{
   AIMEE_WIRE_UNKNOWN = 0,
   AIMEE_WIRE_ANTHROPIC,  /* Anthropic Messages API (/v1/messages) */
   AIMEE_WIRE_OPENAI_CHAT, /* OpenAI Chat Completions (/v1/chat/completions) */
   AIMEE_WIRE_RESPONSES,  /* OpenAI Responses API (/v1/responses; codex) */
   AIMEE_WIRE_GEMINI
} aimee_wire_t;

/* A content block. Ordered within a message/response; ordering is significant. */
typedef enum
{
   AIMEE_BLK_TEXT = 0,
   AIMEE_BLK_TOOL_USE,    /* assistant asks to call a tool */
   AIMEE_BLK_TOOL_RESULT, /* user/tool returns a tool's output */
   AIMEE_BLK_IMAGE,
   AIMEE_BLK_DOCUMENT,
   AIMEE_BLK_THINKING,    /* Anthropic extended-thinking / o-series reasoning */
   AIMEE_BLK_UNKNOWN      /* preserved via `raw` only */
} aimee_block_type_t;

typedef struct
{
   aimee_block_type_t type;
   /* TEXT / THINKING */
   char *text;
   /* TOOL_USE: id = the call id (stable, links to the matching TOOL_RESULT);
    * name = opaque tool name; input = opaque argument JSON (borrowed sidecar view,
    * preserved verbatim). TOOL_RESULT: id = the tool_use id it answers;
    * result = opaque content; is_error set on an error result. */
   char *tool_id;
   char *tool_name;
   struct cJSON *tool_input;  /* owned */
   struct cJSON *tool_result; /* owned */
   int tool_is_error;
   /* IMAGE / DOCUMENT: media_type (e.g. "image/png"), and either a base64 payload
    * or a URL in media_ref. */
   char *media_type;
   char *media_ref;
   /* Per-block prompt-cache marker (opaque, e.g. "ephemeral"); NULL if none. The
    * cache decision is per-section, so this must survive block-preserving stages. */
   char *cache_control;
   /* Sidecar: the block's original wire JSON (unknown fields, exact bytes for
    * same-protocol replay). Owned. */
   struct cJSON *raw;
} aimee_block_t;

typedef struct
{
   char *role; /* opaque: "user" / "assistant" / "system" / "tool" / ... */
   aimee_block_t *blocks;
   int n_blocks;
   struct cJSON *raw; /* sidecar */
} aimee_message_t;

typedef struct
{
   char *name;           /* opaque */
   char *description;
   struct cJSON *schema; /* input JSON schema, opaque; owned */
   char *cache_control;
   struct cJSON *raw; /* sidecar */
} aimee_tool_t;

typedef struct
{
   char *model;
   aimee_block_t *system; /* system as ORDERED BLOCKS, not a flat string */
   int n_system;
   aimee_message_t *messages;
   int n_messages;
   aimee_tool_t *tools;
   int n_tools;
   struct cJSON *tool_choice; /* opaque; owned */
   int max_tokens;
   int has_max_tokens;
   double temperature;
   int has_temperature;
   int stream;
   char **stop_sequences;
   int n_stop;
   aimee_wire_t frontend; /* the client wire this was parsed from */
   /* Whole-request sidecar: the ORIGINAL request JSON. Enables the same-protocol
    * raw-passthrough fast-path (frontend==backend, no mutating stage) and lossless
    * replay. Owned. */
   struct cJSON *raw;
} aimee_request_t;

/* Canonical stop reasons; the provider-specific string is kept alongside so nothing
 * is lost to logs/observability. */
typedef enum
{
   AIMEE_STOP_UNKNOWN = 0,
   AIMEE_STOP_END_TURN,   /* natural stop */
   AIMEE_STOP_MAX_TOKENS,
   AIMEE_STOP_TOOL_USE,   /* stopped to call a tool */
   AIMEE_STOP_STOP_SEQUENCE,
   AIMEE_STOP_CONTENT_FILTER,
   AIMEE_STOP_ERROR
} aimee_stop_reason_t;

typedef struct
{
   char *id;
   char *model;
   char *role; /* usually "assistant" */
   aimee_block_t *content;
   int n_content;
   aimee_stop_reason_t stop_reason;
   char *raw_stop_reason; /* provider-specific string (e.g. "stop_sequence") */
   long usage_in;
   long usage_out;
   long usage_cache_read;
   long usage_cache_write;
   long usage_reasoning;
   struct cJSON *raw; /* sidecar */
} aimee_response_t;

/* ---- streaming: a SEPARATE IR surface (event stream, not one object) ----
 * backend.parse_sse emits these; frontend.render_sse consumes them. Ordering and
 * block_id linkage are significant. Size caps are enforced by the delta producer. */
typedef enum
{
   AIMEE_DELTA_TURN_START = 0,
   AIMEE_DELTA_BLOCK_START, /* kind + block_id */
   AIMEE_DELTA_BLOCK_DELTA, /* block_id + kind + incremental payload */
   AIMEE_DELTA_BLOCK_STOP,  /* block_id */
   AIMEE_DELTA_TURN_STOP,   /* stop_reason + usage */
   AIMEE_DELTA_ERROR
} aimee_delta_type_t;

typedef struct
{
   aimee_delta_type_t type;
   int block_id;
   aimee_block_type_t kind;
   char *text_delta;       /* BLOCK_DELTA for text/thinking */
   char *tool_args_delta;  /* BLOCK_DELTA for tool_use argument JSON fragment */
   aimee_stop_reason_t stop_reason; /* TURN_STOP */
   long usage_in, usage_out;        /* TURN_STOP */
   char *error_message;    /* ERROR */
} aimee_delta_t;

/* ---- lifecycle ---- */
void aimee_request_free(aimee_request_t *r);
void aimee_response_free(aimee_response_t *r);
void aimee_block_free_contents(aimee_block_t *b);

/* ---- accessors used by the core stages / KB ----
 * Concatenate the text of the LAST user-role message's TEXT blocks into `buf`
 * (truncating to n). This is the ONE shape-agnostic query extractor that replaces
 * ingress_preinject_query_from_messages' per-shape arms (which dropped non-text
 * blocks). Returns the number of chars written (excluding NUL), or 0 if none. */
size_t aimee_ir_last_user_text(const aimee_request_t *r, char *buf, size_t n);

/* Canonical stop-reason name (stable string for the enum). */
const char *aimee_stop_reason_name(aimee_stop_reason_t s);
aimee_stop_reason_t aimee_stop_reason_parse(const char *canonical_name);

/* 1 if two requests are SEMANTICALLY equal: same system blocks, messages (role +
 * ordered content blocks incl. tool ids/names/args/results + cache_control),
 * tools, tool_choice, and sampling params. IGNORES provenance that legitimately
 * differs across frontends: `frontend` wire tag, the `raw` sidecar, and `model`
 * (the client's model string). This is the golden-test assertion that an
 * Anthropic-shaped turn and an OpenAI-shaped turn with identical semantics produce
 * identical IR -> identical KB input + identical backend build. */
int aimee_ir_request_equal(const aimee_request_t *a, const aimee_request_t *b);

#endif /* DEC_AIMEE_IR_H */
