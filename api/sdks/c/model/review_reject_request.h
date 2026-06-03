/*
 * review_reject_request.h
 *
 * 
 */

#ifndef _review_reject_request_H_
#define _review_reject_request_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct review_reject_request_t review_reject_request_t;




typedef struct review_reject_request_t {
    char *reason; // string

    int _library_owned; // Is the library responsible for freeing this object?
} review_reject_request_t;

__attribute__((deprecated)) review_reject_request_t *review_reject_request_create(
    char *reason
);

void review_reject_request_free(review_reject_request_t *review_reject_request);

review_reject_request_t *review_reject_request_parseFromJSON(cJSON *review_reject_requestJSON);

cJSON *review_reject_request_convertToJSON(review_reject_request_t *review_reject_request);

#endif /* _review_reject_request_H_ */

