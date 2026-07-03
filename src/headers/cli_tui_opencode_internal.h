#ifndef CLI_TUI_OPENCODE_INTERNAL_H
#define CLI_TUI_OPENCODE_INTERNAL_H
#include <stddef.h>
#include <pthread.h>
#include <signal.h>
#include "cJSON.h"
/* INTERNAL: cross-TU decls for the cli_tui_opencode split; unstable. */
#include "cli_tui.h"
typedef struct opencode_v2_event
{
   unsigned long long seq;
   char *body;
   struct opencode_v2_event *next;
} opencode_v2_event_t;

typedef struct opencode_v2_turn
{
   char user_id[96];
   char assistant_id[96];
   long long created_ms;
   long long completed_ms;
   char *user_text;
   char *assistant_text;
   size_t assistant_len;
   size_t assistant_cap;
   int assistant_started;
   int user_visible;
   int user_published;
   int prompt_published;
   struct opencode_v2_turn *next;
} opencode_v2_turn_t;

typedef struct opencode_v2_prompt_job
{
   opencode_v2_turn_t *turn;
   struct opencode_v2_prompt_job *next;
} opencode_v2_prompt_job_t;

typedef struct
{
   int (*should_abort)(void *userdata);
   void (*set_stream_fd)(int fd, void *userdata);
   void *userdata;
   int aborted;
} builtin_chat_stream_control_t;

/* promoted cross-TU (former .inc statics) */
int append_text(char **buf, size_t *len, size_t *cap, const char *text);
int builtin_chat_send_streaming_control(const char *sock, const char *provider_session_id,
                                               const char *aimee_session_id, const char *message,
                                               char **reply_out, char *provider_session_out,
                                               size_t provider_session_out_len,
                                               void (*text_cb)(const char *text, void *userdata),
                                               void (*event_cb)(const char *event, void *userdata),
                                               void *text_cb_data,
                                               builtin_chat_stream_control_t *control);
void chat_format_aimee_model_label(const char *provider, const char *model,
                                          const char *effort, char *out, size_t out_len);
int cli_chat_presence_attach(const char *sock, const char *session_id, const char *surface,
                                    char *out_attach, size_t out_n);
void cli_chat_presence_detach(const char *sock, const char *session_id,
                                     const char *attach_id);
int opencode_exec_tui(const char *sock, const char *agent_name, const char *model,
                             const char *effort, const char *session_id, int autonomous, int debug,
                             int default_launch, int argc, char **argv, int *handled);

#define BUILTIN_CHAT_SEND_TRANSPORT_ERROR   2

#define CLI_TUI_PATH_MAX                    4096

typedef struct
{
   pthread_mutex_t lock;
   pthread_cond_t cond;
   const char *sock;
   const char *agent_name;
   const char *model;
   const char *effort;
   const char *aimee_session_id;
   int autonomous;
   int debug;
   int closing;
   int busy;
   int queue_worker_active;
   int abort_requested;
   int active_stream_fd;
   int awaiting_persona_pick; /* armed by `/persona`; next bare number selects */
   unsigned int turn_seq;
   unsigned int id_seq;
   unsigned long long seq;
   size_t event_count;
   long long created_ms;
   char cwd[CLI_TUI_PATH_MAX];
   char root[CLI_TUI_PATH_MAX];
   char session_id[96];
   char title[256];
   char model_label[512];
   char provider_session_id[256];
   char attach_id[64]; /* unified-presence "tui" attachment for this session */
   int attached;
   opencode_v2_event_t *events_head;
   opencode_v2_event_t *events_tail;
   opencode_v2_prompt_job_t *queue_head;
   opencode_v2_prompt_job_t *queue_tail;
   opencode_v2_turn_t *turns_head;
   opencode_v2_turn_t *turns_tail;
   opencode_v2_turn_t *active_turn;
} opencode_v2_bridge_t;
typedef struct
{
   opencode_v2_bridge_t *bridge;
   int fd;
} opencode_v2_client_t;
typedef struct
{
   opencode_v2_bridge_t *bridge;
} opencode_v2_queue_worker_t;
/* promoted cross-TU (former .inc statics) */
cJSON *opencode_v2_agent_json(opencode_v2_bridge_t *b);
cJSON *opencode_v2_config_providers_json(opencode_v2_bridge_t *b);
char *opencode_v2_extract_message_id(const char *body);
char *opencode_v2_extract_prompt(const char *body);
cJSON *opencode_v2_file_content_json_locked(opencode_v2_bridge_t *b, const char *query);
cJSON *opencode_v2_file_list_json_locked(opencode_v2_bridge_t *b, const char *query);
int opencode_v2_find_on_path(const char *name, char *out, size_t out_len);
opencode_v2_turn_t *opencode_v2_find_turn_locked(opencode_v2_bridge_t *b, const char *id,
                                                        int *assistant);
void opencode_v2_free_events(opencode_v2_bridge_t *b);
void opencode_v2_free_prompt_jobs(opencode_v2_prompt_job_t *job);
void opencode_v2_free_turns(opencode_v2_bridge_t *b);
int opencode_v2_has_prior_unstarted_turn_locked(opencode_v2_bridge_t *b,
                                                       opencode_v2_turn_t *turn);
void opencode_v2_hash_id(char *out, size_t out_len, const char *prefix, const char *seed);
cJSON *opencode_v2_legacy_bundle_locked(opencode_v2_bridge_t *b, opencode_v2_turn_t *turn,
                                               int assistant);
cJSON *opencode_v2_legacy_message_props_locked(opencode_v2_bridge_t *b,
                                                      opencode_v2_turn_t *turn, int assistant);
cJSON *opencode_v2_legacy_messages_json_locked(opencode_v2_bridge_t *b);
cJSON *opencode_v2_legacy_part_json_locked(opencode_v2_bridge_t *b, opencode_v2_turn_t *turn,
                                                  int assistant);
cJSON *opencode_v2_legacy_part_props_locked(opencode_v2_bridge_t *b,
                                                   opencode_v2_turn_t *turn, int assistant);
cJSON *opencode_v2_message_json_locked(opencode_v2_bridge_t *b, opencode_v2_turn_t *turn,
                                              int assistant);
cJSON *opencode_v2_messages_json_locked(opencode_v2_bridge_t *b);
cJSON *opencode_v2_model_json(opencode_v2_bridge_t *b);
long long opencode_v2_now_ms(void);
void opencode_v2_on_term(int sig);
cJSON *opencode_v2_path_json_locked(opencode_v2_bridge_t *b);
cJSON *opencode_v2_project_json_locked(opencode_v2_bridge_t *b);
const char *opencode_v2_provider_id(const opencode_v2_bridge_t *b);
cJSON *opencode_v2_provider_json(opencode_v2_bridge_t *b);
cJSON *opencode_v2_provider_list_legacy_json(opencode_v2_bridge_t *b);
void opencode_v2_publish_locked(opencode_v2_bridge_t *b, const char *type, cJSON *properties);
void opencode_v2_publish_user_turn_locked(opencode_v2_bridge_t *b, opencode_v2_turn_t *turn);
void opencode_v2_send_bool(int fd, int value);
void opencode_v2_send_empty_array(int fd);
void opencode_v2_send_empty_object(int fd);
void opencode_v2_send_json(int fd, cJSON *json);
void opencode_v2_send_no_content(int fd);
void opencode_v2_send_status(int fd, int status, const char *ctype, size_t len);
void opencode_v2_send_text(int fd, int status, const char *ctype, const char *body);
cJSON *opencode_v2_session_json_locked(opencode_v2_bridge_t *b);
cJSON *opencode_v2_session_props_locked(opencode_v2_bridge_t *b);
cJSON *opencode_v2_session_status_map_locked(opencode_v2_bridge_t *b);
void opencode_v2_set_title_locked(opencode_v2_bridge_t *b, const char *base);
cJSON *opencode_v2_status_props_locked(opencode_v2_bridge_t *b);
cJSON *opencode_v2_step_end_props_locked(opencode_v2_bridge_t *b, long long timestamp);
cJSON *opencode_v2_step_props_locked(opencode_v2_bridge_t *b, long long timestamp);
void opencode_v2_stream_delta(const char *text, void *userdata);
void opencode_v2_stream_event(const char *event, void *userdata);
void opencode_v2_stream_events(opencode_v2_bridge_t *b, int fd, int global);
cJSON *opencode_v2_sync_history_json_locked(opencode_v2_bridge_t *b);
opencode_v2_prompt_job_t *opencode_v2_take_prompt_jobs_locked(opencode_v2_bridge_t *b);
cJSON *opencode_v2_text_props_locked(opencode_v2_bridge_t *b, const char *field,
                                            const char *value);
cJSON *opencode_v2_v2_messages_response_locked(opencode_v2_bridge_t *b);
cJSON *opencode_v2_v2_sessions_response_locked(opencode_v2_bridge_t *b);

extern volatile sig_atomic_t g_opencode_v2_term;
typedef struct
{
   opencode_v2_bridge_t *bridge;
   opencode_v2_turn_t *turn;
} opencode_v2_stream_t;

opencode_v2_turn_t * opencode_v2_create_turn_locked(opencode_v2_bridge_t *b, const char *prompt, const char *message_id);
#endif
