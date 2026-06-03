#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "maintenance_reconcile_request.h"



static maintenance_reconcile_request_t *maintenance_reconcile_request_create_internal(
    int *dry_run
    ) {
    maintenance_reconcile_request_t *maintenance_reconcile_request_local_var = malloc(sizeof(maintenance_reconcile_request_t));
    if (!maintenance_reconcile_request_local_var) {
        return NULL;
    }
    memset(maintenance_reconcile_request_local_var, 0, sizeof(maintenance_reconcile_request_t));
    maintenance_reconcile_request_local_var->_library_owned = 1;
    maintenance_reconcile_request_local_var->dry_run = dry_run;
    return maintenance_reconcile_request_local_var;
}

__attribute__((deprecated)) maintenance_reconcile_request_t *maintenance_reconcile_request_create(
    int *dry_run
    ) {
    int *dry_run_copy = NULL;
    if (dry_run) {
        dry_run_copy = malloc(sizeof(int));
        if (dry_run_copy) *dry_run_copy = *dry_run;
    }
    maintenance_reconcile_request_t *result = maintenance_reconcile_request_create_internal (
        dry_run_copy
        );
    if (!result) {
        free(dry_run_copy);
    }
    return result;
}

void maintenance_reconcile_request_free(maintenance_reconcile_request_t *maintenance_reconcile_request) {
    if(NULL == maintenance_reconcile_request){
        return ;
    }
    if(maintenance_reconcile_request->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "maintenance_reconcile_request_free");
        return ;
    }
    listEntry_t *listEntry;
    if (maintenance_reconcile_request->dry_run) {
        free(maintenance_reconcile_request->dry_run);
        maintenance_reconcile_request->dry_run = NULL;
    }
    free(maintenance_reconcile_request);
}

cJSON *maintenance_reconcile_request_convertToJSON(maintenance_reconcile_request_t *maintenance_reconcile_request) {
    cJSON *item = cJSON_CreateObject();

    // maintenance_reconcile_request->dry_run
    if(maintenance_reconcile_request->dry_run) {
    if(cJSON_AddBoolToObject(item, "dry_run", *maintenance_reconcile_request->dry_run) == NULL) {
    goto fail; //Bool
    }
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

maintenance_reconcile_request_t *maintenance_reconcile_request_parseFromJSON(cJSON *maintenance_reconcile_requestJSON){

    maintenance_reconcile_request_t *maintenance_reconcile_request_local_var = NULL;

    // define the local variable for maintenance_reconcile_request->dry_run
    int *dry_run_local_var = NULL;

    // maintenance_reconcile_request->dry_run
    cJSON *dry_run = cJSON_GetObjectItemCaseSensitive(maintenance_reconcile_requestJSON, "dry_run");
    if (cJSON_IsNull(dry_run)) {
        dry_run = NULL;
    }
    if (dry_run) { 
    if(!cJSON_IsBool(dry_run))
    {
    goto end; //Bool
    }
    dry_run_local_var = malloc(sizeof(int));
    if(!dry_run_local_var)
    {
        goto end;
    }
    *dry_run_local_var = dry_run->valueint;
    }



    maintenance_reconcile_request_local_var = maintenance_reconcile_request_create_internal (
        dry_run_local_var
        );

    if (!maintenance_reconcile_request_local_var) {
        goto end;
    }

    return maintenance_reconcile_request_local_var;
end:
    if (dry_run_local_var) {
        free(dry_run_local_var);
        dry_run_local_var = NULL;
    }
    return NULL;

}
