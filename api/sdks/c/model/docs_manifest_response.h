/*
 * docs_manifest_response.h
 *
 * 
 */

#ifndef _docs_manifest_response_H_
#define _docs_manifest_response_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct docs_manifest_response_t docs_manifest_response_t;

#include "docs_manifest_response_missing_inner.h"



typedef struct docs_manifest_response_t {
    list_t *missing; //nonprimitive container
    int *total; //numeric
    int *present; //numeric
    int *missing_count; //numeric

    int _library_owned; // Is the library responsible for freeing this object?
} docs_manifest_response_t;

__attribute__((deprecated)) docs_manifest_response_t *docs_manifest_response_create(
    list_t *missing,
    int *total,
    int *present,
    int *missing_count
);

void docs_manifest_response_free(docs_manifest_response_t *docs_manifest_response);

docs_manifest_response_t *docs_manifest_response_parseFromJSON(cJSON *docs_manifest_responseJSON);

cJSON *docs_manifest_response_convertToJSON(docs_manifest_response_t *docs_manifest_response);

#endif /* _docs_manifest_response_H_ */

