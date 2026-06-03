/*
 * entity_profile_response.h
 *
 * 
 */

#ifndef _entity_profile_response_H_
#define _entity_profile_response_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct entity_profile_response_t entity_profile_response_t;




typedef struct entity_profile_response_t {
    char *entity; // string
    char *kind; // string
    char *summary; // string
    list_t *facts; //primitive container
    list_t *tags; //primitive container
    char *updated_at; // string

    int _library_owned; // Is the library responsible for freeing this object?
} entity_profile_response_t;

__attribute__((deprecated)) entity_profile_response_t *entity_profile_response_create(
    char *entity,
    char *kind,
    char *summary,
    list_t *facts,
    list_t *tags,
    char *updated_at
);

void entity_profile_response_free(entity_profile_response_t *entity_profile_response);

entity_profile_response_t *entity_profile_response_parseFromJSON(cJSON *entity_profile_responseJSON);

cJSON *entity_profile_response_convertToJSON(entity_profile_response_t *entity_profile_response);

#endif /* _entity_profile_response_H_ */

