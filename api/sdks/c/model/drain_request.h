/*
 * drain_request.h
 *
 * 
 */

#ifndef _drain_request_H_
#define _drain_request_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct drain_request_t drain_request_t;




typedef struct drain_request_t {
    char *embedding_command; // string
    int *timeout; //numeric

    int _library_owned; // Is the library responsible for freeing this object?
} drain_request_t;

__attribute__((deprecated)) drain_request_t *drain_request_create(
    char *embedding_command,
    int *timeout
);

void drain_request_free(drain_request_t *drain_request);

drain_request_t *drain_request_parseFromJSON(cJSON *drain_requestJSON);

cJSON *drain_request_convertToJSON(drain_request_t *drain_request);

#endif /* _drain_request_H_ */

