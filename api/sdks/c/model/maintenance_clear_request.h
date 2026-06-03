/*
 * maintenance_clear_request.h
 *
 * 
 */

#ifndef _maintenance_clear_request_H_
#define _maintenance_clear_request_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct maintenance_clear_request_t maintenance_clear_request_t;




typedef struct maintenance_clear_request_t {
    char *project; // string

    int _library_owned; // Is the library responsible for freeing this object?
} maintenance_clear_request_t;

__attribute__((deprecated)) maintenance_clear_request_t *maintenance_clear_request_create(
    char *project
);

void maintenance_clear_request_free(maintenance_clear_request_t *maintenance_clear_request);

maintenance_clear_request_t *maintenance_clear_request_parseFromJSON(cJSON *maintenance_clear_requestJSON);

cJSON *maintenance_clear_request_convertToJSON(maintenance_clear_request_t *maintenance_clear_request);

#endif /* _maintenance_clear_request_H_ */

