#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "code_update_request.h"



static code_update_request_t *code_update_request_create_internal(
    char *path,
    char *project,
    char *embedding_command
    ) {
    code_update_request_t *code_update_request_local_var = malloc(sizeof(code_update_request_t));
    if (!code_update_request_local_var) {
        return NULL;
    }
    memset(code_update_request_local_var, 0, sizeof(code_update_request_t));
    code_update_request_local_var->_library_owned = 1;
    code_update_request_local_var->path = path;
    code_update_request_local_var->project = project;
    code_update_request_local_var->embedding_command = embedding_command;
    return code_update_request_local_var;
}

__attribute__((deprecated)) code_update_request_t *code_update_request_create(
    char *path,
    char *project,
    char *embedding_command
    ) {
    code_update_request_t *result = code_update_request_create_internal (
        path,
        project,
        embedding_command
        );
    if (!result) {
    }
    return result;
}

void code_update_request_free(code_update_request_t *code_update_request) {
    if(NULL == code_update_request){
        return ;
    }
    if(code_update_request->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "code_update_request_free");
        return ;
    }
    listEntry_t *listEntry;
    if (code_update_request->path) {
        free(code_update_request->path);
        code_update_request->path = NULL;
    }
    if (code_update_request->project) {
        free(code_update_request->project);
        code_update_request->project = NULL;
    }
    if (code_update_request->embedding_command) {
        free(code_update_request->embedding_command);
        code_update_request->embedding_command = NULL;
    }
    free(code_update_request);
}

cJSON *code_update_request_convertToJSON(code_update_request_t *code_update_request) {
    cJSON *item = cJSON_CreateObject();

    // code_update_request->path
    if (!code_update_request->path) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "path", code_update_request->path) == NULL) {
    goto fail; //String
    }


    // code_update_request->project
    if (!code_update_request->project) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "project", code_update_request->project) == NULL) {
    goto fail; //String
    }


    // code_update_request->embedding_command
    if(code_update_request->embedding_command) {
    if(cJSON_AddStringToObject(item, "embedding_command", code_update_request->embedding_command) == NULL) {
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

code_update_request_t *code_update_request_parseFromJSON(cJSON *code_update_requestJSON){

    code_update_request_t *code_update_request_local_var = NULL;

    char *path_local_str = NULL;

    char *project_local_str = NULL;

    char *embedding_command_local_str = NULL;

    // code_update_request->path
    cJSON *path = cJSON_GetObjectItemCaseSensitive(code_update_requestJSON, "path");
    if (cJSON_IsNull(path)) {
        path = NULL;
    }
    if (!path) {
        goto end;
    }

    
    if(!cJSON_IsString(path))
    {
    goto end; //String
    }

    // code_update_request->project
    cJSON *project = cJSON_GetObjectItemCaseSensitive(code_update_requestJSON, "project");
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

    // code_update_request->embedding_command
    cJSON *embedding_command = cJSON_GetObjectItemCaseSensitive(code_update_requestJSON, "embedding_command");
    if (cJSON_IsNull(embedding_command)) {
        embedding_command = NULL;
    }
    if (embedding_command) { 
    if(!cJSON_IsString(embedding_command) && !cJSON_IsNull(embedding_command))
    {
    goto end; //String
    }
    }


    if (path && !cJSON_IsNull(path)) path_local_str = strdup(path->valuestring);
    if (project && !cJSON_IsNull(project)) project_local_str = strdup(project->valuestring);
    if (embedding_command && !cJSON_IsNull(embedding_command)) embedding_command_local_str = strdup(embedding_command->valuestring);

    code_update_request_local_var = code_update_request_create_internal (
        path_local_str,
        project_local_str,
        embedding_command_local_str
        );

    if (!code_update_request_local_var) {
        goto end;
    }

    return code_update_request_local_var;
end:
    if (path_local_str) {
        free(path_local_str);
        path_local_str = NULL;
    }
    if (project_local_str) {
        free(project_local_str);
        project_local_str = NULL;
    }
    if (embedding_command_local_str) {
        free(embedding_command_local_str);
        embedding_command_local_str = NULL;
    }
    return NULL;

}
