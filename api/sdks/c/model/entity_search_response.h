/*
 * entity_search_response.h
 *
 * 
 */

#ifndef _entity_search_response_H_
#define _entity_search_response_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct entity_search_response_t entity_search_response_t;

#include "entity_search_response_entities_inner.h"



typedef struct entity_search_response_t {
    list_t *entities; //nonprimitive container
    char *next_cursor; // string

    int _library_owned; // Is the library responsible for freeing this object?
} entity_search_response_t;

__attribute__((deprecated)) entity_search_response_t *entity_search_response_create(
    list_t *entities,
    char *next_cursor
);

void entity_search_response_free(entity_search_response_t *entity_search_response);

entity_search_response_t *entity_search_response_parseFromJSON(cJSON *entity_search_responseJSON);

cJSON *entity_search_response_convertToJSON(entity_search_response_t *entity_search_response);

#endif /* _entity_search_response_H_ */

