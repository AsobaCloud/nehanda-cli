/* trajectory.h: export replayable trajectories from DB1 interaction events. */
#ifndef DEC_TRAJECTORY_H
#define DEC_TRAJECTORY_H 1

#include "aimee.h"
#include "agent_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

   typedef struct
   {
      int compress;
      int redact;
      int max_tool_result_bytes;
      int redaction_buf_bytes;
   } trajectory_opts_t;

   typedef struct
   {
      const char *tasks_path;
      const char *toolset_dist;
      const char *out_dir;
      trajectory_opts_t export_opts;
   } trajectory_batch_opts_t;

   int trajectory_export(const char *selector, const trajectory_opts_t *opts, char **out_json);
   int trajectory_batch_run(agent_config_t *cfg, const trajectory_batch_opts_t *opts,
                            char **summary_json);
   const char *trajectory_toolset_for_index(const char *dist, int index);

#ifdef __cplusplus
}
#endif

#endif /* DEC_TRAJECTORY_H */
