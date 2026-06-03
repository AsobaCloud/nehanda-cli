/*
 * entity_search_response_entities_inner.h
 *
 * 
 */

#ifndef _entity_search_response_entities_inner_H_
#define _entity_search_response_entities_inner_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct entity_search_response_entities_inner_t entity_search_response_entities_inner_t;




typedef struct entity_search_response_entities_inner_t {
    char *entity; // string
    char *kind; // string
    char *summary; // string
    double *score; //numeric

    int _library_owned; // Is the library responsible for freeing this object?
} entity_search_response_entities_inner_t;

__attribute__((deprecated)) entity_search_response_entities_inner_t *entity_search_response_entities_inner_create(
    char *entity,
    char *kind,
    char *summary,
    double *score
);

void entity_search_response_entities_inner_free(entity_search_response_entities_inner_t *entity_search_response_entities_inner);

entity_search_response_entities_inner_t *entity_search_response_entities_inner_parseFromJSON(cJSON *entity_search_response_entities_innerJSON);

cJSON *entity_search_response_entities_inner_convertToJSON(entity_search_response_entities_inner_t *entity_search_response_entities_inner);

#endif /* _entity_search_response_entities_inner_H_ */

