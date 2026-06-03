/*
 * maintenance_repair_request.h
 *
 * 
 */

#ifndef _maintenance_repair_request_H_
#define _maintenance_repair_request_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct maintenance_repair_request_t maintenance_repair_request_t;




typedef struct maintenance_repair_request_t {
    char *path; // string
    char *project; // string
    char *embedding_command; // string

    int _library_owned; // Is the library responsible for freeing this object?
} maintenance_repair_request_t;

__attribute__((deprecated)) maintenance_repair_request_t *maintenance_repair_request_create(
    char *path,
    char *project,
    char *embedding_command
);

void maintenance_repair_request_free(maintenance_repair_request_t *maintenance_repair_request);

maintenance_repair_request_t *maintenance_repair_request_parseFromJSON(cJSON *maintenance_repair_requestJSON);

cJSON *maintenance_repair_request_convertToJSON(maintenance_repair_request_t *maintenance_repair_request);

#endif /* _maintenance_repair_request_H_ */

