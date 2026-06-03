/*
 * doc_ingest_response.h
 *
 * 
 */

#ifndef _doc_ingest_response_H_
#define _doc_ingest_response_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct doc_ingest_response_t doc_ingest_response_t;


// Enum STATE for doc_ingest_response

typedef enum  { aimee_kb_api_doc_ingest_response_STATE_NULL = 0, aimee_kb_api_doc_ingest_response_STATE_staged } aimee_kb_api_doc_ingest_response_STATE_e;

char* doc_ingest_response_state_ToString(aimee_kb_api_doc_ingest_response_STATE_e state);

aimee_kb_api_doc_ingest_response_STATE_e doc_ingest_response_state_FromString(char* state);



typedef struct doc_ingest_response_t {
    long *doc_id; //numeric
    aimee_kb_api_doc_ingest_response_STATE_e state; //enum

    int _library_owned; // Is the library responsible for freeing this object?
} doc_ingest_response_t;

__attribute__((deprecated)) doc_ingest_response_t *doc_ingest_response_create(
    long *doc_id,
    aimee_kb_api_doc_ingest_response_STATE_e state
);

void doc_ingest_response_free(doc_ingest_response_t *doc_ingest_response);

doc_ingest_response_t *doc_ingest_response_parseFromJSON(cJSON *doc_ingest_responseJSON);

cJSON *doc_ingest_response_convertToJSON(doc_ingest_response_t *doc_ingest_response);

#endif /* _doc_ingest_response_H_ */

