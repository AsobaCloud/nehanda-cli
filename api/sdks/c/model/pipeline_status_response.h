/*
 * pipeline_status_response.h
 *
 * 
 */

#ifndef _pipeline_status_response_H_
#define _pipeline_status_response_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct pipeline_status_response_t pipeline_status_response_t;

#include "object.h"
#include "pipeline_status_response_queue.h"

// Enum STATE for pipeline_status_response

typedef enum  { aimee_kb_api_pipeline_status_response_STATE_NULL = 0, aimee_kb_api_pipeline_status_response_STATE_idle, aimee_kb_api_pipeline_status_response_STATE_running, aimee_kb_api_pipeline_status_response_STATE_failed } aimee_kb_api_pipeline_status_response_STATE_e;

char* pipeline_status_response_state_ToString(aimee_kb_api_pipeline_status_response_STATE_e state);

aimee_kb_api_pipeline_status_response_STATE_e pipeline_status_response_state_FromString(char* state);



typedef struct pipeline_status_response_t {
    aimee_kb_api_pipeline_status_response_STATE_e state; //enum
    int *queue_depth; //numeric
    list_t *active_jobs; //nonprimitive container
    struct pipeline_status_response_queue_t *queue; //model

    int _library_owned; // Is the library responsible for freeing this object?
} pipeline_status_response_t;

__attribute__((deprecated)) pipeline_status_response_t *pipeline_status_response_create(
    aimee_kb_api_pipeline_status_response_STATE_e state,
    int *queue_depth,
    list_t *active_jobs,
    pipeline_status_response_queue_t *queue
);

void pipeline_status_response_free(pipeline_status_response_t *pipeline_status_response);

pipeline_status_response_t *pipeline_status_response_parseFromJSON(cJSON *pipeline_status_responseJSON);

cJSON *pipeline_status_response_convertToJSON(pipeline_status_response_t *pipeline_status_response);

#endif /* _pipeline_status_response_H_ */

