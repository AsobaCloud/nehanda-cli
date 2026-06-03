/*
 * code_update_response.h
 *
 * 
 */

#ifndef _code_update_response_H_
#define _code_update_response_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct code_update_response_t code_update_response_t;




typedef struct code_update_response_t {
    char *status; // string
    char *project; // string
    int *files_scanned; //numeric
    int *files_indexed; //numeric
    int *files_skipped; //numeric
    int *files_removed; //numeric
    int *chunks_added; //numeric
    int *chunks_removed; //numeric
    int *embeddings_added; //numeric

    int _library_owned; // Is the library responsible for freeing this object?
} code_update_response_t;

__attribute__((deprecated)) code_update_response_t *code_update_response_create(
    char *status,
    char *project,
    int *files_scanned,
    int *files_indexed,
    int *files_skipped,
    int *files_removed,
    int *chunks_added,
    int *chunks_removed,
    int *embeddings_added
);

void code_update_response_free(code_update_response_t *code_update_response);

code_update_response_t *code_update_response_parseFromJSON(cJSON *code_update_responseJSON);

cJSON *code_update_response_convertToJSON(code_update_response_t *code_update_response);

#endif /* _code_update_response_H_ */

