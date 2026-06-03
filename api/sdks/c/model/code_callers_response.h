/*
 * code_callers_response.h
 *
 * 
 */

#ifndef _code_callers_response_H_
#define _code_callers_response_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct code_callers_response_t code_callers_response_t;

#include "code_caller_hit.h"



typedef struct code_callers_response_t {
    char *status; // string
    list_t *hits; //nonprimitive container
    char *next_cursor; // string

    int _library_owned; // Is the library responsible for freeing this object?
} code_callers_response_t;

__attribute__((deprecated)) code_callers_response_t *code_callers_response_create(
    char *status,
    list_t *hits,
    char *next_cursor
);

void code_callers_response_free(code_callers_response_t *code_callers_response);

code_callers_response_t *code_callers_response_parseFromJSON(cJSON *code_callers_responseJSON);

cJSON *code_callers_response_convertToJSON(code_callers_response_t *code_callers_response);

#endif /* _code_callers_response_H_ */

