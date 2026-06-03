/*
 * health_response.h
 *
 * 
 */

#ifndef _health_response_H_
#define _health_response_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct health_response_t health_response_t;

#include "any_type.h"



typedef struct health_response_t {
    char *status; // string
    int *db2_ok; //boolean
    int *db2_kb_tables_ok; //boolean
    int *pgvec_ok; //boolean
    int *pgvec_collection_ok; //boolean
    int *embed_ok; //boolean
    char *embed_command; // string
    int *chunk_count; //numeric
    int *embedding_count; //numeric
    list_t *warnings; //primitive container

    int _library_owned; // Is the library responsible for freeing this object?
} health_response_t;

__attribute__((deprecated)) health_response_t *health_response_create(
    char *status,
    int *db2_ok,
    int *db2_kb_tables_ok,
    int *pgvec_ok,
    int *pgvec_collection_ok,
    int *embed_ok,
    char *embed_command,
    int *chunk_count,
    int *embedding_count,
    list_t *warnings
);

void health_response_free(health_response_t *health_response);

health_response_t *health_response_parseFromJSON(cJSON *health_responseJSON);

cJSON *health_response_convertToJSON(health_response_t *health_response);

#endif /* _health_response_H_ */

