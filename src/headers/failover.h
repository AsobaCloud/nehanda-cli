#ifndef DEC_FAILOVER_H
#define DEC_FAILOVER_H 1

typedef enum
{
   FAILOVER_NONE = 0,
   FAILOVER_AUTH,
   FAILOVER_AUTH_PERMANENT,
   FAILOVER_BILLING,
   FAILOVER_RATE_LIMIT,
   FAILOVER_OVERLOADED,
   FAILOVER_SERVER_ERROR,
   FAILOVER_TIMEOUT,
   FAILOVER_CONTEXT_OVERFLOW,
   FAILOVER_PAYLOAD_TOO_LARGE,
   FAILOVER_MODEL_NOT_FOUND,
   FAILOVER_PROVIDER_POLICY,
   FAILOVER_FORMAT_ERROR,
   FAILOVER_CONTENT_SHAPE,
   FAILOVER_UNKNOWN,
} failover_reason_t;

typedef enum
{
   RECOVER_NONE = 0,
   RECOVER_BACKOFF,
   RECOVER_ROTATE_CREDENTIAL,
   RECOVER_FALLBACK_PROVIDER,
   RECOVER_FALLBACK_MODEL,
   RECOVER_COMPRESS_CONTEXT,
   RECOVER_SHRINK_PAYLOAD,
   RECOVER_DOWNGRADE_CONTENT,
   RECOVER_ABORT,
} recover_action_t;

failover_reason_t failover_classify(const char *provider, int http_status, const char *body);
recover_action_t failover_action(failover_reason_t reason);
int failover_reason_uses_backoff(failover_reason_t reason);
const char *failover_reason_name(failover_reason_t reason);
const char *failover_action_name(recover_action_t action);

int failover_record_event(const char *session_id, const char *provider, const char *model,
                          int http_status, failover_reason_t reason, recover_action_t action,
                          int attempt, int recovered);

#endif /* DEC_FAILOVER_H */
