/*
 * code_definition.h
 *
 * 
 */

#ifndef _code_definition_H_
#define _code_definition_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct code_definition_t code_definition_t;




typedef struct code_definition_t {
    char *name; // string
    char *kind; // string
    int *line; //numeric

    int _library_owned; // Is the library responsible for freeing this object?
} code_definition_t;

__attribute__((deprecated)) code_definition_t *code_definition_create(
    char *name,
    char *kind,
    int *line
);

void code_definition_free(code_definition_t *code_definition);

code_definition_t *code_definition_parseFromJSON(cJSON *code_definitionJSON);

cJSON *code_definition_convertToJSON(code_definition_t *code_definition);

#endif /* _code_definition_H_ */

