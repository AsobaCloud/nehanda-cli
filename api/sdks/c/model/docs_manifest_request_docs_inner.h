/*
 * docs_manifest_request_docs_inner.h
 *
 * 
 */

#ifndef _docs_manifest_request_docs_inner_H_
#define _docs_manifest_request_docs_inner_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct docs_manifest_request_docs_inner_t docs_manifest_request_docs_inner_t;




typedef struct docs_manifest_request_docs_inner_t {
    char *doc_key; // string
    char *path; // string
    char *content_hash; // string
    char *scope; // string

    int _library_owned; // Is the library responsible for freeing this object?
} docs_manifest_request_docs_inner_t;

__attribute__((deprecated)) docs_manifest_request_docs_inner_t *docs_manifest_request_docs_inner_create(
    char *doc_key,
    char *path,
    char *content_hash,
    char *scope
);

void docs_manifest_request_docs_inner_free(docs_manifest_request_docs_inner_t *docs_manifest_request_docs_inner);

docs_manifest_request_docs_inner_t *docs_manifest_request_docs_inner_parseFromJSON(cJSON *docs_manifest_request_docs_innerJSON);

cJSON *docs_manifest_request_docs_inner_convertToJSON(docs_manifest_request_docs_inner_t *docs_manifest_request_docs_inner);

#endif /* _docs_manifest_request_docs_inner_H_ */

