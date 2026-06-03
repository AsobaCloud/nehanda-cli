/* plugin_ctx.c: plugin_ctx_t helpers — lifecycle and registration wrappers.
 *
 * Part of the pluggable context engine surface described in
 * docs/proposals/pending/plugin-extension-surface-and-context-engine.md.
 */
#include "headers/plugin_ctx.h"
#include "headers/log.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string.h>

/* Store plugin name for plugin_ctx_name(). */
static char g_plugin_name[64] = {0};

/* --- Lifecycle helpers --- */

plugin_ctx_t *plugin_ctx_create_ex(const char *plugin_name)
{
   plugin_ctx_t *ctx = plugin_ctx_create();
   if (!ctx)
      return NULL;

   if (plugin_name)
      snprintf(g_plugin_name, sizeof(g_plugin_name), "%s", plugin_name);

   return ctx;
}

void plugin_ctx_destroy_ex(plugin_ctx_t *ctx)
{
   plugin_ctx_destroy(ctx);
}

/* --- Accessors --- */

const char *plugin_ctx_name(const plugin_ctx_t *ctx)
{
   (void)ctx;
   return g_plugin_name[0] ? g_plugin_name : NULL;
}

const char *plugin_ctx_source_path(const plugin_ctx_t *ctx)
{
   return ctx ? ctx->source_path : NULL;
}

const char *plugin_ctx_kind(const plugin_ctx_t *ctx)
{
   if (!ctx)
      return NULL;
   switch (ctx->kind)
   {
   case PLUGIN_KIND_STANDALONE:
      return "standalone";
   case PLUGIN_KIND_BACKEND:
      return "backend";
   case PLUGIN_KIND_EXCLUSIVE:
      return "exclusive";
   case PLUGIN_KIND_PLATFORM:
      return "platform";
   default:
      return "unknown";
   }
}

void plugin_ctx_set_source_path(plugin_ctx_t *ctx, const char *path)
{
   if (!ctx || !path)
      return;

   size_t len = strlen(path);
   if (len >= sizeof(ctx->source_path))
      len = sizeof(ctx->source_path) - 1;

   memcpy((char *)ctx->source_path, path, len);
   ((char *)ctx->source_path)[len] = '\0';
}

void plugin_ctx_set_kind(plugin_ctx_t *ctx, const char *kind)
{
   if (!ctx || !kind)
      return;

   if (strcmp(kind, "standalone") == 0)
      ctx->kind = PLUGIN_KIND_STANDALONE;
   else if (strcmp(kind, "backend") == 0)
      ctx->kind = PLUGIN_KIND_BACKEND;
   else if (strcmp(kind, "exclusive") == 0)
      ctx->kind = PLUGIN_KIND_EXCLUSIVE;
   else if (strcmp(kind, "platform") == 0)
      ctx->kind = PLUGIN_KIND_PLATFORM;
}

/* --- Shortcut registration wrappers --- */

int plugin_ctx_register_tool(plugin_ctx_t *ctx, const plugin_tool_t *tool)
{
   if (!ctx || !ctx->register_tool)
   {
      fprintf(stderr, "plugin_ctx: no ctx or register_tool\n");
      return -1;
   }
   return ctx->register_tool(ctx, tool);
}

int plugin_ctx_register_hook(plugin_ctx_t *ctx, const plugin_hook_t *hook)
{
   if (!ctx || !ctx->register_hook)
   {
      fprintf(stderr, "plugin_ctx: no ctx or register_hook\n");
      return -1;
   }
   return ctx->register_hook(ctx, hook);
}

int plugin_ctx_register_slash_command(plugin_ctx_t *ctx, const plugin_slash_cmd_t *cmd)
{
   if (!ctx || !ctx->register_slash_command)
   {
      fprintf(stderr, "plugin_ctx: no ctx or register_slash_command\n");
      return -1;
   }
   return ctx->register_slash_command(ctx, cmd);
}

int plugin_ctx_register_cli_subcommand(plugin_ctx_t *ctx, const plugin_cli_cmd_t *cmd)
{
   if (!ctx || !ctx->register_cli_subcommand)
   {
      fprintf(stderr, "plugin_ctx: no ctx or register_cli_subcommand\n");
      return -1;
   }
   return ctx->register_cli_subcommand(ctx, cmd);
}

int plugin_ctx_register_memory_provider(plugin_ctx_t *ctx, const memory_provider_t *provider)
{
   if (!ctx || !ctx->register_memory_provider)
   {
      fprintf(stderr, "plugin_ctx: no ctx or register_memory_provider\n");
      return -1;
   }
   return ctx->register_memory_provider(ctx, provider);
}

int plugin_ctx_register_context_engine(plugin_ctx_t *ctx, const context_engine_t *engine)
{
   if (!ctx || !ctx->register_context_engine)
   {
      fprintf(stderr, "plugin_ctx: no ctx or register_context_engine\n");
      return -1;
   }
   return ctx->register_context_engine(ctx, engine);
}
