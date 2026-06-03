#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "job_status_response.h"


char* job_status_response_status_ToString(aimee_kb_api_job_status_response_STATUS_e status) {
    char* statusArray[] =  { "NULL", "pending", "running", "done", "failed" };
    return statusArray[status];
}

aimee_kb_api_job_status_response_STATUS_e job_status_response_status_FromString(char* status){
    int stringToReturn = 0;
    char *statusArray[] =  { "NULL", "pending", "running", "done", "failed" };
    size_t sizeofArray = sizeof(statusArray) / sizeof(statusArray[0]);
    while(stringToReturn < sizeofArray) {
        if(strcmp(status, statusArray[stringToReturn]) == 0) {
            return stringToReturn;
        }
        stringToReturn++;
    }
    return 0;
}

static job_status_response_t *job_status_response_create_internal(
    long *id,
    char *kind,
    long *document_id,
    char *project,
    aimee_kb_api_job_status_response_STATUS_e status,
    int *attempts,
    char *last_error,
    char *claimed_by,
    char *claimed_at,
    char *created_at,
    char *updated_at
    ) {
    job_status_response_t *job_status_response_local_var = malloc(sizeof(job_status_response_t));
    if (!job_status_response_local_var) {
        return NULL;
    }
    memset(job_status_response_local_var, 0, sizeof(job_status_response_t));
    job_status_response_local_var->_library_owned = 1;
    job_status_response_local_var->id = id;
    job_status_response_local_var->kind = kind;
    job_status_response_local_var->document_id = document_id;
    job_status_response_local_var->project = project;
    job_status_response_local_var->status = status;
    job_status_response_local_var->attempts = attempts;
    job_status_response_local_var->last_error = last_error;
    job_status_response_local_var->claimed_by = claimed_by;
    job_status_response_local_var->claimed_at = claimed_at;
    job_status_response_local_var->created_at = created_at;
    job_status_response_local_var->updated_at = updated_at;
    return job_status_response_local_var;
}

__attribute__((deprecated)) job_status_response_t *job_status_response_create(
    long *id,
    char *kind,
    long *document_id,
    char *project,
    aimee_kb_api_job_status_response_STATUS_e status,
    int *attempts,
    char *last_error,
    char *claimed_by,
    char *claimed_at,
    char *created_at,
    char *updated_at
    ) {
    long *id_copy = NULL;
    if (id) {
        id_copy = malloc(sizeof(long));
        if (id_copy) *id_copy = *id;
    }
    long *document_id_copy = NULL;
    if (document_id) {
        document_id_copy = malloc(sizeof(long));
        if (document_id_copy) *document_id_copy = *document_id;
    }
    int *attempts_copy = NULL;
    if (attempts) {
        attempts_copy = malloc(sizeof(int));
        if (attempts_copy) *attempts_copy = *attempts;
    }
    job_status_response_t *result = job_status_response_create_internal (
        id_copy,
        kind,
        document_id_copy,
        project,
        status,
        attempts_copy,
        last_error,
        claimed_by,
        claimed_at,
        created_at,
        updated_at
        );
    if (!result) {
        free(id_copy);
        free(document_id_copy);
        free(attempts_copy);
    }
    return result;
}

void job_status_response_free(job_status_response_t *job_status_response) {
    if(NULL == job_status_response){
        return ;
    }
    if(job_status_response->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "job_status_response_free");
        return ;
    }
    listEntry_t *listEntry;
    if (job_status_response->id) {
        free(job_status_response->id);
        job_status_response->id = NULL;
    }
    if (job_status_response->kind) {
        free(job_status_response->kind);
        job_status_response->kind = NULL;
    }
    if (job_status_response->document_id) {
        free(job_status_response->document_id);
        job_status_response->document_id = NULL;
    }
    if (job_status_response->project) {
        free(job_status_response->project);
        job_status_response->project = NULL;
    }
    if (job_status_response->attempts) {
        free(job_status_response->attempts);
        job_status_response->attempts = NULL;
    }
    if (job_status_response->last_error) {
        free(job_status_response->last_error);
        job_status_response->last_error = NULL;
    }
    if (job_status_response->claimed_by) {
        free(job_status_response->claimed_by);
        job_status_response->claimed_by = NULL;
    }
    if (job_status_response->claimed_at) {
        free(job_status_response->claimed_at);
        job_status_response->claimed_at = NULL;
    }
    if (job_status_response->created_at) {
        free(job_status_response->created_at);
        job_status_response->created_at = NULL;
    }
    if (job_status_response->updated_at) {
        free(job_status_response->updated_at);
        job_status_response->updated_at = NULL;
    }
    free(job_status_response);
}

cJSON *job_status_response_convertToJSON(job_status_response_t *job_status_response) {
    cJSON *item = cJSON_CreateObject();

    // job_status_response->id
    if (!job_status_response->id) {
        goto fail;
    }
    if(cJSON_AddNumberToObject(item, "id", *job_status_response->id) == NULL) {
    goto fail; //Numeric
    }


    // job_status_response->kind
    if (!job_status_response->kind) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "kind", job_status_response->kind) == NULL) {
    goto fail; //String
    }


    // job_status_response->document_id
    if (!job_status_response->document_id) {
        goto fail;
    }
    if(cJSON_AddNumberToObject(item, "document_id", *job_status_response->document_id) == NULL) {
    goto fail; //Numeric
    }


    // job_status_response->project
    if (!job_status_response->project) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "project", job_status_response->project) == NULL) {
    goto fail; //String
    }


    // job_status_response->status
    if (aimee_kb_api_job_status_response_STATUS_NULL == job_status_response->status) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "status", job_status_response_status_ToString(job_status_response->status)) == NULL)
    {
    goto fail; //Enum
    }


    // job_status_response->attempts
    if (!job_status_response->attempts) {
        goto fail;
    }
    if(cJSON_AddNumberToObject(item, "attempts", *job_status_response->attempts) == NULL) {
    goto fail; //Numeric
    }


    // job_status_response->last_error
    if(job_status_response->last_error) {
    if(cJSON_AddStringToObject(item, "last_error", job_status_response->last_error) == NULL) {
    goto fail; //String
    }
    }


    // job_status_response->claimed_by
    if(job_status_response->claimed_by) {
    if(cJSON_AddStringToObject(item, "claimed_by", job_status_response->claimed_by) == NULL) {
    goto fail; //String
    }
    }


    // job_status_response->claimed_at
    if(job_status_response->claimed_at) {
    if(cJSON_AddStringToObject(item, "claimed_at", job_status_response->claimed_at) == NULL) {
    goto fail; //String
    }
    }


    // job_status_response->created_at
    if(job_status_response->created_at) {
    if(cJSON_AddStringToObject(item, "created_at", job_status_response->created_at) == NULL) {
    goto fail; //String
    }
    }


    // job_status_response->updated_at
    if(job_status_response->updated_at) {
    if(cJSON_AddStringToObject(item, "updated_at", job_status_response->updated_at) == NULL) {
    goto fail; //String
    }
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

job_status_response_t *job_status_response_parseFromJSON(cJSON *job_status_responseJSON){

    job_status_response_t *job_status_response_local_var = NULL;

    // define the local variable for job_status_response->id
    long *id_local_var = NULL;

    char *kind_local_str = NULL;

    // define the local variable for job_status_response->document_id
    long *document_id_local_var = NULL;

    char *project_local_str = NULL;

    // define the local variable for job_status_response->attempts
    int *attempts_local_var = NULL;

    char *last_error_local_str = NULL;

    char *claimed_by_local_str = NULL;

    char *claimed_at_local_str = NULL;

    char *created_at_local_str = NULL;

    char *updated_at_local_str = NULL;

    // job_status_response->id
    cJSON *id = cJSON_GetObjectItemCaseSensitive(job_status_responseJSON, "id");
    if (cJSON_IsNull(id)) {
        id = NULL;
    }
    if (!id) {
        goto end;
    }

    
    if(!cJSON_IsNumber(id))
    {
    goto end; //Numeric
    }
    id_local_var = malloc(sizeof(long));
    if(!id_local_var)
    {
        goto end;
    }
    *id_local_var = id->valuedouble;

    // job_status_response->kind
    cJSON *kind = cJSON_GetObjectItemCaseSensitive(job_status_responseJSON, "kind");
    if (cJSON_IsNull(kind)) {
        kind = NULL;
    }
    if (!kind) {
        goto end;
    }

    
    if(!cJSON_IsString(kind))
    {
    goto end; //String
    }

    // job_status_response->document_id
    cJSON *document_id = cJSON_GetObjectItemCaseSensitive(job_status_responseJSON, "document_id");
    if (cJSON_IsNull(document_id)) {
        document_id = NULL;
    }
    if (!document_id) {
        goto end;
    }

    
    if(!cJSON_IsNumber(document_id))
    {
    goto end; //Numeric
    }
    document_id_local_var = malloc(sizeof(long));
    if(!document_id_local_var)
    {
        goto end;
    }
    *document_id_local_var = document_id->valuedouble;

    // job_status_response->project
    cJSON *project = cJSON_GetObjectItemCaseSensitive(job_status_responseJSON, "project");
    if (cJSON_IsNull(project)) {
        project = NULL;
    }
    if (!project) {
        goto end;
    }

    
    if(!cJSON_IsString(project))
    {
    goto end; //String
    }

    // job_status_response->status
    cJSON *status = cJSON_GetObjectItemCaseSensitive(job_status_responseJSON, "status");
    if (cJSON_IsNull(status)) {
        status = NULL;
    }
    if (!status) {
        goto end;
    }

    aimee_kb_api_job_status_response_STATUS_e statusVariable;
    
    if(!cJSON_IsString(status))
    {
    goto end; //Enum
    }
    statusVariable = job_status_response_status_FromString(status->valuestring);

    // job_status_response->attempts
    cJSON *attempts = cJSON_GetObjectItemCaseSensitive(job_status_responseJSON, "attempts");
    if (cJSON_IsNull(attempts)) {
        attempts = NULL;
    }
    if (!attempts) {
        goto end;
    }

    
    if(!cJSON_IsNumber(attempts))
    {
    goto end; //Numeric
    }
    attempts_local_var = malloc(sizeof(int));
    if(!attempts_local_var)
    {
        goto end;
    }
    *attempts_local_var = attempts->valuedouble;

    // job_status_response->last_error
    cJSON *last_error = cJSON_GetObjectItemCaseSensitive(job_status_responseJSON, "last_error");
    if (cJSON_IsNull(last_error)) {
        last_error = NULL;
    }
    if (last_error) { 
    if(!cJSON_IsString(last_error) && !cJSON_IsNull(last_error))
    {
    goto end; //String
    }
    }

    // job_status_response->claimed_by
    cJSON *claimed_by = cJSON_GetObjectItemCaseSensitive(job_status_responseJSON, "claimed_by");
    if (cJSON_IsNull(claimed_by)) {
        claimed_by = NULL;
    }
    if (claimed_by) { 
    if(!cJSON_IsString(claimed_by) && !cJSON_IsNull(claimed_by))
    {
    goto end; //String
    }
    }

    // job_status_response->claimed_at
    cJSON *claimed_at = cJSON_GetObjectItemCaseSensitive(job_status_responseJSON, "claimed_at");
    if (cJSON_IsNull(claimed_at)) {
        claimed_at = NULL;
    }
    if (claimed_at) { 
    if(!cJSON_IsString(claimed_at) && !cJSON_IsNull(claimed_at))
    {
    goto end; //String
    }
    }

    // job_status_response->created_at
    cJSON *created_at = cJSON_GetObjectItemCaseSensitive(job_status_responseJSON, "created_at");
    if (cJSON_IsNull(created_at)) {
        created_at = NULL;
    }
    if (created_at) { 
    if(!cJSON_IsString(created_at) && !cJSON_IsNull(created_at))
    {
    goto end; //String
    }
    }

    // job_status_response->updated_at
    cJSON *updated_at = cJSON_GetObjectItemCaseSensitive(job_status_responseJSON, "updated_at");
    if (cJSON_IsNull(updated_at)) {
        updated_at = NULL;
    }
    if (updated_at) { 
    if(!cJSON_IsString(updated_at) && !cJSON_IsNull(updated_at))
    {
    goto end; //String
    }
    }


    if (kind && !cJSON_IsNull(kind)) kind_local_str = strdup(kind->valuestring);
    if (project && !cJSON_IsNull(project)) project_local_str = strdup(project->valuestring);
    if (last_error && !cJSON_IsNull(last_error)) last_error_local_str = strdup(last_error->valuestring);
    if (claimed_by && !cJSON_IsNull(claimed_by)) claimed_by_local_str = strdup(claimed_by->valuestring);
    if (claimed_at && !cJSON_IsNull(claimed_at)) claimed_at_local_str = strdup(claimed_at->valuestring);
    if (created_at && !cJSON_IsNull(created_at)) created_at_local_str = strdup(created_at->valuestring);
    if (updated_at && !cJSON_IsNull(updated_at)) updated_at_local_str = strdup(updated_at->valuestring);

    job_status_response_local_var = job_status_response_create_internal (
        id_local_var,
        kind_local_str,
        document_id_local_var,
        project_local_str,
        statusVariable,
        attempts_local_var,
        last_error_local_str,
        claimed_by_local_str,
        claimed_at_local_str,
        created_at_local_str,
        updated_at_local_str
        );

    if (!job_status_response_local_var) {
        goto end;
    }

    return job_status_response_local_var;
end:
    if (id_local_var) {
        free(id_local_var);
        id_local_var = NULL;
    }
    if (kind_local_str) {
        free(kind_local_str);
        kind_local_str = NULL;
    }
    if (document_id_local_var) {
        free(document_id_local_var);
        document_id_local_var = NULL;
    }
    if (project_local_str) {
        free(project_local_str);
        project_local_str = NULL;
    }
    if (attempts_local_var) {
        free(attempts_local_var);
        attempts_local_var = NULL;
    }
    if (last_error_local_str) {
        free(last_error_local_str);
        last_error_local_str = NULL;
    }
    if (claimed_by_local_str) {
        free(claimed_by_local_str);
        claimed_by_local_str = NULL;
    }
    if (claimed_at_local_str) {
        free(claimed_at_local_str);
        claimed_at_local_str = NULL;
    }
    if (created_at_local_str) {
        free(created_at_local_str);
        created_at_local_str = NULL;
    }
    if (updated_at_local_str) {
        free(updated_at_local_str);
        updated_at_local_str = NULL;
    }
    return NULL;

}
