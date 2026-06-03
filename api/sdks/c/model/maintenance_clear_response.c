#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "maintenance_clear_response.h"



static maintenance_clear_response_t *maintenance_clear_response_create_internal(
    char *status,
    char *project,
    int *chunks_deleted
    ) {
    maintenance_clear_response_t *maintenance_clear_response_local_var = malloc(sizeof(maintenance_clear_response_t));
    if (!maintenance_clear_response_local_var) {
        return NULL;
    }
    memset(maintenance_clear_response_local_var, 0, sizeof(maintenance_clear_response_t));
    maintenance_clear_response_local_var->_library_owned = 1;
    maintenance_clear_response_local_var->status = status;
    maintenance_clear_response_local_var->project = project;
    maintenance_clear_response_local_var->chunks_deleted = chunks_deleted;
    return maintenance_clear_response_local_var;
}

__attribute__((deprecated)) maintenance_clear_response_t *maintenance_clear_response_create(
    char *status,
    char *project,
    int *chunks_deleted
    ) {
    int *chunks_deleted_copy = NULL;
    if (chunks_deleted) {
        chunks_deleted_copy = malloc(sizeof(int));
        if (chunks_deleted_copy) *chunks_deleted_copy = *chunks_deleted;
    }
    maintenance_clear_response_t *result = maintenance_clear_response_create_internal (
        status,
        project,
        chunks_deleted_copy
        );
    if (!result) {
        free(chunks_deleted_copy);
    }
    return result;
}

void maintenance_clear_response_free(maintenance_clear_response_t *maintenance_clear_response) {
    if(NULL == maintenance_clear_response){
        return ;
    }
    if(maintenance_clear_response->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "maintenance_clear_response_free");
        return ;
    }
    listEntry_t *listEntry;
    if (maintenance_clear_response->status) {
        free(maintenance_clear_response->status);
        maintenance_clear_response->status = NULL;
    }
    if (maintenance_clear_response->project) {
        free(maintenance_clear_response->project);
        maintenance_clear_response->project = NULL;
    }
    if (maintenance_clear_response->chunks_deleted) {
        free(maintenance_clear_response->chunks_deleted);
        maintenance_clear_response->chunks_deleted = NULL;
    }
    free(maintenance_clear_response);
}

cJSON *maintenance_clear_response_convertToJSON(maintenance_clear_response_t *maintenance_clear_response) {
    cJSON *item = cJSON_CreateObject();

    // maintenance_clear_response->status
    if(maintenance_clear_response->status) {
    if(cJSON_AddStringToObject(item, "status", maintenance_clear_response->status) == NULL) {
    goto fail; //String
    }
    }


    // maintenance_clear_response->project
    if(maintenance_clear_response->project) {
    if(cJSON_AddStringToObject(item, "project", maintenance_clear_response->project) == NULL) {
    goto fail; //String
    }
    }


    // maintenance_clear_response->chunks_deleted
    if(maintenance_clear_response->chunks_deleted) {
    if(cJSON_AddNumberToObject(item, "chunks_deleted", *maintenance_clear_response->chunks_deleted) == NULL) {
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

maintenance_clear_response_t *maintenance_clear_response_parseFromJSON(cJSON *maintenance_clear_responseJSON){

    maintenance_clear_response_t *maintenance_clear_response_local_var = NULL;

    char *status_local_str = NULL;

    char *project_local_str = NULL;

    // define the local variable for maintenance_clear_response->chunks_deleted
    int *chunks_deleted_local_var = NULL;

    // maintenance_clear_response->status
    cJSON *status = cJSON_GetObjectItemCaseSensitive(maintenance_clear_responseJSON, "status");
    if (cJSON_IsNull(status)) {
        status = NULL;
    }
    if (status) { 
    if(!cJSON_IsString(status) && !cJSON_IsNull(status))
    {
    goto end; //String
    }
    }

    // maintenance_clear_response->project
    cJSON *project = cJSON_GetObjectItemCaseSensitive(maintenance_clear_responseJSON, "project");
    if (cJSON_IsNull(project)) {
        project = NULL;
    }
    if (project) { 
    if(!cJSON_IsString(project) && !cJSON_IsNull(project))
    {
    goto end; //String
    }
    }

    // maintenance_clear_response->chunks_deleted
    cJSON *chunks_deleted = cJSON_GetObjectItemCaseSensitive(maintenance_clear_responseJSON, "chunks_deleted");
    if (cJSON_IsNull(chunks_deleted)) {
        chunks_deleted = NULL;
    }
    if (chunks_deleted) { 
    if(!cJSON_IsNumber(chunks_deleted))
    {
    goto end; //Numeric
    }
    chunks_deleted_local_var = malloc(sizeof(int));
    if(!chunks_deleted_local_var)
    {
        goto end;
    }
    *chunks_deleted_local_var = chunks_deleted->valuedouble;
    }


    if (status && !cJSON_IsNull(status)) status_local_str = strdup(status->valuestring);
    if (project && !cJSON_IsNull(project)) project_local_str = strdup(project->valuestring);

    maintenance_clear_response_local_var = maintenance_clear_response_create_internal (
        status_local_str,
        project_local_str,
        chunks_deleted_local_var
        );

    if (!maintenance_clear_response_local_var) {
        goto end;
    }

    return maintenance_clear_response_local_var;
end:
    if (status_local_str) {
        free(status_local_str);
        status_local_str = NULL;
    }
    if (project_local_str) {
        free(project_local_str);
        project_local_str = NULL;
    }
    if (chunks_deleted_local_var) {
        free(chunks_deleted_local_var);
        chunks_deleted_local_var = NULL;
    }
    return NULL;

}
