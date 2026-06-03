#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "maintenance_reconcile_response.h"



static maintenance_reconcile_response_t *maintenance_reconcile_response_create_internal(
    char *status,
    int *rc,
    int *dry_run,
    maintenance_reconcile_response_memory_t *memory,
    maintenance_reconcile_response_memory_t *kb
    ) {
    maintenance_reconcile_response_t *maintenance_reconcile_response_local_var = malloc(sizeof(maintenance_reconcile_response_t));
    if (!maintenance_reconcile_response_local_var) {
        return NULL;
    }
    memset(maintenance_reconcile_response_local_var, 0, sizeof(maintenance_reconcile_response_t));
    maintenance_reconcile_response_local_var->_library_owned = 1;
    maintenance_reconcile_response_local_var->status = status;
    maintenance_reconcile_response_local_var->rc = rc;
    maintenance_reconcile_response_local_var->dry_run = dry_run;
    maintenance_reconcile_response_local_var->memory = memory;
    maintenance_reconcile_response_local_var->kb = kb;
    return maintenance_reconcile_response_local_var;
}

__attribute__((deprecated)) maintenance_reconcile_response_t *maintenance_reconcile_response_create(
    char *status,
    int *rc,
    int *dry_run,
    maintenance_reconcile_response_memory_t *memory,
    maintenance_reconcile_response_memory_t *kb
    ) {
    int *rc_copy = NULL;
    if (rc) {
        rc_copy = malloc(sizeof(int));
        if (rc_copy) *rc_copy = *rc;
    }
    int *dry_run_copy = NULL;
    if (dry_run) {
        dry_run_copy = malloc(sizeof(int));
        if (dry_run_copy) *dry_run_copy = *dry_run;
    }
    maintenance_reconcile_response_t *result = maintenance_reconcile_response_create_internal (
        status,
        rc_copy,
        dry_run_copy,
        memory,
        kb
        );
    if (!result) {
        free(rc_copy);
        free(dry_run_copy);
    }
    return result;
}

void maintenance_reconcile_response_free(maintenance_reconcile_response_t *maintenance_reconcile_response) {
    if(NULL == maintenance_reconcile_response){
        return ;
    }
    if(maintenance_reconcile_response->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "maintenance_reconcile_response_free");
        return ;
    }
    listEntry_t *listEntry;
    if (maintenance_reconcile_response->status) {
        free(maintenance_reconcile_response->status);
        maintenance_reconcile_response->status = NULL;
    }
    if (maintenance_reconcile_response->rc) {
        free(maintenance_reconcile_response->rc);
        maintenance_reconcile_response->rc = NULL;
    }
    if (maintenance_reconcile_response->dry_run) {
        free(maintenance_reconcile_response->dry_run);
        maintenance_reconcile_response->dry_run = NULL;
    }
    if (maintenance_reconcile_response->memory) {
        maintenance_reconcile_response_memory_free(maintenance_reconcile_response->memory);
        maintenance_reconcile_response->memory = NULL;
    }
    if (maintenance_reconcile_response->kb) {
        maintenance_reconcile_response_memory_free(maintenance_reconcile_response->kb);
        maintenance_reconcile_response->kb = NULL;
    }
    free(maintenance_reconcile_response);
}

cJSON *maintenance_reconcile_response_convertToJSON(maintenance_reconcile_response_t *maintenance_reconcile_response) {
    cJSON *item = cJSON_CreateObject();

    // maintenance_reconcile_response->status
    if(maintenance_reconcile_response->status) {
    if(cJSON_AddStringToObject(item, "status", maintenance_reconcile_response->status) == NULL) {
    goto fail; //String
    }
    }


    // maintenance_reconcile_response->rc
    if(maintenance_reconcile_response->rc) {
    if(cJSON_AddNumberToObject(item, "rc", *maintenance_reconcile_response->rc) == NULL) {
    goto fail; //Numeric
    }
    }


    // maintenance_reconcile_response->dry_run
    if(maintenance_reconcile_response->dry_run) {
    if(cJSON_AddBoolToObject(item, "dry_run", *maintenance_reconcile_response->dry_run) == NULL) {
    goto fail; //Bool
    }
    }


    // maintenance_reconcile_response->memory
    if(maintenance_reconcile_response->memory) {
    cJSON *memory_local_JSON = maintenance_reconcile_response_memory_convertToJSON(maintenance_reconcile_response->memory);
    if(memory_local_JSON == NULL) {
    goto fail; //model
    }
    cJSON_AddItemToObject(item, "memory", memory_local_JSON);
    if(item->child == NULL) {
    goto fail;
    }
    }


    // maintenance_reconcile_response->kb
    if(maintenance_reconcile_response->kb) {
    cJSON *kb_local_JSON = maintenance_reconcile_response_memory_convertToJSON(maintenance_reconcile_response->kb);
    if(kb_local_JSON == NULL) {
    goto fail; //model
    }
    cJSON_AddItemToObject(item, "kb", kb_local_JSON);
    if(item->child == NULL) {
    goto fail;
    }
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

maintenance_reconcile_response_t *maintenance_reconcile_response_parseFromJSON(cJSON *maintenance_reconcile_responseJSON){

    maintenance_reconcile_response_t *maintenance_reconcile_response_local_var = NULL;

    char *status_local_str = NULL;

    // define the local variable for maintenance_reconcile_response->rc
    int *rc_local_var = NULL;

    // define the local variable for maintenance_reconcile_response->dry_run
    int *dry_run_local_var = NULL;

    // define the local variable for maintenance_reconcile_response->memory
    maintenance_reconcile_response_memory_t *memory_local_nonprim = NULL;

    // define the local variable for maintenance_reconcile_response->kb
    maintenance_reconcile_response_memory_t *kb_local_nonprim = NULL;

    // maintenance_reconcile_response->status
    cJSON *status = cJSON_GetObjectItemCaseSensitive(maintenance_reconcile_responseJSON, "status");
    if (cJSON_IsNull(status)) {
        status = NULL;
    }
    if (status) { 
    if(!cJSON_IsString(status) && !cJSON_IsNull(status))
    {
    goto end; //String
    }
    }

    // maintenance_reconcile_response->rc
    cJSON *rc = cJSON_GetObjectItemCaseSensitive(maintenance_reconcile_responseJSON, "rc");
    if (cJSON_IsNull(rc)) {
        rc = NULL;
    }
    if (rc) { 
    if(!cJSON_IsNumber(rc))
    {
    goto end; //Numeric
    }
    rc_local_var = malloc(sizeof(int));
    if(!rc_local_var)
    {
        goto end;
    }
    *rc_local_var = rc->valuedouble;
    }

    // maintenance_reconcile_response->dry_run
    cJSON *dry_run = cJSON_GetObjectItemCaseSensitive(maintenance_reconcile_responseJSON, "dry_run");
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

    // maintenance_reconcile_response->memory
    cJSON *memory = cJSON_GetObjectItemCaseSensitive(maintenance_reconcile_responseJSON, "memory");
    if (cJSON_IsNull(memory)) {
        memory = NULL;
    }
    if (memory) { 
    memory_local_nonprim = maintenance_reconcile_response_memory_parseFromJSON(memory); //nonprimitive
    }

    // maintenance_reconcile_response->kb
    cJSON *kb = cJSON_GetObjectItemCaseSensitive(maintenance_reconcile_responseJSON, "kb");
    if (cJSON_IsNull(kb)) {
        kb = NULL;
    }
    if (kb) { 
    kb_local_nonprim = maintenance_reconcile_response_memory_parseFromJSON(kb); //nonprimitive
    }


    if (status && !cJSON_IsNull(status)) status_local_str = strdup(status->valuestring);

    maintenance_reconcile_response_local_var = maintenance_reconcile_response_create_internal (
        status_local_str,
        rc_local_var,
        dry_run_local_var,
        memory ? memory_local_nonprim : NULL,
        kb ? kb_local_nonprim : NULL
        );

    if (!maintenance_reconcile_response_local_var) {
        goto end;
    }

    return maintenance_reconcile_response_local_var;
end:
    if (status_local_str) {
        free(status_local_str);
        status_local_str = NULL;
    }
    if (rc_local_var) {
        free(rc_local_var);
        rc_local_var = NULL;
    }
    if (dry_run_local_var) {
        free(dry_run_local_var);
        dry_run_local_var = NULL;
    }
    if (memory_local_nonprim) {
        maintenance_reconcile_response_memory_free(memory_local_nonprim);
        memory_local_nonprim = NULL;
    }
    if (kb_local_nonprim) {
        maintenance_reconcile_response_memory_free(kb_local_nonprim);
        kb_local_nonprim = NULL;
    }
    return NULL;

}
