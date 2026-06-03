/*
 * maintenance_reconcile_request.h
 *
 * 
 */

#ifndef _maintenance_reconcile_request_H_
#define _maintenance_reconcile_request_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct maintenance_reconcile_request_t maintenance_reconcile_request_t;




typedef struct maintenance_reconcile_request_t {
    int *dry_run; //boolean

    int _library_owned; // Is the library responsible for freeing this object?
} maintenance_reconcile_request_t;

__attribute__((deprecated)) maintenance_reconcile_request_t *maintenance_reconcile_request_create(
    int *dry_run
);

void maintenance_reconcile_request_free(maintenance_reconcile_request_t *maintenance_reconcile_request);

maintenance_reconcile_request_t *maintenance_reconcile_request_parseFromJSON(cJSON *maintenance_reconcile_requestJSON);

cJSON *maintenance_reconcile_request_convertToJSON(maintenance_reconcile_request_t *maintenance_reconcile_request);

#endif /* _maintenance_reconcile_request_H_ */

