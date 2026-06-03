/* kb_service_roadmap.h: prototypes for the roadmap.* dispatch handlers
 * defined in kb_service_roadmap.c.  Included from kb_service.c. */
#ifndef DEC_KB_SERVICE_ROADMAP_H
#define DEC_KB_SERVICE_ROADMAP_H 1

#include "cJSON.h"

int kb_handle_roadmap_create_from_decomposition(int fd, cJSON *req);
int kb_handle_roadmap_validate(int fd, cJSON *req);
int kb_handle_roadmap_show(int fd, cJSON *req);
int kb_handle_roadmap_list(int fd, cJSON *req);
int kb_handle_roadmap_rebuild(int fd, cJSON *req);
int kb_handle_roadmap_report(int fd, cJSON *req);

#endif /* DEC_KB_SERVICE_ROADMAP_H */