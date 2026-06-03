/*
 * create_release_response.h
 *
 * 
 */

#ifndef _create_release_response_H_
#define _create_release_response_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct create_release_response_t create_release_response_t;


// Enum STATE for create_release_response

typedef enum  { aimee_kb_api_create_release_response_STATE_NULL = 0, aimee_kb_api_create_release_response_STATE_pending } aimee_kb_api_create_release_response_STATE_e;

char* create_release_response_state_ToString(aimee_kb_api_create_release_response_STATE_e state);

aimee_kb_api_create_release_response_STATE_e create_release_response_state_FromString(char* state);



typedef struct create_release_response_t {
    long *release_id; //numeric
    aimee_kb_api_create_release_response_STATE_e state; //enum

    int _library_owned; // Is the library responsible for freeing this object?
} create_release_response_t;

__attribute__((deprecated)) create_release_response_t *create_release_response_create(
    long *release_id,
    aimee_kb_api_create_release_response_STATE_e state
);

void create_release_response_free(create_release_response_t *create_release_response);

create_release_response_t *create_release_response_parseFromJSON(cJSON *create_release_responseJSON);

cJSON *create_release_response_convertToJSON(create_release_response_t *create_release_response);

#endif /* _create_release_response_H_ */

