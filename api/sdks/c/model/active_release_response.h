/*
 * active_release_response.h
 *
 * 
 */

#ifndef _active_release_response_H_
#define _active_release_response_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct active_release_response_t active_release_response_t;




typedef struct active_release_response_t {
    long *release_id; //numeric
    char *name; // string
    char *state; // string
    char *promoted_at; // string

    int _library_owned; // Is the library responsible for freeing this object?
} active_release_response_t;

__attribute__((deprecated)) active_release_response_t *active_release_response_create(
    long *release_id,
    char *name,
    char *state,
    char *promoted_at
);

void active_release_response_free(active_release_response_t *active_release_response);

active_release_response_t *active_release_response_parseFromJSON(cJSON *active_release_responseJSON);

cJSON *active_release_response_convertToJSON(active_release_response_t *active_release_response);

#endif /* _active_release_response_H_ */

