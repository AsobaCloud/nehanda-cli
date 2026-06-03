/* plugin.h: Plugin system for aimee — manifest parsing, registry, hooks, tools */
#ifndef DEC_PLUGIN_H
#define DEC_PLUGIN_H 1

#include "config.h"

#define PLUGIN_MAX_HOOKS       16
#define PLUGIN_MAX_TOOLS       16
#define PLUGIN_MAX_PERMISSIONS 8
#define PLUGIN_MAX_PLUGINS     64

#define PLUGIN_MANIFEST_FILE ".aimee-plugin/plugin.json"
#define PLUGIN_REGISTRY_FILE "plugins/installed.json"

/* --- Forward declarations for pluggable types --- */

/* delegate_backend_t is defined in delegate_backend.h;
 * we forward-declare here so plugin_ctx_t can reference it. */
struct delegate_backend;
typedef struct delegate_backend delegate_backend_t;

/* memory_provider_t is defined in memory_provider.h;
 * forward-declared here so plugin_ctx_t can reference it. */
struct memory_provider;
typedef struct memory_provider memory_provider_t;

/* context_engine_t is defined in context_engine.h;
 * forward-declared here so plugin_ctx_t can reference it. */
struct context_engine;
typedef struct context_engine context_engine_t;

/* --- Plugin kind taxonomy ---
 *
 * Determines load order and replacement semantics.
 * - STANDALONE:  default; loaded on demand, non-exclusive
 * - BACKEND:     auto-loaded; one active at a time (exclusive replaceable substrate)
 * - EXCLUSIVE:   one active at a time (e.g. memory provider)
 * - PLATFORM:    auto-loaded; additive (multiple can coexist) */
typedef enum
{
   PLUGIN_KIND_STANDALONE = 0, /* default */
   PLUGIN_KIND_BACKEND,        /* exclusive replaceable substrate */
   PLUGIN_KIND_EXCLUSIVE,      /* one active at a time */
   PLUGIN_KIND_PLATFORM        /* additive, auto-loaded */
} plugin_kind_t;

const char *plugin_kind_name(plugin_kind_t k);
plugin_kind_t plugin_kind_from_str(const char *s);

/* --- Capability flags for plugin.yaml capabilities[] --- */
typedef enum
{
   PLUGIN_CAP_NONE = 0,
   PLUGIN_CAP_TOOLS = 1 << 0,
   PLUGIN_CAP_HOOKS = 1 << 1,
   PLUGIN_CAP_SLASH_COMMANDS = 1 << 2,
   PLUGIN_CAP_CLI_SUBCOMMANDS = 1 << 3,
   PLUGIN_CAP_DELEGATE_BACKEND = 1 << 4,
   PLUGIN_CAP_MEMORY_PROVIDER = 1 << 5,
   PLUGIN_CAP_CONTEXT_ENGINE = 1 << 6,
   PLUGIN_CAP_MODEL_PROVIDER = 1 << 7,
   PLUGIN_CAP_PLATFORM_ADAPTER = 1 << 8
} plugin_capability_t;

/* Valid permission levels (declared in tool spec) */
typedef enum
{
   PLUGIN_PERM_READ = 0, /* file inspection */
   PLUGIN_PERM_WRITE,    /* file modification */
   PLUGIN_PERM_EXECUTE,  /* shell commands */
   PLUGIN_PERM_DANGEROUS /* network, destructive ops */
} plugin_permission_t;

const char *plugin_permission_name(plugin_permission_t p);
plugin_permission_t plugin_permission_from_str(const char *s);

/* --- Hook entry from a plugin --- */
typedef struct
{
   char event[32];    /* "PreToolUse" or "PostToolUse" */
   char command[512]; /* path to hook script */
} plugin_hook_t;

/* --- Tool entry from a plugin --- */
typedef struct
{
   char name[64];
   char description[512];
   char command[512];
   char input_schema_json[2048];
   plugin_permission_t permission;
} plugin_tool_t;

/* --- Slash command entry --- */
typedef struct
{
   char name[64]; /* e.g. "triage" -> /triage */
   char description[256];
   char command[512]; /* path to handler script */
} plugin_slash_cmd_t;

/* --- CLI subcommand entry --- */
typedef struct
{
   char name[64]; /* subcommand name, e.g. "dbstat" -> aimee dbstat */
   char description[256];
   char command[512]; /* path to handler binary/script */
} plugin_cli_cmd_t;

/* --- Model provider entry --- */
typedef struct
{
   char name[64]; /* e.g. "openai", "anthropic" */
   char api_url[256];
   char model_name[128];
   char api_key_env[64]; /* env var name holding the API key */
   int default_provider; /* 1 if this should be the default */
} plugin_model_provider_t;

/* --- Platform adapter entry (stub for multi-channel ambient presence) --- */
typedef struct
{
   char name[64]; /* e.g. "slack", "discord", "mattermost" */
   char description[256];
   char webhook_endpoint[512];
   int enabled;
} plugin_platform_adapter_t;

/* --- Memory provider entry --- */
typedef struct
{
   char name[64]; /* e.g. "sqlite", "pg", "neo4j" */
   char description[256];
   char connection_string[512]; /* templated, env-var refs allowed */
   int is_default;              /* 1 if this is the default memory provider */
} plugin_memory_provider_t;

/* --- Context engine entry --- */
typedef struct
{
   char name[64]; /* e.g. "default", "vector-graph", "semantic" */
   char description[256];
   int is_default; /* 1 if this is the default context engine */
} plugin_context_engine_t;

/* --- Full plugin descriptor (in-memory representation of plugin.json) --- */
typedef struct
{
   char name[64];
   char version[32];
   char description[256];
   char source_path[MAX_PATH_LEN]; /* absolute path to plugin directory */
   int enabled;
   plugin_kind_t kind;       /* taxonomy: standalone/backend/exclusive/platform */
   plugin_capability_t caps; /* ORed PLUGIN_CAP_* flags */

   plugin_hook_t hooks[PLUGIN_MAX_HOOKS];
   int hook_count;

   plugin_tool_t tools[PLUGIN_MAX_TOOLS];
   int tool_count;

   int default_enabled;

   /* Extended manifest fields */
   char capabilities_json[2048];                  /* raw JSON array from manifest */
   char config_schema_json[4096];                 /* JSON schema for plugin config */
   char required_env[PLUGIN_MAX_PERMISSIONS][64]; /* required env var names */
   int required_env_count;
} plugin_t;

/* --- plugin_ctx_t: the unified registration context ---
 *
 * All plugin contributors implement `int register(plugin_ctx_t *ctx)`.
 * The ctx provides typed registration methods for each extension surface.
 * Plugins call ctx->register_* in their register() function to declare
 * what they provide; aimee stores the registrations in global lists. */
typedef struct plugin_ctx plugin_ctx_t;
struct plugin_ctx
{
   /* Plugin identity — set by aimee before calling register() */
   const char *plugin_name;
   const char *plugin_version;
   const char *source_path;
   plugin_kind_t kind;

   /* --- Registration methods --- */

   /* Tools: same as the existing plugin_tool_t registration flow */
   int (*register_tool)(plugin_ctx_t *ctx, const plugin_tool_t *tool);

   /* Hooks: same as the existing plugin_hook_t registration flow */
   int (*register_hook)(plugin_ctx_t *ctx, const plugin_hook_t *hook);

   /* Slash commands: /name triggers command */
   int (*register_slash_command)(plugin_ctx_t *ctx, const plugin_slash_cmd_t *cmd);

   /* CLI subcommands: `aimee <name>` triggers command */
   int (*register_cli_subcommand)(plugin_ctx_t *ctx, const plugin_cli_cmd_t *cmd);

   /* Delegate execution backend: registers a delegate_backend_t */
   int (*register_delegate_backend)(plugin_ctx_t *ctx, delegate_backend_t *backend);

   /* Platform adapter: multi-channel inbound/outbound */
   int (*register_platform_adapter)(plugin_ctx_t *ctx, const plugin_platform_adapter_t *adapter);

   /* Memory provider: pluggable storage backend (runtime ABI; see memory_provider.h).
    * Enforces exclusive-kind rule: bundled + at most one external. */
   int (*register_memory_provider)(plugin_ctx_t *ctx, const memory_provider_t *provider);

   /* Context engine: pluggable context assembly engine (runtime ABI; see context_engine.h). */
   int (*register_context_engine)(plugin_ctx_t *ctx, const context_engine_t *engine);

   /* Model provider: pluggable LLM provider profile */
   int (*register_model_provider)(plugin_ctx_t *ctx, const plugin_model_provider_t *provider);

   /* --- Lifecycle callbacks --- */

   /* Called during startup after all plugins register. Plugins should
    * initialise any runtime state here (DB connections, etc.). */
   int (*on_init)(plugin_ctx_t *ctx);

   /* Called before server exits. Plugins should clean up here. */
   void (*on_shutdown)(plugin_ctx_t *ctx);
};

/* Create a new plugin_ctx_t with all function pointers set to NULL.
 * Caller passes the ctx to the plugin's register() function.
 * Returns NULL on allocation failure. */
plugin_ctx_t *plugin_ctx_create(void);

/* Destroy a plugin_ctx_t. Calls on_shutdown if set, then frees the struct. */
void plugin_ctx_destroy(plugin_ctx_t *ctx);

/* --- Global extension registries ---
 *
 * Populated by plugin_ctx_register_* during plugin register() calls,
 * consumed by the relevant subsystems at runtime.
 * All counts are 0 until plugin_load_and_register() runs. */

/* Delegate backends registered by plugins */
extern delegate_backend_t *plugin_delegate_backends[PLUGIN_MAX_PLUGINS];
extern int plugin_delegate_backend_count;

/* Platform adapters registered by plugins */
extern plugin_platform_adapter_t plugin_platform_adapters[PLUGIN_MAX_PLUGINS];
extern int plugin_platform_adapter_count;

/* Memory providers registered by plugins */
extern plugin_memory_provider_t plugin_memory_providers[PLUGIN_MAX_PLUGINS];
extern int plugin_memory_provider_count;

/* Context engines registered by plugins */
extern plugin_context_engine_t plugin_context_engines[PLUGIN_MAX_PLUGINS];
extern int plugin_context_engine_count;

/* CLI subcommands registered by plugins */
extern plugin_cli_cmd_t plugin_cli_subcommands[PLUGIN_MAX_PLUGINS];
extern int plugin_cli_subcommand_count;

/* Slash commands registered by plugins */
extern plugin_slash_cmd_t plugin_slash_commands[PLUGIN_MAX_PLUGINS];
extern int plugin_slash_command_count;

/* Model providers registered by plugins */
extern plugin_model_provider_t plugin_model_providers[PLUGIN_MAX_PLUGINS];
extern int plugin_model_provider_count;

/* Collect delegate backends from all registered plugins into the
 * global delegate_backend registry. Called at server startup. */
int plugin_collect_delegate_backends(void);

/* --- Plugin registry API --- */

/* Registry path: ~/.config/aimee/plugins/installed.json */
const char *plugin_registry_path(void);

/* Load all plugins from the registry into plugins[].
 * Returns count (0 means empty registry; not an error). */
int plugin_registry_load(plugin_t *plugins, int max);

/* Save plugins[] to the registry. Returns 0 on success. */
int plugin_registry_save(const plugin_t *plugins, int count);

/* Find a plugin by name in the registry. Returns 0 on success, -1 if absent. */
int plugin_registry_get(const char *name, plugin_t *out);

/* Return a malloc'd JSON array describing installed plugins. Caller frees. */
char *plugin_registry_json(void);

/* --- Manifest parsing --- */

/* Parse plugin.json at dir/PLUGIN_MANIFEST_FILE into out.
 * source_path is set to dir.
 * Also supports plugin.yaml (YAML format) if available.
 * Returns 0 on success, -1 on error (sets err_buf). */
int plugin_manifest_parse(const char *dir, plugin_t *out, char *err_buf, size_t err_len);

/* --- Load and register ---
 *
 * Load a plugin from its source_path, create a plugin_ctx_t,
 * and call the plugin's register(ctx) entry point.
 * Returns 0 on success, -1 on error (sets err_buf).
 * The plugin's on_init is called after registration succeeds. */
int plugin_load_and_register(const plugin_t *plugin, char *err_buf, size_t err_len);

/* Load all enabled plugins from the registry and call their register(ctx).
 * Calls plugin_collect_delegate_backends() after registration.
 * Returns 0 on success (even if no plugins), -1 on critical error. */
int plugin_load_all_registered(char *err_buf, size_t err_len);

/* --- Install / enable / disable / remove --- */

/* Install a plugin from source_dir.
 * Reads the manifest, checks for name conflicts, appends to registry.
 * Returns 0 on success, -1 on error. */
int plugin_install(const char *source_dir, char *err_buf, size_t err_len);

/* Enable or disable a plugin by name (0 = disable, 1 = enable). */
int plugin_set_enabled(const char *name, int enabled, char *err_buf, size_t err_len);

/* Remove a plugin by name from the registry. */
int plugin_remove(const char *name, char *err_buf, size_t err_len);

/* --- Project-local plugins --- */

/* Scan workspace_roots for .aimee-plugin/ directories and add them to
 * plugins[] if not already registered. Returns count of newly added. */
int plugin_discover_local(const char **workspace_roots, int root_count, plugin_t *plugins,
                          int *count, int max);

/* --- Hook aggregation --- */

/* Collect all PreToolUse or PostToolUse hook commands from enabled plugins
 * into cmds[]. Returns count of collected commands.
 * event should be "PreToolUse" or "PostToolUse". */
int plugin_collect_hooks(const plugin_t *plugins, int count, const char *event, char cmds[][512],
                         int max);

/* --- Tool access --- */

/* Collect all tools from enabled plugins into out[]. Returns count. */
int plugin_collect_tools(const plugin_t *plugins, int count, plugin_tool_t *out, int max_tools);

/* Check if a tool name conflicts with a built-in. Returns 1 if conflict, 0 otherwise. */
int plugin_tool_conflicts_with_builtin(const char *tool_name);

#endif /* DEC_PLUGIN_H */