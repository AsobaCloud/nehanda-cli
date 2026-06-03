/*
 * code_projects_response.h
 *
 * 
 */

#ifndef _code_projects_response_H_
#define _code_projects_response_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct code_projects_response_t code_projects_response_t;

#include "code_project.h"



typedef struct code_projects_response_t {
    char *status; // string
    list_t *projects; //nonprimitive container
    char *next_cursor; // string

    int _library_owned; // Is the library responsible for freeing this object?
} code_projects_response_t;

__attribute__((deprecated)) code_projects_response_t *code_projects_response_create(
    char *status,
    list_t *projects,
    char *next_cursor
);

void code_projects_response_free(code_projects_response_t *code_projects_response);

code_projects_response_t *code_projects_response_parseFromJSON(cJSON *code_projects_responseJSON);

cJSON *code_projects_response_convertToJSON(code_projects_response_t *code_projects_response);

#endif /* _code_projects_response_H_ */

