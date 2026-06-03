#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "maintenance_reconcile_response_memory.h"



static maintenance_reconcile_response_memory_t *maintenance_reconcile_response_memory_create_internal(
    long *kept,
    long *pruned
    ) {
    maintenance_reconcile_response_memory_t *maintenance_reconcile_response_memory_local_var = malloc(sizeof(maintenance_reconcile_response_memory_t));
    if (!maintenance_reconcile_response_memory_local_var) {
        return NULL;
    }
    memset(maintenance_reconcile_response_memory_local_var, 0, sizeof(maintenance_reconcile_response_memory_t));
    maintenance_reconcile_response_memory_local_var->_library_owned = 1;
    maintenance_reconcile_response_memory_local_var->kept = kept;
    maintenance_reconcile_response_memory_local_var->pruned = pruned;
    return maintenance_reconcile_response_memory_local_var;
}

__attribute__((deprecated)) maintenance_reconcile_response_memory_t *maintenance_reconcile_response_memory_create(
    long *kept,
    long *pruned
    ) {
    long *kept_copy = NULL;
    if (kept) {
        kept_copy = malloc(sizeof(long));
        if (kept_copy) *kept_copy = *kept;
    }
    long *pruned_copy = NULL;
    if (pruned) {
        pruned_copy = malloc(sizeof(long));
        if (pruned_copy) *pruned_copy = *pruned;
    }
    maintenance_reconcile_response_memory_t *result = maintenance_reconcile_response_memory_create_internal (
        kept_copy,
        pruned_copy
        );
    if (!result) {
        free(kept_copy);
        free(pruned_copy);
    }
    return result;
}

void maintenance_reconcile_response_memory_free(maintenance_reconcile_response_memory_t *maintenance_reconcile_response_memory) {
    if(NULL == maintenance_reconcile_response_memory){
        return ;
    }
    if(maintenance_reconcile_response_memory->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "maintenance_reconcile_response_memory_free");
        return ;
    }
    listEntry_t *listEntry;
    if (maintenance_reconcile_response_memory->kept) {
        free(maintenance_reconcile_response_memory->kept);
        maintenance_reconcile_response_memory->kept = NULL;
    }
    if (maintenance_reconcile_response_memory->pruned) {
        free(maintenance_reconcile_response_memory->pruned);
        maintenance_reconcile_response_memory->pruned = NULL;
    }
    free(maintenance_reconcile_response_memory);
}

cJSON *maintenance_reconcile_response_memory_convertToJSON(maintenance_reconcile_response_memory_t *maintenance_reconcile_response_memory) {
    cJSON *item = cJSON_CreateObject();

    // maintenance_reconcile_response_memory->kept
    if(maintenance_reconcile_response_memory->kept) {
    if(cJSON_AddNumberToObject(item, "kept", *maintenance_reconcile_response_memory->kept) == NULL) {
    goto fail; //Numeric
    }
    }


    // maintenance_reconcile_response_memory->pruned
    if(maintenance_reconcile_response_memory->pruned) {
    if(cJSON_AddNumberToObject(item, "pruned", *maintenance_reconcile_response_memory->pruned) == NULL) {
    goto fail; //Numeric
    }
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

maintenance_reconcile_response_memory_t *maintenance_reconcile_response_memory_parseFromJSON(cJSON *maintenance_reconcile_response_memoryJSON){

    maintenance_reconcile_response_memory_t *maintenance_reconcile_response_memory_local_var = NULL;

    // define the local variable for maintenance_reconcile_response_memory->kept
    long *kept_local_var = NULL;

    // define the local variable for maintenance_reconcile_response_memory->pruned
    long *pruned_local_var = NULL;

    // maintenance_reconcile_response_memory->kept
    cJSON *kept = cJSON_GetObjectItemCaseSensitive(maintenance_reconcile_response_memoryJSON, "kept");
    if (cJSON_IsNull(kept)) {
        kept = NULL;
    }
    if (kept) { 
    if(!cJSON_IsNumber(kept))
    {
    goto end; //Numeric
    }
    kept_local_var = malloc(sizeof(long));
    if(!kept_local_var)
    {
        goto end;
    }
    *kept_local_var = kept->valuedouble;
    }

    // maintenance_reconcile_response_memory->pruned
    cJSON *pruned = cJSON_GetObjectItemCaseSensitive(maintenance_reconcile_response_memoryJSON, "pruned");
    if (cJSON_IsNull(pruned)) {
        pruned = NULL;
    }
    if (pruned) { 
    if(!cJSON_IsNumber(pruned))
    {
    goto end; //Numeric
    }
    pruned_local_var = malloc(sizeof(long));
    if(!pruned_local_var)
    {
        goto end;
    }
    *pruned_local_var = pruned->valuedouble;
    }



    maintenance_reconcile_response_memory_local_var = maintenance_reconcile_response_memory_create_internal (
        kept_local_var,
        pruned_local_var
        );

    if (!maintenance_reconcile_response_memory_local_var) {
        goto end;
    }

    return maintenance_reconcile_response_memory_local_var;
end:
    if (kept_local_var) {
        free(kept_local_var);
        kept_local_var = NULL;
    }
    if (pruned_local_var) {
        free(pruned_local_var);
        pruned_local_var = NULL;
    }
    return NULL;

}
