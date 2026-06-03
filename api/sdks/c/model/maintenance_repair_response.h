/*
 * maintenance_repair_response.h
 *
 * 
 */

#ifndef _maintenance_repair_response_H_
#define _maintenance_repair_response_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct maintenance_repair_response_t maintenance_repair_response_t;




typedef struct maintenance_repair_response_t {
    char *status; // string
    char *project; // string
    int *files_scanned; //numeric
    int *files_indexed; //numeric
    int *files_skipped; //numeric
    int *files_removed; //numeric
    int *chunks_added; //numeric
    int *chunks_removed; //numeric
    int *embeddings_added; //numeric

    int _library_owned; // Is the library responsible for freeing this object?
} maintenance_repair_response_t;

__attribute__((deprecated)) maintenance_repair_response_t *maintenance_repair_response_create(
    char *status,
    char *project,
    int *files_scanned,
    int *files_indexed,
    int *files_skipped,
    int *files_removed,
    int *chunks_added,
    int *chunks_removed,
    int *embeddings_added
);

void maintenance_repair_response_free(maintenance_repair_response_t *maintenance_repair_response);

maintenance_repair_response_t *maintenance_repair_response_parseFromJSON(cJSON *maintenance_repair_responseJSON);

cJSON *maintenance_repair_response_convertToJSON(maintenance_repair_response_t *maintenance_repair_response);

#endif /* _maintenance_repair_response_H_ */

