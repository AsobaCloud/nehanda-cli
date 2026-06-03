#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "ingest_request.h"



static ingest_request_t *ingest_request_create_internal(
    char *workspace,
    char *embedding_command,
    int *force
    ) {
    ingest_request_t *ingest_request_local_var = malloc(sizeof(ingest_request_t));
    if (!ingest_request_local_var) {
        return NULL;
    }
    memset(ingest_request_local_var, 0, sizeof(ingest_request_t));
    ingest_request_local_var->_library_owned = 1;
    ingest_request_local_var->workspace = workspace;
    ingest_request_local_var->embedding_command = embedding_command;
    ingest_request_local_var->force = force;
    return ingest_request_local_var;
}

__attribute__((deprecated)) ingest_request_t *ingest_request_create(
    char *workspace,
    char *embedding_command,
    int *force
    ) {
    int *force_copy = NULL;
    if (force) {
        force_copy = malloc(sizeof(int));
        if (force_copy) *force_copy = *force;
    }
    ingest_request_t *result = ingest_request_create_internal (
        workspace,
        embedding_command,
        force_copy
        );
    if (!result) {
        free(force_copy);
    }
    return result;
}

void ingest_request_free(ingest_request_t *ingest_request) {
    if(NULL == ingest_request){
        return ;
    }
    if(ingest_request->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "ingest_request_free");
        return ;
    }
    listEntry_t *listEntry;
    if (ingest_request->workspace) {
        free(ingest_request->workspace);
        ingest_request->workspace = NULL;
    }
    if (ingest_request->embedding_command) {
        free(ingest_request->embedding_command);
        ingest_request->embedding_command = NULL;
    }
    if (ingest_request->force) {
        free(ingest_request->force);
        ingest_request->force = NULL;
    }
    free(ingest_request);
}

cJSON *ingest_request_convertToJSON(ingest_request_t *ingest_request) {
    cJSON *item = cJSON_CreateObject();

    // ingest_request->workspace
    if(ingest_request->workspace) {
    if(cJSON_AddStringToObject(item, "workspace", ingest_request->workspace) == NULL) {
    goto fail; //String
    }
    }


    // ingest_request->embedding_command
    if(ingest_request->embedding_command) {
    if(cJSON_AddStringToObject(item, "embedding_command", ingest_request->embedding_command) == NULL) {
    goto fail; //String
    }
    }


    // ingest_request->force
    if(ingest_request->force) {
    if(cJSON_AddBoolToObject(item, "force", *ingest_request->force) == NULL) {
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

ingest_request_t *ingest_request_parseFromJSON(cJSON *ingest_requestJSON){

    ingest_request_t *ingest_request_local_var = NULL;

    char *workspace_local_str = NULL;

    char *embedding_command_local_str = NULL;

    // define the local variable for ingest_request->force
    int *force_local_var = NULL;

    // ingest_request->workspace
    cJSON *workspace = cJSON_GetObjectItemCaseSensitive(ingest_requestJSON, "workspace");
    if (cJSON_IsNull(workspace)) {
        workspace = NULL;
    }
    if (workspace) { 
    if(!cJSON_IsString(workspace) && !cJSON_IsNull(workspace))
    {
    goto end; //String
    }
    }

    // ingest_request->embedding_command
    cJSON *embedding_command = cJSON_GetObjectItemCaseSensitive(ingest_requestJSON, "embedding_command");
    if (cJSON_IsNull(embedding_command)) {
        embedding_command = NULL;
    }
    if (embedding_command) { 
    if(!cJSON_IsString(embedding_command) && !cJSON_IsNull(embedding_command))
    {
    goto end; //String
    }
    }

    // ingest_request->force
    cJSON *force = cJSON_GetObjectItemCaseSensitive(ingest_requestJSON, "force");
    if (cJSON_IsNull(force)) {
        force = NULL;
    }
    if (force) { 
    if(!cJSON_IsBool(force))
    {
    goto end; //Bool
    }
    force_local_var = malloc(sizeof(int));
    if(!force_local_var)
    {
        goto end;
    }
    *force_local_var = force->valueint;
    }


    if (workspace && !cJSON_IsNull(workspace)) workspace_local_str = strdup(workspace->valuestring);
    if (embedding_command && !cJSON_IsNull(embedding_command)) embedding_command_local_str = strdup(embedding_command->valuestring);

    ingest_request_local_var = ingest_request_create_internal (
        workspace_local_str,
        embedding_command_local_str,
        force_local_var
        );

    if (!ingest_request_local_var) {
        goto end;
    }

    return ingest_request_local_var;
end:
    if (workspace_local_str) {
        free(workspace_local_str);
        workspace_local_str = NULL;
    }
    if (embedding_command_local_str) {
        free(embedding_command_local_str);
        embedding_command_local_str = NULL;
    }
    if (force_local_var) {
        free(force_local_var);
        force_local_var = NULL;
    }
    return NULL;

}
