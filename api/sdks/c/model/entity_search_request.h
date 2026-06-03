/*
 * entity_search_request.h
 *
 * 
 */

#ifndef _entity_search_request_H_
#define _entity_search_request_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct entity_search_request_t entity_search_request_t;




typedef struct entity_search_request_t {
    char *query; // string
    int *limit; //numeric
    char *cursor; // string

    int _library_owned; // Is the library responsible for freeing this object?
} entity_search_request_t;

__attribute__((deprecated)) entity_search_request_t *entity_search_request_create(
    char *query,
    int *limit,
    char *cursor
);

void entity_search_request_free(entity_search_request_t *entity_search_request);

entity_search_request_t *entity_search_request_parseFromJSON(cJSON *entity_search_requestJSON);

cJSON *entity_search_request_convertToJSON(entity_search_request_t *entity_search_request);

#endif /* _entity_search_request_H_ */

