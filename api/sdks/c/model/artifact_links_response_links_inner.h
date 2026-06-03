/*
 * artifact_links_response_links_inner.h
 *
 * 
 */

#ifndef _artifact_links_response_links_inner_H_
#define _artifact_links_response_links_inner_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct artifact_links_response_links_inner_t artifact_links_response_links_inner_t;




typedef struct artifact_links_response_links_inner_t {
    char *to_id; // string
    char *link_kind; // string

    int _library_owned; // Is the library responsible for freeing this object?
} artifact_links_response_links_inner_t;

__attribute__((deprecated)) artifact_links_response_links_inner_t *artifact_links_response_links_inner_create(
    char *to_id,
    char *link_kind
);

void artifact_links_response_links_inner_free(artifact_links_response_links_inner_t *artifact_links_response_links_inner);

artifact_links_response_links_inner_t *artifact_links_response_links_inner_parseFromJSON(cJSON *artifact_links_response_links_innerJSON);

cJSON *artifact_links_response_links_inner_convertToJSON(artifact_links_response_links_inner_t *artifact_links_response_links_inner);

#endif /* _artifact_links_response_links_inner_H_ */

