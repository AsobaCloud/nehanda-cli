/*
 * code_update_request.h
 *
 * 
 */

#ifndef _code_update_request_H_
#define _code_update_request_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct code_update_request_t code_update_request_t;




typedef struct code_update_request_t {
    char *path; // string
    char *project; // string
    char *embedding_command; // string

    int _library_owned; // Is the library responsible for freeing this object?
} code_update_request_t;

__attribute__((deprecated)) code_update_request_t *code_update_request_create(
    char *path,
    char *project,
    char *embedding_command
);

void code_update_request_free(code_update_request_t *code_update_request);

code_update_request_t *code_update_request_parseFromJSON(cJSON *code_update_requestJSON);

cJSON *code_update_request_convertToJSON(code_update_request_t *code_update_request);

#endif /* _code_update_request_H_ */

