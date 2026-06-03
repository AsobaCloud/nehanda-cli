#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "maintenance_repair_response.h"



static maintenance_repair_response_t *maintenance_repair_response_create_internal(
    char *status,
    char *project,
    int *files_scanned,
    int *files_indexed,
    int *files_skipped,
    int *files_removed,
    int *chunks_added,
    int *chunks_removed,
    int *embeddings_added
    ) {
    maintenance_repair_response_t *maintenance_repair_response_local_var = malloc(sizeof(maintenance_repair_response_t));
    if (!maintenance_repair_response_local_var) {
        return NULL;
    }
    memset(maintenance_repair_response_local_var, 0, sizeof(maintenance_repair_response_t));
    maintenance_repair_response_local_var->_library_owned = 1;
    maintenance_repair_response_local_var->status = status;
    maintenance_repair_response_local_var->project = project;
    maintenance_repair_response_local_var->files_scanned = files_scanned;
    maintenance_repair_response_local_var->files_indexed = files_indexed;
    maintenance_repair_response_local_var->files_skipped = files_skipped;
    maintenance_repair_response_local_var->files_removed = files_removed;
    maintenance_repair_response_local_var->chunks_added = chunks_added;
    maintenance_repair_response_local_var->chunks_removed = chunks_removed;
    maintenance_repair_response_local_var->embeddings_added = embeddings_added;
    return maintenance_repair_response_local_var;
}

__attribute__((deprecated)) maintenance_repair_response_t *maintenance_repair_response_create(
    char *status,
    char *project,
    int *files_scanned,
    int *files_indexed,
    int *files_skipped,
    int *files_removed,
    int *chunks_added,
    int *chunks_removed,
    int *embeddings_added
    ) {
    int *files_scanned_copy = NULL;
    if (files_scanned) {
        files_scanned_copy = malloc(sizeof(int));
        if (files_scanned_copy) *files_scanned_copy = *files_scanned;
    }
    int *files_indexed_copy = NULL;
    if (files_indexed) {
        files_indexed_copy = malloc(sizeof(int));
        if (files_indexed_copy) *files_indexed_copy = *files_indexed;
    }
    int *files_skipped_copy = NULL;
    if (files_skipped) {
        files_skipped_copy = malloc(sizeof(int));
        if (files_skipped_copy) *files_skipped_copy = *files_skipped;
    }
    int *files_removed_copy = NULL;
    if (files_removed) {
        files_removed_copy = malloc(sizeof(int));
        if (files_removed_copy) *files_removed_copy = *files_removed;
    }
    int *chunks_added_copy = NULL;
    if (chunks_added) {
        chunks_added_copy = malloc(sizeof(int));
        if (chunks_added_copy) *chunks_added_copy = *chunks_added;
    }
    int *chunks_removed_copy = NULL;
    if (chunks_removed) {
        chunks_removed_copy = malloc(sizeof(int));
        if (chunks_removed_copy) *chunks_removed_copy = *chunks_removed;
    }
    int *embeddings_added_copy = NULL;
    if (embeddings_added) {
        embeddings_added_copy = malloc(sizeof(int));
        if (embeddings_added_copy) *embeddings_added_copy = *embeddings_added;
    }
    maintenance_repair_response_t *result = maintenance_repair_response_create_internal (
        status,
        project,
        files_scanned_copy,
        files_indexed_copy,
        files_skipped_copy,
        files_removed_copy,
        chunks_added_copy,
        chunks_removed_copy,
        embeddings_added_copy
        );
    if (!result) {
        free(files_scanned_copy);
        free(files_indexed_copy);
        free(files_skipped_copy);
        free(files_removed_copy);
        free(chunks_added_copy);
        free(chunks_removed_copy);
        free(embeddings_added_copy);
    }
    return result;
}

void maintenance_repair_response_free(maintenance_repair_response_t *maintenance_repair_response) {
    if(NULL == maintenance_repair_response){
        return ;
    }
    if(maintenance_repair_response->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "maintenance_repair_response_free");
        return ;
    }
    listEntry_t *listEntry;
    if (maintenance_repair_response->status) {
        free(maintenance_repair_response->status);
        maintenance_repair_response->status = NULL;
    }
    if (maintenance_repair_response->project) {
        free(maintenance_repair_response->project);
        maintenance_repair_response->project = NULL;
    }
    if (maintenance_repair_response->files_scanned) {
        free(maintenance_repair_response->files_scanned);
        maintenance_repair_response->files_scanned = NULL;
    }
    if (maintenance_repair_response->files_indexed) {
        free(maintenance_repair_response->files_indexed);
        maintenance_repair_response->files_indexed = NULL;
    }
    if (maintenance_repair_response->files_skipped) {
        free(maintenance_repair_response->files_skipped);
        maintenance_repair_response->files_skipped = NULL;
    }
    if (maintenance_repair_response->files_removed) {
        free(maintenance_repair_response->files_removed);
        maintenance_repair_response->files_removed = NULL;
    }
    if (maintenance_repair_response->chunks_added) {
        free(maintenance_repair_response->chunks_added);
        maintenance_repair_response->chunks_added = NULL;
    }
    if (maintenance_repair_response->chunks_removed) {
        free(maintenance_repair_response->chunks_removed);
        maintenance_repair_response->chunks_removed = NULL;
    }
    if (maintenance_repair_response->embeddings_added) {
        free(maintenance_repair_response->embeddings_added);
        maintenance_repair_response->embeddings_added = NULL;
    }
    free(maintenance_repair_response);
}

cJSON *maintenance_repair_response_convertToJSON(maintenance_repair_response_t *maintenance_repair_response) {
    cJSON *item = cJSON_CreateObject();

    // maintenance_repair_response->status
    if(maintenance_repair_response->status) {
    if(cJSON_AddStringToObject(item, "status", maintenance_repair_response->status) == NULL) {
    goto fail; //String
    }
    }


    // maintenance_repair_response->project
    if(maintenance_repair_response->project) {
    if(cJSON_AddStringToObject(item, "project", maintenance_repair_response->project) == NULL) {
    goto fail; //String
    }
    }


    // maintenance_repair_response->files_scanned
    if(maintenance_repair_response->files_scanned) {
    if(cJSON_AddNumberToObject(item, "files_scanned", *maintenance_repair_response->files_scanned) == NULL) {
    goto fail; //Numeric
    }
    }


    // maintenance_repair_response->files_indexed
    if(maintenance_repair_response->files_indexed) {
    if(cJSON_AddNumberToObject(item, "files_indexed", *maintenance_repair_response->files_indexed) == NULL) {
    goto fail; //Numeric
    }
    }


    // maintenance_repair_response->files_skipped
    if(maintenance_repair_response->files_skipped) {
    if(cJSON_AddNumberToObject(item, "files_skipped", *maintenance_repair_response->files_skipped) == NULL) {
    goto fail; //Numeric
    }
    }


    // maintenance_repair_response->files_removed
    if(maintenance_repair_response->files_removed) {
    if(cJSON_AddNumberToObject(item, "files_removed", *maintenance_repair_response->files_removed) == NULL) {
    goto fail; //Numeric
    }
    }


    // maintenance_repair_response->chunks_added
    if(maintenance_repair_response->chunks_added) {
    if(cJSON_AddNumberToObject(item, "chunks_added", *maintenance_repair_response->chunks_added) == NULL) {
    goto fail; //Numeric
    }
    }


    // maintenance_repair_response->chunks_removed
    if(maintenance_repair_response->chunks_removed) {
    if(cJSON_AddNumberToObject(item, "chunks_removed", *maintenance_repair_response->chunks_removed) == NULL) {
    goto fail; //Numeric
    }
    }


    // maintenance_repair_response->embeddings_added
    if(maintenance_repair_response->embeddings_added) {
    if(cJSON_AddNumberToObject(item, "embeddings_added", *maintenance_repair_response->embeddings_added) == NULL) {
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

maintenance_repair_response_t *maintenance_repair_response_parseFromJSON(cJSON *maintenance_repair_responseJSON){

    maintenance_repair_response_t *maintenance_repair_response_local_var = NULL;

    char *status_local_str = NULL;

    char *project_local_str = NULL;

    // define the local variable for maintenance_repair_response->files_scanned
    int *files_scanned_local_var = NULL;

    // define the local variable for maintenance_repair_response->files_indexed
    int *files_indexed_local_var = NULL;

    // define the local variable for maintenance_repair_response->files_skipped
    int *files_skipped_local_var = NULL;

    // define the local variable for maintenance_repair_response->files_removed
    int *files_removed_local_var = NULL;

    // define the local variable for maintenance_repair_response->chunks_added
    int *chunks_added_local_var = NULL;

    // define the local variable for maintenance_repair_response->chunks_removed
    int *chunks_removed_local_var = NULL;

    // define the local variable for maintenance_repair_response->embeddings_added
    int *embeddings_added_local_var = NULL;

    // maintenance_repair_response->status
    cJSON *status = cJSON_GetObjectItemCaseSensitive(maintenance_repair_responseJSON, "status");
    if (cJSON_IsNull(status)) {
        status = NULL;
    }
    if (status) { 
    if(!cJSON_IsString(status) && !cJSON_IsNull(status))
    {
    goto end; //String
    }
    }

    // maintenance_repair_response->project
    cJSON *project = cJSON_GetObjectItemCaseSensitive(maintenance_repair_responseJSON, "project");
    if (cJSON_IsNull(project)) {
        project = NULL;
    }
    if (project) { 
    if(!cJSON_IsString(project) && !cJSON_IsNull(project))
    {
    goto end; //String
    }
    }

    // maintenance_repair_response->files_scanned
    cJSON *files_scanned = cJSON_GetObjectItemCaseSensitive(maintenance_repair_responseJSON, "files_scanned");
    if (cJSON_IsNull(files_scanned)) {
        files_scanned = NULL;
    }
    if (files_scanned) { 
    if(!cJSON_IsNumber(files_scanned))
    {
    goto end; //Numeric
    }
    files_scanned_local_var = malloc(sizeof(int));
    if(!files_scanned_local_var)
    {
        goto end;
    }
    *files_scanned_local_var = files_scanned->valuedouble;
    }

    // maintenance_repair_response->files_indexed
    cJSON *files_indexed = cJSON_GetObjectItemCaseSensitive(maintenance_repair_responseJSON, "files_indexed");
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

    // maintenance_repair_response->files_skipped
    cJSON *files_skipped = cJSON_GetObjectItemCaseSensitive(maintenance_repair_responseJSON, "files_skipped");
    if (cJSON_IsNull(files_skipped)) {
        files_skipped = NULL;
    }
    if (files_skipped) { 
    if(!cJSON_IsNumber(files_skipped))
    {
    goto end; //Numeric
    }
    files_skipped_local_var = malloc(sizeof(int));
    if(!files_skipped_local_var)
    {
        goto end;
    }
    *files_skipped_local_var = files_skipped->valuedouble;
    }

    // maintenance_repair_response->files_removed
    cJSON *files_removed = cJSON_GetObjectItemCaseSensitive(maintenance_repair_responseJSON, "files_removed");
    if (cJSON_IsNull(files_removed)) {
        files_removed = NULL;
    }
    if (files_removed) { 
    if(!cJSON_IsNumber(files_removed))
    {
    goto end; //Numeric
    }
    files_removed_local_var = malloc(sizeof(int));
    if(!files_removed_local_var)
    {
        goto end;
    }
    *files_removed_local_var = files_removed->valuedouble;
    }

    // maintenance_repair_response->chunks_added
    cJSON *chunks_added = cJSON_GetObjectItemCaseSensitive(maintenance_repair_responseJSON, "chunks_added");
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

    // maintenance_repair_response->chunks_removed
    cJSON *chunks_removed = cJSON_GetObjectItemCaseSensitive(maintenance_repair_responseJSON, "chunks_removed");
    if (cJSON_IsNull(chunks_removed)) {
        chunks_removed = NULL;
    }
    if (chunks_removed) { 
    if(!cJSON_IsNumber(chunks_removed))
    {
    goto end; //Numeric
    }
    chunks_removed_local_var = malloc(sizeof(int));
    if(!chunks_removed_local_var)
    {
        goto end;
    }
    *chunks_removed_local_var = chunks_removed->valuedouble;
    }

    // maintenance_repair_response->embeddings_added
    cJSON *embeddings_added = cJSON_GetObjectItemCaseSensitive(maintenance_repair_responseJSON, "embeddings_added");
    if (cJSON_IsNull(embeddings_added)) {
        embeddings_added = NULL;
    }
    if (embeddings_added) { 
    if(!cJSON_IsNumber(embeddings_added))
    {
    goto end; //Numeric
    }
    embeddings_added_local_var = malloc(sizeof(int));
    if(!embeddings_added_local_var)
    {
        goto end;
    }
    *embeddings_added_local_var = embeddings_added->valuedouble;
    }


    if (status && !cJSON_IsNull(status)) status_local_str = strdup(status->valuestring);
    if (project && !cJSON_IsNull(project)) project_local_str = strdup(project->valuestring);

    maintenance_repair_response_local_var = maintenance_repair_response_create_internal (
        status_local_str,
        project_local_str,
        files_scanned_local_var,
        files_indexed_local_var,
        files_skipped_local_var,
        files_removed_local_var,
        chunks_added_local_var,
        chunks_removed_local_var,
        embeddings_added_local_var
        );

    if (!maintenance_repair_response_local_var) {
        goto end;
    }

    return maintenance_repair_response_local_var;
end:
    if (status_local_str) {
        free(status_local_str);
        status_local_str = NULL;
    }
    if (project_local_str) {
        free(project_local_str);
        project_local_str = NULL;
    }
    if (files_scanned_local_var) {
        free(files_scanned_local_var);
        files_scanned_local_var = NULL;
    }
    if (files_indexed_local_var) {
        free(files_indexed_local_var);
        files_indexed_local_var = NULL;
    }
    if (files_skipped_local_var) {
        free(files_skipped_local_var);
        files_skipped_local_var = NULL;
    }
    if (files_removed_local_var) {
        free(files_removed_local_var);
        files_removed_local_var = NULL;
    }
    if (chunks_added_local_var) {
        free(chunks_added_local_var);
        chunks_added_local_var = NULL;
    }
    if (chunks_removed_local_var) {
        free(chunks_removed_local_var);
        chunks_removed_local_var = NULL;
    }
    if (embeddings_added_local_var) {
        free(embeddings_added_local_var);
        embeddings_added_local_var = NULL;
    }
    return NULL;

}
