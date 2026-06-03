/*
 * artifact_response.h
 *
 * 
 */

#ifndef _artifact_response_H_
#define _artifact_response_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct artifact_response_t artifact_response_t;

#include "object.h"
#include "search_hit_citations_inner.h"



typedef struct artifact_response_t {
    char *id; // string
    char *kind; // string
    char *state; // string
    char *scope_kind; // string
    char *scope_id; // string
    double *confidence; //numeric
    object_t *payload; //object
    list_t *citations; //nonprimitive container
    char *updated_at; //date time

    int _library_owned; // Is the library responsible for freeing this object?
} artifact_response_t;

__attribute__((deprecated)) artifact_response_t *artifact_response_create(
    char *id,
    char *kind,
    char *state,
    char *scope_kind,
    char *scope_id,
    double *confidence,
    object_t *payload,
    list_t *citations,
    char *updated_at
);

void artifact_response_free(artifact_response_t *artifact_response);

artifact_response_t *artifact_response_parseFromJSON(cJSON *artifact_responseJSON);

cJSON *artifact_response_convertToJSON(artifact_response_t *artifact_response);

#endif /* _artifact_response_H_ */

