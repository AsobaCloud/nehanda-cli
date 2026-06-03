/*
 * maintenance_clear_response.h
 *
 * 
 */

#ifndef _maintenance_clear_response_H_
#define _maintenance_clear_response_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct maintenance_clear_response_t maintenance_clear_response_t;




typedef struct maintenance_clear_response_t {
    char *status; // string
    char *project; // string
    int *chunks_deleted; //numeric

    int _library_owned; // Is the library responsible for freeing this object?
} maintenance_clear_response_t;

__attribute__((deprecated)) maintenance_clear_response_t *maintenance_clear_response_create(
    char *status,
    char *project,
    int *chunks_deleted
);

void maintenance_clear_response_free(maintenance_clear_response_t *maintenance_clear_response);

maintenance_clear_response_t *maintenance_clear_response_parseFromJSON(cJSON *maintenance_clear_responseJSON);

cJSON *maintenance_clear_response_convertToJSON(maintenance_clear_response_t *maintenance_clear_response);

#endif /* _maintenance_clear_response_H_ */

