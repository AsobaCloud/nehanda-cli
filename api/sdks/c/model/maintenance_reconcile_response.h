/*
 * maintenance_reconcile_response.h
 *
 * 
 */

#ifndef _maintenance_reconcile_response_H_
#define _maintenance_reconcile_response_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct maintenance_reconcile_response_t maintenance_reconcile_response_t;

#include "maintenance_reconcile_response_memory.h"



typedef struct maintenance_reconcile_response_t {
    char *status; // string
    int *rc; //numeric
    int *dry_run; //boolean
    struct maintenance_reconcile_response_memory_t *memory; //model
    struct maintenance_reconcile_response_memory_t *kb; //model

    int _library_owned; // Is the library responsible for freeing this object?
} maintenance_reconcile_response_t;

__attribute__((deprecated)) maintenance_reconcile_response_t *maintenance_reconcile_response_create(
    char *status,
    int *rc,
    int *dry_run,
    maintenance_reconcile_response_memory_t *memory,
    maintenance_reconcile_response_memory_t *kb
);

void maintenance_reconcile_response_free(maintenance_reconcile_response_t *maintenance_reconcile_response);

maintenance_reconcile_response_t *maintenance_reconcile_response_parseFromJSON(cJSON *maintenance_reconcile_responseJSON);

cJSON *maintenance_reconcile_response_convertToJSON(maintenance_reconcile_response_t *maintenance_reconcile_response);

#endif /* _maintenance_reconcile_response_H_ */

