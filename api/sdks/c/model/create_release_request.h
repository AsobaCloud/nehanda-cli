/*
 * create_release_request.h
 *
 * 
 */

#ifndef _create_release_request_H_
#define _create_release_request_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct create_release_request_t create_release_request_t;




typedef struct create_release_request_t {
    char *name; // string

    int _library_owned; // Is the library responsible for freeing this object?
} create_release_request_t;

__attribute__((deprecated)) create_release_request_t *create_release_request_create(
    char *name
);

void create_release_request_free(create_release_request_t *create_release_request);

create_release_request_t *create_release_request_parseFromJSON(cJSON *create_release_requestJSON);

cJSON *create_release_request_convertToJSON(create_release_request_t *create_release_request);

#endif /* _create_release_request_H_ */

