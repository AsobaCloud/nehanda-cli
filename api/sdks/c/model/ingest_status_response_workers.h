/*
 * ingest_status_response_workers.h
 *
 * 
 */

#ifndef _ingest_status_response_workers_H_
#define _ingest_status_response_workers_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct ingest_status_response_workers_t ingest_status_response_workers_t;




typedef struct ingest_status_response_workers_t {
    int *configured; //numeric
    int *active; //numeric

    int _library_owned; // Is the library responsible for freeing this object?
} ingest_status_response_workers_t;

__attribute__((deprecated)) ingest_status_response_workers_t *ingest_status_response_workers_create(
    int *configured,
    int *active
);

void ingest_status_response_workers_free(ingest_status_response_workers_t *ingest_status_response_workers);

ingest_status_response_workers_t *ingest_status_response_workers_parseFromJSON(cJSON *ingest_status_response_workersJSON);

cJSON *ingest_status_response_workers_convertToJSON(ingest_status_response_workers_t *ingest_status_response_workers);

#endif /* _ingest_status_response_workers_H_ */

