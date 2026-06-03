/*
 * code_build_request.h
 *
 * 
 */

#ifndef _code_build_request_H_
#define _code_build_request_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct code_build_request_t code_build_request_t;




typedef struct code_build_request_t {
    char *path; // string
    char *project; // string
    char *embedding_command; // string
    int *force; //boolean

    int _library_owned; // Is the library responsible for freeing this object?
} code_build_request_t;

__attribute__((deprecated)) code_build_request_t *code_build_request_create(
    char *path,
    char *project,
    char *embedding_command,
    int *force
);

void code_build_request_free(code_build_request_t *code_build_request);

code_build_request_t *code_build_request_parseFromJSON(cJSON *code_build_requestJSON);

cJSON *code_build_request_convertToJSON(code_build_request_t *code_build_request);

#endif /* _code_build_request_H_ */

