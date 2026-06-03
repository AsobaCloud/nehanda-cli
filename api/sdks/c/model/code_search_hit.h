/*
 * code_search_hit.h
 *
 * 
 */

#ifndef _code_search_hit_H_
#define _code_search_hit_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct code_search_hit_t code_search_hit_t;




typedef struct code_search_hit_t {
    char *project; // string
    char *file_path; // string
    char *snippet; // string
    double *rank; //numeric

    int _library_owned; // Is the library responsible for freeing this object?
} code_search_hit_t;

__attribute__((deprecated)) code_search_hit_t *code_search_hit_create(
    char *project,
    char *file_path,
    char *snippet,
    double *rank
);

void code_search_hit_free(code_search_hit_t *code_search_hit);

code_search_hit_t *code_search_hit_parseFromJSON(cJSON *code_search_hitJSON);

cJSON *code_search_hit_convertToJSON(code_search_hit_t *code_search_hit);

#endif /* _code_search_hit_H_ */

