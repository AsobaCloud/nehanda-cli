/*
 * docs_manifest_request.h
 *
 * 
 */

#ifndef _docs_manifest_request_H_
#define _docs_manifest_request_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct docs_manifest_request_t docs_manifest_request_t;

#include "docs_manifest_request_docs_inner.h"



typedef struct docs_manifest_request_t {
    char *scope; // string
    list_t *docs; //nonprimitive container

    int _library_owned; // Is the library responsible for freeing this object?
} docs_manifest_request_t;

__attribute__((deprecated)) docs_manifest_request_t *docs_manifest_request_create(
    char *scope,
    list_t *docs
);

void docs_manifest_request_free(docs_manifest_request_t *docs_manifest_request);

docs_manifest_request_t *docs_manifest_request_parseFromJSON(cJSON *docs_manifest_requestJSON);

cJSON *docs_manifest_request_convertToJSON(docs_manifest_request_t *docs_manifest_request);

#endif /* _docs_manifest_request_H_ */

