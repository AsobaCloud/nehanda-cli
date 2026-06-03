/*
 * ingest_status_response_queue.h
 *
 * 
 */

#ifndef _ingest_status_response_queue_H_
#define _ingest_status_response_queue_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct ingest_status_response_queue_t ingest_status_response_queue_t;




typedef struct ingest_status_response_queue_t {
    int *pending; //numeric
    int *running; //numeric
    int *done_last_24h; //numeric
    int *failed_last_24h; //numeric

    int _library_owned; // Is the library responsible for freeing this object?
} ingest_status_response_queue_t;

__attribute__((deprecated)) ingest_status_response_queue_t *ingest_status_response_queue_create(
    int *pending,
    int *running,
    int *done_last_24h,
    int *failed_last_24h
);

void ingest_status_response_queue_free(ingest_status_response_queue_t *ingest_status_response_queue);

ingest_status_response_queue_t *ingest_status_response_queue_parseFromJSON(cJSON *ingest_status_response_queueJSON);

cJSON *ingest_status_response_queue_convertToJSON(ingest_status_response_queue_t *ingest_status_response_queue);

#endif /* _ingest_status_response_queue_H_ */

