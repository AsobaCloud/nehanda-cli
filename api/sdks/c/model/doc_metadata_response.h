/*
 * doc_metadata_response.h
 *
 * 
 */

#ifndef _doc_metadata_response_H_
#define _doc_metadata_response_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct doc_metadata_response_t doc_metadata_response_t;




typedef struct doc_metadata_response_t {
    long *id; //numeric
    char *filename; // string
    char *content_hash; // string
    char *converter; // string
    char *converter_version; // string
    char *scope; // string
    char *state; // string
    int *review_needed; //boolean
    char *created_at; //date time

    int _library_owned; // Is the library responsible for freeing this object?
} doc_metadata_response_t;

__attribute__((deprecated)) doc_metadata_response_t *doc_metadata_response_create(
    long *id,
    char *filename,
    char *content_hash,
    char *converter,
    char *converter_version,
    char *scope,
    char *state,
    int *review_needed,
    char *created_at
);

void doc_metadata_response_free(doc_metadata_response_t *doc_metadata_response);

doc_metadata_response_t *doc_metadata_response_parseFromJSON(cJSON *doc_metadata_responseJSON);

cJSON *doc_metadata_response_convertToJSON(doc_metadata_response_t *doc_metadata_response);

#endif /* _doc_metadata_response_H_ */

