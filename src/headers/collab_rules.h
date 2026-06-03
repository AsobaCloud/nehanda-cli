#ifndef DEC_COLLAB_RULES_H
#define DEC_COLLAB_RULES_H 1

#define COLLAB_RULE_TEXT_LEN    160
#define COLLAB_RULE_REASON_LEN  240
#define COLLAB_RULE_AGENT_LEN   64
#define COLLAB_MAX_ACTIVE_RULES 10
#define COLLAB_MAX_TOTAL_RULES  50

typedef enum
{
   COLLAB_PROPOSED,
   COLLAB_ACTIVE,
   COLLAB_REJECTED,
   COLLAB_RETIRED
} collab_rule_status_t;

typedef struct
{
   int id;
   char text[COLLAB_RULE_TEXT_LEN + 1];
   char reason[COLLAB_RULE_REASON_LEN + 1];
   char proposed_by[COLLAB_RULE_AGENT_LEN + 1];
   collab_rule_status_t status;
   char created_at[32];
   char decided_at[32];
} collab_rule_t;

#endif /* DEC_COLLAB_RULES_H */
