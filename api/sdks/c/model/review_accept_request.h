/*
 * review_accept_request.h
 *
 * 
 */

#ifndef _review_accept_request_H_
#define _review_accept_request_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct review_accept_request_t review_accept_request_t;




typedef struct review_accept_request_t {
    long *release_id; //numeric

    int _library_owned; // Is the library responsible for freeing this object?
} review_accept_request_t;

__attribute__((deprecated)) review_accept_request_t *review_accept_request_create(
    long *release_id
);

void review_accept_request_free(review_accept_request_t *review_accept_request);

review_accept_request_t *review_accept_request_parseFromJSON(cJSON *review_accept_requestJSON);

cJSON *review_accept_request_convertToJSON(review_accept_request_t *review_accept_request);

#endif /* _review_accept_request_H_ */

