/*
 * ingest_status_response.h
 *
 * 
 */

#ifndef _ingest_status_response_H_
#define _ingest_status_response_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct ingest_status_response_t ingest_status_response_t;

#include "ingest_status_response_queue.h"
#include "ingest_status_response_recent_inner.h"
#include "ingest_status_response_workers.h"



typedef struct ingest_status_response_t {
    char *status; // string
    struct ingest_status_response_queue_t *queue; //model
    struct ingest_status_response_workers_t *workers; //model
    list_t *recent; //nonprimitive container

    int _library_owned; // Is the library responsible for freeing this object?
} ingest_status_response_t;

__attribute__((deprecated)) ingest_status_response_t *ingest_status_response_create(
    char *status,
    ingest_status_response_queue_t *queue,
    ingest_status_response_workers_t *workers,
    list_t *recent
);

void ingest_status_response_free(ingest_status_response_t *ingest_status_response);

ingest_status_response_t *ingest_status_response_parseFromJSON(cJSON *ingest_status_responseJSON);

cJSON *ingest_status_response_convertToJSON(ingest_status_response_t *ingest_status_response);

#endif /* _ingest_status_response_H_ */

