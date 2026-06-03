/* plugin_ctx.h: plugin_ctx_t helpers — registration API for plugin contributors.
 *
 * Part of the pluggable context engine surface described in
 * docs/proposals/pending/plugin-extension-surface-and-context-engine.md.
 *
 * plugin_ctx_t is defined in src/headers/plugin.h as an opaque struct with
 * function pointers. This file provides simple wrappers around the ctx->register_*
 * function pointers for plugin contributors.
 */
#ifndef DEC_PLUGIN_CTX_H
#define DEC_PLUGIN_CTX_H 1

#include "plugin.h"
#include "memory_provider.h"
#include "context_engine.h"

#ifdef __cplusplus
extern "C"
{
#endif

   /* --- Extended lifecycle helpers ---
    *
    * plugin_ctx_t *ctx = plugin_ctx_create_ex(plugin_name);
    * ctx->register_tool(ctx, &tool);  ...
    * plugin_ctx_destroy_ex(ctx);
    *
    * plugin_ctx_create_ex() calls plugin_ctx_create() (from plugin.h) and
    * stores the name for plugin_ctx_name(). plugin_ctx_destroy_ex() forwards
    * to plugin_ctx_destroy().
    */

   /* Like plugin_ctx_create() but stores the name for plugin_ctx_name(). */
   plugin_ctx_t *plugin_ctx_create_ex(const char *plugin_name);

   /* Forward to plugin_ctx_destroy(). */
   void plugin_ctx_destroy_ex(plugin_ctx_t *ctx);

   /* Accessor: plugin name (never NULL after create_ex). */
   const char *plugin_ctx_name(const plugin_ctx_t *ctx);

   /* --- Accessors delegating to the struct from plugin.h --- */

   const char *plugin_ctx_source_path(const plugin_ctx_t *ctx);
   const char *plugin_ctx_kind(const plugin_ctx_t *ctx);
   void plugin_ctx_set_source_path(plugin_ctx_t *ctx, const char *path);
   void plugin_ctx_set_kind(plugin_ctx_t *ctx, const char *kind);

   /* --- Shortcut registration wrappers ---
    *
    * These call ctx->register_*() with a helpful error log on failure.
    * Plugins can use either these helpers or call ctx->register_*() directly.
    */

   int plugin_ctx_register_tool(plugin_ctx_t *ctx, const plugin_tool_t *tool);
   int plugin_ctx_register_hook(plugin_ctx_t *ctx, const plugin_hook_t *hook);
   int plugin_ctx_register_slash_command(plugin_ctx_t *ctx, const plugin_slash_cmd_t *cmd);
   int plugin_ctx_register_cli_subcommand(plugin_ctx_t *ctx, const plugin_cli_cmd_t *cmd);
   int plugin_ctx_register_memory_provider(plugin_ctx_t *ctx, const memory_provider_t *provider);
   int plugin_ctx_register_context_engine(plugin_ctx_t *ctx, const context_engine_t *engine);

#ifdef __cplusplus
}
#endif

#endif /* DEC_PLUGIN_CTX_H */
