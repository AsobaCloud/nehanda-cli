/* server_compute_concurrency.c: split from server_compute.c into a real translation unit
 * (was server_compute_concurrency.inc, textually included only to stay under the
 * line-check ceiling). Cross-TU declarations live in the module header. */
#include "server_compute_internal.h"
#include "aimee.h"
#include "json_fluent.h" /* jo_ok */
#include "db1.h"
#include "server_delegate_monitor.h" /* delegate heartbeat begin/end (keep slow delegates alive) */
#include "server_compute_impl.h"
#include "agent_config.h"
#include "gateway_policy.h"
#include "presence.h"
#include "compute_pool.h"
#include "agent.h"
#include "agent_coord.h"
#include "cmd_agent_delegate_impl.h"
#include "compute_concurrency.h"
#include "config.h"
#include "token_tracker.h"
#include "delegate_credential_retry.h"
#include "delegate_launch.h"
#include "delegate_source_authority.h"
#include "agent_source_authority.h" /* TLS source-authority context (race-free in-process) */
#include "server_coord_dispatcher.h"
#include "delegate_credentials.h"
#include "vault_service.h" /* WP-C.1 vault-first credential resolution */
#include <openssl/crypto.h>
#include "delegate_economics.h"
#include "delegate_run_phases.h"
#include "db1/delegate_learning.h"
#include "kb_client.h"
#include "kb_bandit.h"
#include "db1/interaction_events.h"
#include "delegate_role.h"
#include "delegate_ensemble.h"
#include "evidence_replay.h"
#include "guardrails.h"
#include "liveness.h"
#include "log.h"
#include "model_registry.h"
#include "openai_runs_store.h"
#include "platform_process.h"
#include "prompts.h"
#include "persona.h"
#include "server_http.h"
#include "provider_catalog.h"
#include "role_templates.h"
#include "workspace.h"
#include "workspace_provider.h"
#include "workspace_turn.h"
#include "cJSON.h"
#include <ctype.h>
#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

concurrency_mgr_t g_concurrency_mgr;
static pthread_mutex_t g_concurrency_reload_lock = PTHREAD_MUTEX_INITIALIZER;
static int g_concurrency_initialized = 0;
static long long g_concurrency_config_mtime_ns = -2;
static long long g_concurrency_agents_mtime_ns = -2;

typedef struct
{
   int default_limit;
   concurrency_entry_t per_model[CONCURRENCY_MAX_ENTRIES];
   int per_model_count;
   concurrency_entry_t per_provider[CONCURRENCY_MAX_ENTRIES];
   int per_provider_count;
} delegate_concurrency_limits_t;

concurrency_route_key_t concurrency_key(const char *via_name, const agent_t *target_agent,
                                        char *tier_key, size_t tier_key_len)
{
   if (!via_name || !via_name[0])
   {
      snprintf(tier_key, tier_key_len, "tier:%d", target_agent ? target_agent->cost_tier : 0);
      return (concurrency_route_key_t){.model = tier_key, .provider = ""};
   }
   return (concurrency_route_key_t){
       .model = (target_agent && target_agent->model[0]) ? target_agent->model : "",
       .provider = (target_agent && target_agent->provider[0]) ? target_agent->provider : "",
   };
}

static long long concurrency_path_mtime_ns(const char *path)
{
   struct stat st;
   if (!path || stat(path, &st) != 0)
      return -1;

#if defined(__APPLE__)
   return ((long long)st.st_mtimespec.tv_sec * 1000000000LL) + st.st_mtimespec.tv_nsec;
#elif defined(_WIN32) || defined(_WIN64)
   return (long long)st.st_mtime * 1000000000LL;
#elif defined(__linux__)
   return ((long long)st.st_mtim.tv_sec * 1000000000LL) + st.st_mtim.tv_nsec;
#else
   return (long long)st.st_mtime * 1000000000LL;
#endif
}

static void concurrency_limits_load(delegate_concurrency_limits_t *limits)
{
   memset(limits, 0, sizeof(*limits));

   config_t cfg;
   config_load(&cfg);

   limits->default_limit =
       cfg.concurrency_default > 0 ? cfg.concurrency_default : CONCURRENCY_DEFAULT_LIMIT;

   /* Map config entries to concurrency_entry_t arrays. */
   for (int i = 0;
        i < cfg.concurrency_per_model_count && limits->per_model_count < CONCURRENCY_MAX_ENTRIES;
        i++)
   {
      snprintf(limits->per_model[limits->per_model_count].key,
               sizeof(limits->per_model[limits->per_model_count].key), "%s",
               cfg.concurrency_per_model[i].key);
      limits->per_model[limits->per_model_count].limit = cfg.concurrency_per_model[i].limit;
      limits->per_model_count++;
   }

   for (int i = 0; i < cfg.concurrency_per_provider_count &&
                   limits->per_provider_count < CONCURRENCY_MAX_ENTRIES;
        i++)
   {
      snprintf(limits->per_provider[limits->per_provider_count].key,
               sizeof(limits->per_provider[limits->per_provider_count].key), "%s",
               cfg.concurrency_per_provider[i].key);
      limits->per_provider[limits->per_provider_count].limit =
          cfg.concurrency_per_provider[i].limit;
      limits->per_provider_count++;
   }

   /* Seed the catalog and inject locality-inferred per-model limits.
    * Config-explicit entries already occupy the front of per_model[], so
    * limit_for() will match them first; inferred entries are appended only
    * for models without an explicit override. */
   agent_config_t acfg;
   if (agent_load_config(&acfg) == 0)
   {
      provider_catalog_init(acfg.agents, acfg.agent_count);

      for (int i = 0; i < acfg.agent_count && limits->per_model_count < CONCURRENCY_MAX_ENTRIES;
           i++)
      {
         agent_t *ag = &acfg.agents[i];
         if (!ag->enabled || !ag->model[0])
            continue;
         int inferred = provider_catalog_inferred_concurrency(ag->name);
         if (!inferred)
            continue;

         int has_override = 0;
         for (int j = 0; j < cfg.concurrency_per_model_count; j++)
         {
            if (strcmp(cfg.concurrency_per_model[j].key, ag->model) == 0)
            {
               has_override = 1;
               break;
            }
         }
         if (has_override)
            continue;

         snprintf(limits->per_model[limits->per_model_count].key,
                  sizeof(limits->per_model[limits->per_model_count].key), "%s", ag->model);
         limits->per_model[limits->per_model_count].limit = inferred;
         limits->per_model_count++;
         aimee_log(LOG_DEBUG, "concurrency", "inferred limit %d for model '%s' (agent '%s', %s)",
                   inferred, ag->model, ag->name,
                   provider_locality_label(provider_catalog_get_locality(ag->name)));
      }

      /* Tier-based pool limits: each tier maps to a single shared queue.
       * Limit = sum of max_parallel across all enabled agents at that tier. */
      for (int _t = 0; _t <= 3 && limits->per_model_count < CONCURRENCY_MAX_ENTRIES; _t++)
      {
         int _lim = 0;
         for (int i = 0; i < acfg.agent_count; i++)
         {
            agent_t *ag = &acfg.agents[i];
            if (!ag->enabled || ag->cost_tier != _t)
               continue;
            _lim += ag->max_parallel > 0 ? ag->max_parallel : AGENT_DEFAULT_MAX_PARALLEL;
         }
         if (_lim <= 0)
            continue;
         char _tk[16];
         snprintf(_tk, sizeof(_tk), "tier:%d", _t);
         snprintf(limits->per_model[limits->per_model_count].key,
                  sizeof(limits->per_model[limits->per_model_count].key), "%s", _tk);
         limits->per_model[limits->per_model_count].limit = _lim;
         limits->per_model_count++;
         aimee_log(LOG_DEBUG, "concurrency", "tier:%d pool limit %d", _t, _lim);
      }
   }
}

void concurrency_ensure_current(void)
{
   long long cfg_mtime = concurrency_path_mtime_ns(config_default_path());
   long long agents_mtime = concurrency_path_mtime_ns(agent_config_path());

   pthread_mutex_lock(&g_concurrency_reload_lock);

   int changed = !g_concurrency_initialized || cfg_mtime != g_concurrency_config_mtime_ns ||
                 agents_mtime != g_concurrency_agents_mtime_ns;
   if (changed)
   {
      delegate_concurrency_limits_t limits;
      concurrency_limits_load(&limits);

      if (!g_concurrency_initialized)
      {
         concurrency_mgr_init(&g_concurrency_mgr, limits.default_limit, limits.per_model,
                              limits.per_model_count, limits.per_provider,
                              limits.per_provider_count);
         g_concurrency_initialized = 1;
      }
      else
      {
         concurrency_mgr_update_limits(&g_concurrency_mgr, limits.default_limit, limits.per_model,
                                       limits.per_model_count, limits.per_provider,
                                       limits.per_provider_count);
      }

      g_concurrency_config_mtime_ns = cfg_mtime;
      g_concurrency_agents_mtime_ns = agents_mtime;
   }

   pthread_mutex_unlock(&g_concurrency_reload_lock);
}

void concurrency_maybe_preempt_delegate(const char *model, const char *provider,
                                        int requester_priority, const char *requester_id)
{
   config_t cfg;
   if (config_load(&cfg) != 0 || !cfg.concurrency_preempt_enabled || !model || !model[0])
      return;

   char victim[CONCURRENCY_OWNER_LEN] = "";
   int victim_priority = 0;
   if (!concurrency_preempt_candidate_for_key(
           &g_concurrency_mgr, model, provider, requester_priority,
           cfg.concurrency_preempt_single_slot_only, victim, sizeof(victim), &victim_priority))
      return;
   if (!victim[0] || (requester_id && strcmp(victim, requester_id) == 0))
      return;
   int changed = db1_delegation_spawn_preempt(victim);
   if (changed > 0)
      aimee_log(LOG_INFO, "concurrency", "preempted delegate %s priority %d for requester %s/%d",
                victim, victim_priority, requester_id ? requester_id : "", requester_priority);
}
