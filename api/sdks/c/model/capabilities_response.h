/*
 * capabilities_response.h
 *
 * 
 */

#ifndef _capabilities_response_H_
#define _capabilities_response_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct capabilities_response_t capabilities_response_t;




typedef struct capabilities_response_t {
    list_t *capabilities; //primitive container
    char *version; // string

    int _library_owned; // Is the library responsible for freeing this object?
} capabilities_response_t;

__attribute__((deprecated)) capabilities_response_t *capabilities_response_create(
    list_t *capabilities,
    char *version
);

void capabilities_response_free(capabilities_response_t *capabilities_response);

capabilities_response_t *capabilities_response_parseFromJSON(cJSON *capabilities_responseJSON);

cJSON *capabilities_response_convertToJSON(capabilities_response_t *capabilities_response);

#endif /* _capabilities_response_H_ */

