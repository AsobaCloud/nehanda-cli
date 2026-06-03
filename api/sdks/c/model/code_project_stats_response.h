/*
 * code_project_stats_response.h
 *
 * 
 */

#ifndef _code_project_stats_response_H_
#define _code_project_stats_response_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct code_project_stats_response_t code_project_stats_response_t;

#include "code_project_language.h"



typedef struct code_project_stats_response_t {
    char *status; // string
    char *project; // string
    int *files; //numeric
    int *definitions; //numeric
    list_t *langs; //nonprimitive container

    int _library_owned; // Is the library responsible for freeing this object?
} code_project_stats_response_t;

__attribute__((deprecated)) code_project_stats_response_t *code_project_stats_response_create(
    char *status,
    char *project,
    int *files,
    int *definitions,
    list_t *langs
);

void code_project_stats_response_free(code_project_stats_response_t *code_project_stats_response);

code_project_stats_response_t *code_project_stats_response_parseFromJSON(cJSON *code_project_stats_responseJSON);

cJSON *code_project_stats_response_convertToJSON(code_project_stats_response_t *code_project_stats_response);

#endif /* _code_project_stats_response_H_ */

