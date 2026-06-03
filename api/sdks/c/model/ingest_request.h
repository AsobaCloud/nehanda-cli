/*
 * ingest_request.h
 *
 * 
 */

#ifndef _ingest_request_H_
#define _ingest_request_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct ingest_request_t ingest_request_t;




typedef struct ingest_request_t {
    char *workspace; // string
    char *embedding_command; // string
    int *force; //boolean

    int _library_owned; // Is the library responsible for freeing this object?
} ingest_request_t;

__attribute__((deprecated)) ingest_request_t *ingest_request_create(
    char *workspace,
    char *embedding_command,
    int *force
);

void ingest_request_free(ingest_request_t *ingest_request);

ingest_request_t *ingest_request_parseFromJSON(cJSON *ingest_requestJSON);

cJSON *ingest_request_convertToJSON(ingest_request_t *ingest_request);

#endif /* _ingest_request_H_ */

