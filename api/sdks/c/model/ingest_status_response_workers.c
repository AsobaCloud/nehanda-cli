#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "ingest_status_response_workers.h"



static ingest_status_response_workers_t *ingest_status_response_workers_create_internal(
    int *configured,
    int *active
    ) {
    ingest_status_response_workers_t *ingest_status_response_workers_local_var = malloc(sizeof(ingest_status_response_workers_t));
    if (!ingest_status_response_workers_local_var) {
        return NULL;
    }
    memset(ingest_status_response_workers_local_var, 0, sizeof(ingest_status_response_workers_t));
    ingest_status_response_workers_local_var->_library_owned = 1;
    ingest_status_response_workers_local_var->configured = configured;
    ingest_status_response_workers_local_var->active = active;
    return ingest_status_response_workers_local_var;
}

__attribute__((deprecated)) ingest_status_response_workers_t *ingest_status_response_workers_create(
    int *configured,
    int *active
    ) {
    int *configured_copy = NULL;
    if (configured) {
        configured_copy = malloc(sizeof(int));
        if (configured_copy) *configured_copy = *configured;
    }
    int *active_copy = NULL;
    if (active) {
        active_copy = malloc(sizeof(int));
        if (active_copy) *active_copy = *active;
    }
    ingest_status_response_workers_t *result = ingest_status_response_workers_create_internal (
        configured_copy,
        active_copy
        );
    if (!result) {
        free(configured_copy);
        free(active_copy);
    }
    return result;
}

void ingest_status_response_workers_free(ingest_status_response_workers_t *ingest_status_response_workers) {
    if(NULL == ingest_status_response_workers){
        return ;
    }
    if(ingest_status_response_workers->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "ingest_status_response_workers_free");
        return ;
    }
    listEntry_t *listEntry;
    if (ingest_status_response_workers->configured) {
        free(ingest_status_response_workers->configured);
        ingest_status_response_workers->configured = NULL;
    }
    if (ingest_status_response_workers->active) {
        free(ingest_status_response_workers->active);
        ingest_status_response_workers->active = NULL;
    }
    free(ingest_status_response_workers);
}

cJSON *ingest_status_response_workers_convertToJSON(ingest_status_response_workers_t *ingest_status_response_workers) {
    cJSON *item = cJSON_CreateObject();

    // ingest_status_response_workers->configured
    if(ingest_status_response_workers->configured) {
    if(cJSON_AddNumberToObject(item, "configured", *ingest_status_response_workers->configured) == NULL) {
    goto fail; //Numeric
    }
    }


    // ingest_status_response_workers->active
    if(ingest_status_response_workers->active) {
    if(cJSON_AddNumberToObject(item, "active", *ingest_status_response_workers->active) == NULL) {
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

ingest_status_response_workers_t *ingest_status_response_workers_parseFromJSON(cJSON *ingest_status_response_workersJSON){

    ingest_status_response_workers_t *ingest_status_response_workers_local_var = NULL;

    // define the local variable for ingest_status_response_workers->configured
    int *configured_local_var = NULL;

    // define the local variable for ingest_status_response_workers->active
    int *active_local_var = NULL;

    // ingest_status_response_workers->configured
    cJSON *configured = cJSON_GetObjectItemCaseSensitive(ingest_status_response_workersJSON, "configured");
    if (cJSON_IsNull(configured)) {
        configured = NULL;
    }
    if (configured) { 
    if(!cJSON_IsNumber(configured))
    {
    goto end; //Numeric
    }
    configured_local_var = malloc(sizeof(int));
    if(!configured_local_var)
    {
        goto end;
    }
    *configured_local_var = configured->valuedouble;
    }

    // ingest_status_response_workers->active
    cJSON *active = cJSON_GetObjectItemCaseSensitive(ingest_status_response_workersJSON, "active");
    if (cJSON_IsNull(active)) {
        active = NULL;
    }
    if (active) { 
    if(!cJSON_IsNumber(active))
    {
    goto end; //Numeric
    }
    active_local_var = malloc(sizeof(int));
    if(!active_local_var)
    {
        goto end;
    }
    *active_local_var = active->valuedouble;
    }



    ingest_status_response_workers_local_var = ingest_status_response_workers_create_internal (
        configured_local_var,
        active_local_var
        );

    if (!ingest_status_response_workers_local_var) {
        goto end;
    }

    return ingest_status_response_workers_local_var;
end:
    if (configured_local_var) {
        free(configured_local_var);
        configured_local_var = NULL;
    }
    if (active_local_var) {
        free(active_local_var);
        active_local_var = NULL;
    }
    return NULL;

}
