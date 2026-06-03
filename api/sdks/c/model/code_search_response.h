/*
 * code_search_response.h
 *
 * 
 */

#ifndef _code_search_response_H_
#define _code_search_response_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct code_search_response_t code_search_response_t;

#include "code_search_hit.h"



typedef struct code_search_response_t {
    char *status; // string
    list_t *hits; //nonprimitive container
    char *next_cursor; // string

    int _library_owned; // Is the library responsible for freeing this object?
} code_search_response_t;

__attribute__((deprecated)) code_search_response_t *code_search_response_create(
    char *status,
    list_t *hits,
    char *next_cursor
);

void code_search_response_free(code_search_response_t *code_search_response);

code_search_response_t *code_search_response_parseFromJSON(cJSON *code_search_responseJSON);

cJSON *code_search_response_convertToJSON(code_search_response_t *code_search_response);

#endif /* _code_search_response_H_ */

