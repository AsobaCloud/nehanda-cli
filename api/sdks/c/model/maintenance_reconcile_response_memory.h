/*
 * maintenance_reconcile_response_memory.h
 *
 * 
 */

#ifndef _maintenance_reconcile_response_memory_H_
#define _maintenance_reconcile_response_memory_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct maintenance_reconcile_response_memory_t maintenance_reconcile_response_memory_t;




typedef struct maintenance_reconcile_response_memory_t {
    long *kept; //numeric
    long *pruned; //numeric

    int _library_owned; // Is the library responsible for freeing this object?
} maintenance_reconcile_response_memory_t;

__attribute__((deprecated)) maintenance_reconcile_response_memory_t *maintenance_reconcile_response_memory_create(
    long *kept,
    long *pruned
);

void maintenance_reconcile_response_memory_free(maintenance_reconcile_response_memory_t *maintenance_reconcile_response_memory);

maintenance_reconcile_response_memory_t *maintenance_reconcile_response_memory_parseFromJSON(cJSON *maintenance_reconcile_response_memoryJSON);

cJSON *maintenance_reconcile_response_memory_convertToJSON(maintenance_reconcile_response_memory_t *maintenance_reconcile_response_memory);

#endif /* _maintenance_reconcile_response_memory_H_ */

