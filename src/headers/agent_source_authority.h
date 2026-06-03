#ifndef DEC_AGENT_SOURCE_AUTHORITY_H
#define DEC_AGENT_SOURCE_AUTHORITY_H 1

#include "aimee.h"
#include "cJSON.h"

void agent_source_add_index_freshness(cJSON *obj, const char *project, const char *file_path);
int agent_source_append_overlay_code_hits(cJSON *arr, const char *query, const char *project,
                                          int max_results);

#endif /* DEC_AGENT_SOURCE_AUTHORITY_H */
