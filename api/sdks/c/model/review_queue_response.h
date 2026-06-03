/*
 * review_queue_response.h
 *
 * 
 */

#ifndef _review_queue_response_H_
#define _review_queue_response_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct review_queue_response_t review_queue_response_t;

#include "doc_metadata_response.h"



typedef struct review_queue_response_t {
    list_t *docs; //nonprimitive container
    long *next_cursor; //numeric

    int _library_owned; // Is the library responsible for freeing this object?
} review_queue_response_t;

__attribute__((deprecated)) review_queue_response_t *review_queue_response_create(
    list_t *docs,
    long *next_cursor
);

void review_queue_response_free(review_queue_response_t *review_queue_response);

review_queue_response_t *review_queue_response_parseFromJSON(cJSON *review_queue_responseJSON);

cJSON *review_queue_response_convertToJSON(review_queue_response_t *review_queue_response);

#endif /* _review_queue_response_H_ */

