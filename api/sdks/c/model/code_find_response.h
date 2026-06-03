/*
 * code_find_response.h
 *
 * 
 */

#ifndef _code_find_response_H_
#define _code_find_response_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct code_find_response_t code_find_response_t;

#include "code_find_hit.h"



typedef struct code_find_response_t {
    list_t *hits; //nonprimitive container
    char *next_cursor; // string

    int _library_owned; // Is the library responsible for freeing this object?
} code_find_response_t;

__attribute__((deprecated)) code_find_response_t *code_find_response_create(
    list_t *hits,
    char *next_cursor
);

void code_find_response_free(code_find_response_t *code_find_response);

code_find_response_t *code_find_response_parseFromJSON(cJSON *code_find_responseJSON);

cJSON *code_find_response_convertToJSON(code_find_response_t *code_find_response);

#endif /* _code_find_response_H_ */

