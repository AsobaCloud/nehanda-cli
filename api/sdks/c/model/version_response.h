/*
 * version_response.h
 *
 * 
 */

#ifndef _version_response_H_
#define _version_response_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct version_response_t version_response_t;




typedef struct version_response_t {
    char *version; // string
    char *service; // string

    int _library_owned; // Is the library responsible for freeing this object?
} version_response_t;

__attribute__((deprecated)) version_response_t *version_response_create(
    char *version,
    char *service
);

void version_response_free(version_response_t *version_response);

version_response_t *version_response_parseFromJSON(cJSON *version_responseJSON);

cJSON *version_response_convertToJSON(version_response_t *version_response);

#endif /* _version_response_H_ */

