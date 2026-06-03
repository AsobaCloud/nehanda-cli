/* cmd_memory_op.c: typed mutation verbs for `aimee memory op`.
 *
 * Six stable verbs matching the memory-public-contract typed mutation surface:
 *   store    — write a new memory (delegates to mem_store)
 *   update   — replace content in place
 *   supersede — replace with lineage (delegates to mem_supersede)
 *   forget   — retire a memory (delegates to mem_delete)
 *   affirm   — positive reinforcement (increment use_count)
 *   reject   — negative reinforcement (reduce confidence)
 *
 * Legacy subcommands (store, delete, supersede) remain for existing callers.
 * `memory op` is the stable typed surface for integrations. */

#include "aimee.h"
#include "cmd_memory_internal.h"
#include "kb_client.h"

/* --- typed verb dispatch ------------------------------------------- */

static void op_store(app_ctx_t *ctx, int argc, char **argv)
{
   mem_store(ctx, argc, argv);
}

static void op_update(app_ctx_t *ctx, int argc, char **argv)
{
   if (argc < 2)
      fatal("memory op update requires <id> <content>");
   int64_t id = atoll(argv[0]);
   const char *content = argv[1];
   if (kb_client_memory_update(id, content) != 0)
      fatal("failed to update memory %lld", (long long)id);
   if (ctx->json_output)
      emit_ok_ctx(ctx->json_fields, ctx->response_profile);
}

static void op_supersede(app_ctx_t *ctx, int argc, char **argv)
{
   mem_supersede(ctx, argc, argv);
}

static void op_forget(app_ctx_t *ctx, int argc, char **argv)
{
   mem_delete(ctx, argc, argv);
}

static void op_affirm(app_ctx_t *ctx, int argc, char **argv)
{
   if (argc < 1)
      fatal("memory op affirm requires <id>");
   int64_t id = atoll(argv[0]);
   if (kb_client_memory_touch(id) != 0)
      fatal("failed to affirm memory %lld", (long long)id);
   if (ctx->json_output)
      emit_ok_ctx(ctx->json_fields, ctx->response_profile);
}

static void op_reject(app_ctx_t *ctx, int argc, char **argv)
{
   if (argc < 1)
      fatal("memory op reject requires <id>");

   opt_parsed_t opts;
   opt_parse(argc, argv, NULL, &opts);
   const char *id_str = opt_pos(&opts, 0);
   if (!id_str)
      fatal("memory op reject requires <id>");
   int64_t id = atoll(id_str);
   const char *reason = opt_get(&opts, "reason");

   if (kb_client_memory_reject(id, reason) != 0)
      fatal("failed to reject memory %lld", (long long)id);
   if (ctx->json_output)
      emit_ok_ctx(ctx->json_fields, ctx->response_profile);
}

/* --- mem_op entry point ------------------------------------------- */

void mem_op(app_ctx_t *ctx, int argc, char **argv)
{
   if (argc < 1)
   {
      fprintf(stderr, "Usage: aimee memory op <verb> [args]\n"
                      "Verbs: store | update | supersede | forget | affirm | reject\n");
      exit(1);
   }

   const char *verb = argv[0];
   argc--;
   argv++;

   if (strcmp(verb, "store") == 0)
      op_store(ctx, argc, argv);
   else if (strcmp(verb, "update") == 0)
      op_update(ctx, argc, argv);
   else if (strcmp(verb, "supersede") == 0)
      op_supersede(ctx, argc, argv);
   else if (strcmp(verb, "forget") == 0)
      op_forget(ctx, argc, argv);
   else if (strcmp(verb, "affirm") == 0)
      op_affirm(ctx, argc, argv);
   else if (strcmp(verb, "reject") == 0)
      op_reject(ctx, argc, argv);
   else
      fatal("unknown memory op verb: %s", verb);
}
