/*
 * code_find_hit.h
 *
 * 
 */

#ifndef _code_find_hit_H_
#define _code_find_hit_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct code_find_hit_t code_find_hit_t;




typedef struct code_find_hit_t {
    char *project; // string
    char *file_path; // string
    int *line; //numeric
    char *kind; // string

    int _library_owned; // Is the library responsible for freeing this object?
} code_find_hit_t;

__attribute__((deprecated)) code_find_hit_t *code_find_hit_create(
    char *project,
    char *file_path,
    int *line,
    char *kind
);

void code_find_hit_free(code_find_hit_t *code_find_hit);

code_find_hit_t *code_find_hit_parseFromJSON(cJSON *code_find_hitJSON);

cJSON *code_find_hit_convertToJSON(code_find_hit_t *code_find_hit);

#endif /* _code_find_hit_H_ */

