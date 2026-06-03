/*
 * code_structure_response.h
 *
 * 
 */

#ifndef _code_structure_response_H_
#define _code_structure_response_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct code_structure_response_t code_structure_response_t;

#include "code_definition.h"



typedef struct code_structure_response_t {
    char *status; // string
    list_t *definitions; //nonprimitive container

    int _library_owned; // Is the library responsible for freeing this object?
} code_structure_response_t;

__attribute__((deprecated)) code_structure_response_t *code_structure_response_create(
    char *status,
    list_t *definitions
);

void code_structure_response_free(code_structure_response_t *code_structure_response);

code_structure_response_t *code_structure_response_parseFromJSON(cJSON *code_structure_responseJSON);

cJSON *code_structure_response_convertToJSON(code_structure_response_t *code_structure_response);

#endif /* _code_structure_response_H_ */

