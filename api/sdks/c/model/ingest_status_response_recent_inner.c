#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "ingest_status_response_recent_inner.h"



static ingest_status_response_recent_inner_t *ingest_status_response_recent_inner_create_internal(
    char *project,
    char *status,
    char *completed_at,
    int *files_indexed,
    int *chunks_added,
    char *error
    ) {
    ingest_status_response_recent_inner_t *ingest_status_response_recent_inner_local_var = malloc(sizeof(ingest_status_response_recent_inner_t));
    if (!ingest_status_response_recent_inner_local_var) {
        return NULL;
    }
    memset(ingest_status_response_recent_inner_local_var, 0, sizeof(ingest_status_response_recent_inner_t));
    ingest_status_response_recent_inner_local_var->_library_owned = 1;
    ingest_status_response_recent_inner_local_var->project = project;
    ingest_status_response_recent_inner_local_var->status = status;
    ingest_status_response_recent_inner_local_var->completed_at = completed_at;
    ingest_status_response_recent_inner_local_var->files_indexed = files_indexed;
    ingest_status_response_recent_inner_local_var->chunks_added = chunks_added;
    ingest_status_response_recent_inner_local_var->error = error;
    return ingest_status_response_recent_inner_local_var;
}

__attribute__((deprecated)) ingest_status_response_recent_inner_t *ingest_status_response_recent_inner_create(
    char *project,
    char *status,
    char *completed_at,
    int *files_indexed,
    int *chunks_added,
    char *error
    ) {
    int *files_indexed_copy = NULL;
    if (files_indexed) {
        files_indexed_copy = malloc(sizeof(int));
        if (files_indexed_copy) *files_indexed_copy = *files_indexed;
    }
    int *chunks_added_copy = NULL;
    if (chunks_added) {
        chunks_added_copy = malloc(sizeof(int));
        if (chunks_added_copy) *chunks_added_copy = *chunks_added;
    }
    ingest_status_response_recent_inner_t *result = ingest_status_response_recent_inner_create_internal (
        project,
        status,
        completed_at,
        files_indexed_copy,
        chunks_added_copy,
        error
        );
    if (!result) {
        free(files_indexed_copy);
        free(chunks_added_copy);
    }
    return result;
}

void ingest_status_response_recent_inner_free(ingest_status_response_recent_inner_t *ingest_status_response_recent_inner) {
    if(NULL == ingest_status_response_recent_inner){
        return ;
    }
    if(ingest_status_response_recent_inner->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "ingest_status_response_recent_inner_free");
        return ;
    }
    listEntry_t *listEntry;
    if (ingest_status_response_recent_inner->project) {
        free(ingest_status_response_recent_inner->project);
        ingest_status_response_recent_inner->project = NULL;
    }
    if (ingest_status_response_recent_inner->status) {
        free(ingest_status_response_recent_inner->status);
        ingest_status_response_recent_inner->status = NULL;
    }
    if (ingest_status_response_recent_inner->completed_at) {
        free(ingest_status_response_recent_inner->completed_at);
        ingest_status_response_recent_inner->completed_at = NULL;
    }
    if (ingest_status_response_recent_inner->files_indexed) {
        free(ingest_status_response_recent_inner->files_indexed);
        ingest_status_response_recent_inner->files_indexed = NULL;
    }
    if (ingest_status_response_recent_inner->chunks_added) {
        free(ingest_status_response_recent_inner->chunks_added);
        ingest_status_response_recent_inner->chunks_added = NULL;
    }
    if (ingest_status_response_recent_inner->error) {
        free(ingest_status_response_recent_inner->error);
        ingest_status_response_recent_inner->error = NULL;
    }
    free(ingest_status_response_recent_inner);
}

cJSON *ingest_status_response_recent_inner_convertToJSON(ingest_status_response_recent_inner_t *ingest_status_response_recent_inner) {
    cJSON *item = cJSON_CreateObject();

    // ingest_status_response_recent_inner->project
    if(ingest_status_response_recent_inner->project) {
    if(cJSON_AddStringToObject(item, "project", ingest_status_response_recent_inner->project) == NULL) {
    goto fail; //String
    }
    }


    // ingest_status_response_recent_inner->status
    if(ingest_status_response_recent_inner->status) {
    if(cJSON_AddStringToObject(item, "status", ingest_status_response_recent_inner->status) == NULL) {
    goto fail; //String
    }
    }


    // ingest_status_response_recent_inner->completed_at
    if(ingest_status_response_recent_inner->completed_at) {
    if(cJSON_AddStringToObject(item, "completed_at", ingest_status_response_recent_inner->completed_at) == NULL) {
    goto fail; //String
    }
    }


    // ingest_status_response_recent_inner->files_indexed
    if(ingest_status_response_recent_inner->files_indexed) {
    if(cJSON_AddNumberToObject(item, "files_indexed", *ingest_status_response_recent_inner->files_indexed) == NULL) {
    goto fail; //Numeric
    }
    }


    // ingest_status_response_recent_inner->chunks_added
    if(ingest_status_response_recent_inner->chunks_added) {
    if(cJSON_AddNumberToObject(item, "chunks_added", *ingest_status_response_recent_inner->chunks_added) == NULL) {
    goto fail; //Numeric
    }
    }


    // ingest_status_response_recent_inner->error
    if(ingest_status_response_recent_inner->error) {
    if(cJSON_AddStringToObject(item, "error", ingest_status_response_recent_inner->error) == NULL) {
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

ingest_status_response_recent_inner_t *ingest_status_response_recent_inner_parseFromJSON(cJSON *ingest_status_response_recent_innerJSON){

    ingest_status_response_recent_inner_t *ingest_status_response_recent_inner_local_var = NULL;

    char *project_local_str = NULL;

    char *status_local_str = NULL;

    char *completed_at_local_str = NULL;

    // define the local variable for ingest_status_response_recent_inner->files_indexed
    int *files_indexed_local_var = NULL;

    // define the local variable for ingest_status_response_recent_inner->chunks_added
    int *chunks_added_local_var = NULL;

    char *error_local_str = NULL;

    // ingest_status_response_recent_inner->project
    cJSON *project = cJSON_GetObjectItemCaseSensitive(ingest_status_response_recent_innerJSON, "project");
    if (cJSON_IsNull(project)) {
        project = NULL;
    }
    if (project) { 
    if(!cJSON_IsString(project) && !cJSON_IsNull(project))
    {
    goto end; //String
    }
    }

    // ingest_status_response_recent_inner->status
    cJSON *status = cJSON_GetObjectItemCaseSensitive(ingest_status_response_recent_innerJSON, "status");
    if (cJSON_IsNull(status)) {
        status = NULL;
    }
    if (status) { 
    if(!cJSON_IsString(status) && !cJSON_IsNull(status))
    {
    goto end; //String
    }
    }

    // ingest_status_response_recent_inner->completed_at
    cJSON *completed_at = cJSON_GetObjectItemCaseSensitive(ingest_status_response_recent_innerJSON, "completed_at");
    if (cJSON_IsNull(completed_at)) {
        completed_at = NULL;
    }
    if (completed_at) { 
    if(!cJSON_IsString(completed_at) && !cJSON_IsNull(completed_at))
    {
    goto end; //String
    }
    }

    // ingest_status_response_recent_inner->files_indexed
    cJSON *files_indexed = cJSON_GetObjectItemCaseSensitive(ingest_status_response_recent_innerJSON, "files_indexed");
    if (cJSON_IsNull(files_indexed)) {
        files_indexed = NULL;
    }
    if (files_indexed) { 
    if(!cJSON_IsNumber(files_indexed))
    {
    goto end; //Numeric
    }
    files_indexed_local_var = malloc(sizeof(int));
    if(!files_indexed_local_var)
    {
        goto end;
    }
    *files_indexed_local_var = files_indexed->valuedouble;
    }

    // ingest_status_response_recent_inner->chunks_added
    cJSON *chunks_added = cJSON_GetObjectItemCaseSensitive(ingest_status_response_recent_innerJSON, "chunks_added");
    if (cJSON_IsNull(chunks_added)) {
        chunks_added = NULL;
    }
    if (chunks_added) { 
    if(!cJSON_IsNumber(chunks_added))
    {
    goto end; //Numeric
    }
    chunks_added_local_var = malloc(sizeof(int));
    if(!chunks_added_local_var)
    {
        goto end;
    }
    *chunks_added_local_var = chunks_added->valuedouble;
    }

    // ingest_status_response_recent_inner->error
    cJSON *error = cJSON_GetObjectItemCaseSensitive(ingest_status_response_recent_innerJSON, "error");
    if (cJSON_IsNull(error)) {
        error = NULL;
    }
    if (error) { 
    if(!cJSON_IsString(error) && !cJSON_IsNull(error))
    {
    goto end; //String
    }
    }


    if (project && !cJSON_IsNull(project)) project_local_str = strdup(project->valuestring);
    if (status && !cJSON_IsNull(status)) status_local_str = strdup(status->valuestring);
    if (completed_at && !cJSON_IsNull(completed_at)) completed_at_local_str = strdup(completed_at->valuestring);
    if (error && !cJSON_IsNull(error)) error_local_str = strdup(error->valuestring);

    ingest_status_response_recent_inner_local_var = ingest_status_response_recent_inner_create_internal (
        project_local_str,
        status_local_str,
        completed_at_local_str,
        files_indexed_local_var,
        chunks_added_local_var,
        error_local_str
        );

    if (!ingest_status_response_recent_inner_local_var) {
        goto end;
    }

    return ingest_status_response_recent_inner_local_var;
end:
    if (project_local_str) {
        free(project_local_str);
        project_local_str = NULL;
    }
    if (status_local_str) {
        free(status_local_str);
        status_local_str = NULL;
    }
    if (completed_at_local_str) {
        free(completed_at_local_str);
        completed_at_local_str = NULL;
    }
    if (files_indexed_local_var) {
        free(files_indexed_local_var);
        files_indexed_local_var = NULL;
    }
    if (chunks_added_local_var) {
        free(chunks_added_local_var);
        chunks_added_local_var = NULL;
    }
    if (error_local_str) {
        free(error_local_str);
        error_local_str = NULL;
    }
    return NULL;

}
