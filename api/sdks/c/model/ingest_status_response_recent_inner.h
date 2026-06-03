/*
 * ingest_status_response_recent_inner.h
 *
 * 
 */

#ifndef _ingest_status_response_recent_inner_H_
#define _ingest_status_response_recent_inner_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct ingest_status_response_recent_inner_t ingest_status_response_recent_inner_t;




typedef struct ingest_status_response_recent_inner_t {
    char *project; // string
    char *status; // string
    char *completed_at; // string
    int *files_indexed; //numeric
    int *chunks_added; //numeric
    char *error; // string

    int _library_owned; // Is the library responsible for freeing this object?
} ingest_status_response_recent_inner_t;

__attribute__((deprecated)) ingest_status_response_recent_inner_t *ingest_status_response_recent_inner_create(
    char *project,
    char *status,
    char *completed_at,
    int *files_indexed,
    int *chunks_added,
    char *error
);

void ingest_status_response_recent_inner_free(ingest_status_response_recent_inner_t *ingest_status_response_recent_inner);

ingest_status_response_recent_inner_t *ingest_status_response_recent_inner_parseFromJSON(cJSON *ingest_status_response_recent_innerJSON);

cJSON *ingest_status_response_recent_inner_convertToJSON(ingest_status_response_recent_inner_t *ingest_status_response_recent_inner);

#endif /* _ingest_status_response_recent_inner_H_ */

