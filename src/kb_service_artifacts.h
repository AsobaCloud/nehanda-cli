/* kb_service_artifacts.h: prototypes for the artifacts.* dispatch handlers
 * defined in kb_service_artifacts.c.  Included from kb_service.c. */
#ifndef DEC_KB_SERVICE_ARTIFACTS_H
#define DEC_KB_SERVICE_ARTIFACTS_H 1

#include "cJSON.h"

int kb_handle_artifacts_list_proposed(int fd, cJSON *req);
int kb_handle_artifacts_set_state(int fd, cJSON *req);

#endif /* DEC_KB_SERVICE_ARTIFACTS_H */
