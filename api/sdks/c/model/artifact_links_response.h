/*
 * artifact_links_response.h
 *
 * 
 */

#ifndef _artifact_links_response_H_
#define _artifact_links_response_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct artifact_links_response_t artifact_links_response_t;

#include "artifact_links_response_links_inner.h"



typedef struct artifact_links_response_t {
    char *artifact_id; // string
    list_t *links; //nonprimitive container

    int _library_owned; // Is the library responsible for freeing this object?
} artifact_links_response_t;

__attribute__((deprecated)) artifact_links_response_t *artifact_links_response_create(
    char *artifact_id,
    list_t *links
);

void artifact_links_response_free(artifact_links_response_t *artifact_links_response);

artifact_links_response_t *artifact_links_response_parseFromJSON(cJSON *artifact_links_responseJSON);

cJSON *artifact_links_response_convertToJSON(artifact_links_response_t *artifact_links_response);

#endif /* _artifact_links_response_H_ */

