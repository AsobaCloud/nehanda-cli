/*
 * drain_response.h
 *
 * 
 */

#ifndef _drain_response_H_
#define _drain_response_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct drain_response_t drain_response_t;


// Enum STATE for drain_response

typedef enum  { aimee_kb_api_drain_response_STATE_NULL = 0, aimee_kb_api_drain_response_STATE_idle, aimee_kb_api_drain_response_STATE_running, aimee_kb_api_drain_response_STATE_failed } aimee_kb_api_drain_response_STATE_e;

char* drain_response_state_ToString(aimee_kb_api_drain_response_STATE_e state);

aimee_kb_api_drain_response_STATE_e drain_response_state_FromString(char* state);



typedef struct drain_response_t {
    aimee_kb_api_drain_response_STATE_e state; //enum
    int *processed; //numeric
    int *pending; //numeric
    int *running; //numeric
    int *done; //numeric
    int *failed; //numeric
    int *total; //numeric

    int _library_owned; // Is the library responsible for freeing this object?
} drain_response_t;

__attribute__((deprecated)) drain_response_t *drain_response_create(
    aimee_kb_api_drain_response_STATE_e state,
    int *processed,
    int *pending,
    int *running,
    int *done,
    int *failed,
    int *total
);

void drain_response_free(drain_response_t *drain_response);

drain_response_t *drain_response_parseFromJSON(cJSON *drain_responseJSON);

cJSON *drain_response_convertToJSON(drain_response_t *drain_response);

#endif /* _drain_response_H_ */

