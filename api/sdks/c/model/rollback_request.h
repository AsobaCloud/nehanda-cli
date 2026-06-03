/*
 * rollback_request.h
 *
 * 
 */

#ifndef _rollback_request_H_
#define _rollback_request_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct rollback_request_t rollback_request_t;




typedef struct rollback_request_t {
    long *target_release_id; //numeric

    int _library_owned; // Is the library responsible for freeing this object?
} rollback_request_t;

__attribute__((deprecated)) rollback_request_t *rollback_request_create(
    long *target_release_id
);

void rollback_request_free(rollback_request_t *rollback_request);

rollback_request_t *rollback_request_parseFromJSON(cJSON *rollback_requestJSON);

cJSON *rollback_request_convertToJSON(rollback_request_t *rollback_request);

#endif /* _rollback_request_H_ */

