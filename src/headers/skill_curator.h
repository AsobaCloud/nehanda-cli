/* skill_curator.h: weekly skill consolidation curator trigger. */
#ifndef DEC_SKILL_CURATOR_H
#define DEC_SKILL_CURATOR_H 1

#include "config.h"

/* Launch a background curator delegate if the weekly interval has elapsed
 * and the system has been idle for min_idle_minutes.
 * Returns 1 if launched, 0 if skipped, -1 on error. */
int skill_curator_nudge(const config_t *cfg);

#endif
