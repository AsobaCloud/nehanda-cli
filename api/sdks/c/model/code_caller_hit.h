/*
 * code_caller_hit.h
 *
 * 
 */

#ifndef _code_caller_hit_H_
#define _code_caller_hit_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct code_caller_hit_t code_caller_hit_t;




typedef struct code_caller_hit_t {
    char *project; // string
    char *file_path; // string
    char *caller; // string
    int *line; //numeric

    int _library_owned; // Is the library responsible for freeing this object?
} code_caller_hit_t;

__attribute__((deprecated)) code_caller_hit_t *code_caller_hit_create(
    char *project,
    char *file_path,
    char *caller,
    int *line
);

void code_caller_hit_free(code_caller_hit_t *code_caller_hit);

code_caller_hit_t *code_caller_hit_parseFromJSON(cJSON *code_caller_hitJSON);

cJSON *code_caller_hit_convertToJSON(code_caller_hit_t *code_caller_hit);

#endif /* _code_caller_hit_H_ */

