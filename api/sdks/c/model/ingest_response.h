/*
 * ingest_response.h
 *
 * 
 */

#ifndef _ingest_response_H_
#define _ingest_response_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct ingest_response_t ingest_response_t;




typedef struct ingest_response_t {
    char *status; // string
    int *projects_queued; //numeric
    char *message; // string

    int _library_owned; // Is the library responsible for freeing this object?
} ingest_response_t;

__attribute__((deprecated)) ingest_response_t *ingest_response_create(
    char *status,
    int *projects_queued,
    char *message
);

void ingest_response_free(ingest_response_t *ingest_response);

ingest_response_t *ingest_response_parseFromJSON(cJSON *ingest_responseJSON);

cJSON *ingest_response_convertToJSON(ingest_response_t *ingest_response);

#endif /* _ingest_response_H_ */

