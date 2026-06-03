#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "code_build_request.h"



static code_build_request_t *code_build_request_create_internal(
    char *path,
    char *project,
    char *embedding_command,
    int *force
    ) {
    code_build_request_t *code_build_request_local_var = malloc(sizeof(code_build_request_t));
    if (!code_build_request_local_var) {
        return NULL;
    }
    memset(code_build_request_local_var, 0, sizeof(code_build_request_t));
    code_build_request_local_var->_library_owned = 1;
    code_build_request_local_var->path = path;
    code_build_request_local_var->project = project;
    code_build_request_local_var->embedding_command = embedding_command;
    code_build_request_local_var->force = force;
    return code_build_request_local_var;
}

__attribute__((deprecated)) code_build_request_t *code_build_request_create(
    char *path,
    char *project,
    char *embedding_command,
    int *force
    ) {
    int *force_copy = NULL;
    if (force) {
        force_copy = malloc(sizeof(int));
        if (force_copy) *force_copy = *force;
    }
    code_build_request_t *result = code_build_request_create_internal (
        path,
        project,
        embedding_command,
        force_copy
        );
    if (!result) {
        free(force_copy);
    }
    return result;
}

void code_build_request_free(code_build_request_t *code_build_request) {
    if(NULL == code_build_request){
        return ;
    }
    if(code_build_request->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "code_build_request_free");
        return ;
    }
    listEntry_t *listEntry;
    if (code_build_request->path) {
        free(code_build_request->path);
        code_build_request->path = NULL;
    }
    if (code_build_request->project) {
        free(code_build_request->project);
        code_build_request->project = NULL;
    }
    if (code_build_request->embedding_command) {
        free(code_build_request->embedding_command);
        code_build_request->embedding_command = NULL;
    }
    if (code_build_request->force) {
        free(code_build_request->force);
        code_build_request->force = NULL;
    }
    free(code_build_request);
}

cJSON *code_build_request_convertToJSON(code_build_request_t *code_build_request) {
    cJSON *item = cJSON_CreateObject();

    // code_build_request->path
    if (!code_build_request->path) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "path", code_build_request->path) == NULL) {
    goto fail; //String
    }


    // code_build_request->project
    if (!code_build_request->project) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "project", code_build_request->project) == NULL) {
    goto fail; //String
    }


    // code_build_request->embedding_command
    if(code_build_request->embedding_command) {
    if(cJSON_AddStringToObject(item, "embedding_command", code_build_request->embedding_command) == NULL) {
    goto fail; //String
    }
    }


    // code_build_request->force
    if(code_build_request->force) {
    if(cJSON_AddBoolToObject(item, "force", *code_build_request->force) == NULL) {
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

code_build_request_t *code_build_request_parseFromJSON(cJSON *code_build_requestJSON){

    code_build_request_t *code_build_request_local_var = NULL;

    char *path_local_str = NULL;

    char *project_local_str = NULL;

    char *embedding_command_local_str = NULL;

    // define the local variable for code_build_request->force
    int *force_local_var = NULL;

    // code_build_request->path
    cJSON *path = cJSON_GetObjectItemCaseSensitive(code_build_requestJSON, "path");
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

    // code_build_request->project
    cJSON *project = cJSON_GetObjectItemCaseSensitive(code_build_requestJSON, "project");
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

    // code_build_request->embedding_command
    cJSON *embedding_command = cJSON_GetObjectItemCaseSensitive(code_build_requestJSON, "embedding_command");
    if (cJSON_IsNull(embedding_command)) {
        embedding_command = NULL;
    }
    if (embedding_command) { 
    if(!cJSON_IsString(embedding_command) && !cJSON_IsNull(embedding_command))
    {
    goto end; //String
    }
    }

    // code_build_request->force
    cJSON *force = cJSON_GetObjectItemCaseSensitive(code_build_requestJSON, "force");
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


    if (path && !cJSON_IsNull(path)) path_local_str = strdup(path->valuestring);
    if (project && !cJSON_IsNull(project)) project_local_str = strdup(project->valuestring);
    if (embedding_command && !cJSON_IsNull(embedding_command)) embedding_command_local_str = strdup(embedding_command->valuestring);

    code_build_request_local_var = code_build_request_create_internal (
        path_local_str,
        project_local_str,
        embedding_command_local_str,
        force_local_var
        );

    if (!code_build_request_local_var) {
        goto end;
    }

    return code_build_request_local_var;
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
    if (force_local_var) {
        free(force_local_var);
        force_local_var = NULL;
    }
    return NULL;

}
