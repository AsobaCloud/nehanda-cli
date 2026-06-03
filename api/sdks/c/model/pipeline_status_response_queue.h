/*
 * pipeline_status_response_queue.h
 *
 * 
 */

#ifndef _pipeline_status_response_queue_H_
#define _pipeline_status_response_queue_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct pipeline_status_response_queue_t pipeline_status_response_queue_t;




typedef struct pipeline_status_response_queue_t {
    int *pending; //numeric
    int *running; //numeric
    int *done; //numeric
    int *failed; //numeric
    int *total; //numeric

    int _library_owned; // Is the library responsible for freeing this object?
} pipeline_status_response_queue_t;

__attribute__((deprecated)) pipeline_status_response_queue_t *pipeline_status_response_queue_create(
    int *pending,
    int *running,
    int *done,
    int *failed,
    int *total
);

void pipeline_status_response_queue_free(pipeline_status_response_queue_t *pipeline_status_response_queue);

pipeline_status_response_queue_t *pipeline_status_response_queue_parseFromJSON(cJSON *pipeline_status_response_queueJSON);

cJSON *pipeline_status_response_queue_convertToJSON(pipeline_status_response_queue_t *pipeline_status_response_queue);

#endif /* _pipeline_status_response_queue_H_ */

