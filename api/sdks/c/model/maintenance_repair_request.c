#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "maintenance_repair_request.h"



static maintenance_repair_request_t *maintenance_repair_request_create_internal(
    char *path,
    char *project,
    char *embedding_command
    ) {
    maintenance_repair_request_t *maintenance_repair_request_local_var = malloc(sizeof(maintenance_repair_request_t));
    if (!maintenance_repair_request_local_var) {
        return NULL;
    }
    memset(maintenance_repair_request_local_var, 0, sizeof(maintenance_repair_request_t));
    maintenance_repair_request_local_var->_library_owned = 1;
    maintenance_repair_request_local_var->path = path;
    maintenance_repair_request_local_var->project = project;
    maintenance_repair_request_local_var->embedding_command = embedding_command;
    return maintenance_repair_request_local_var;
}

__attribute__((deprecated)) maintenance_repair_request_t *maintenance_repair_request_create(
    char *path,
    char *project,
    char *embedding_command
    ) {
    maintenance_repair_request_t *result = maintenance_repair_request_create_internal (
        path,
        project,
        embedding_command
        );
    if (!result) {
    }
    return result;
}

void maintenance_repair_request_free(maintenance_repair_request_t *maintenance_repair_request) {
    if(NULL == maintenance_repair_request){
        return ;
    }
    if(maintenance_repair_request->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "maintenance_repair_request_free");
        return ;
    }
    listEntry_t *listEntry;
    if (maintenance_repair_request->path) {
        free(maintenance_repair_request->path);
        maintenance_repair_request->path = NULL;
    }
    if (maintenance_repair_request->project) {
        free(maintenance_repair_request->project);
        maintenance_repair_request->project = NULL;
    }
    if (maintenance_repair_request->embedding_command) {
        free(maintenance_repair_request->embedding_command);
        maintenance_repair_request->embedding_command = NULL;
    }
    free(maintenance_repair_request);
}

cJSON *maintenance_repair_request_convertToJSON(maintenance_repair_request_t *maintenance_repair_request) {
    cJSON *item = cJSON_CreateObject();

    // maintenance_repair_request->path
    if (!maintenance_repair_request->path) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "path", maintenance_repair_request->path) == NULL) {
    goto fail; //String
    }


    // maintenance_repair_request->project
    if (!maintenance_repair_request->project) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "project", maintenance_repair_request->project) == NULL) {
    goto fail; //String
    }


    // maintenance_repair_request->embedding_command
    if(maintenance_repair_request->embedding_command) {
    if(cJSON_AddStringToObject(item, "embedding_command", maintenance_repair_request->embedding_command) == NULL) {
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

maintenance_repair_request_t *maintenance_repair_request_parseFromJSON(cJSON *maintenance_repair_requestJSON){

    maintenance_repair_request_t *maintenance_repair_request_local_var = NULL;

    char *path_local_str = NULL;

    char *project_local_str = NULL;

    char *embedding_command_local_str = NULL;

    // maintenance_repair_request->path
    cJSON *path = cJSON_GetObjectItemCaseSensitive(maintenance_repair_requestJSON, "path");
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

    // maintenance_repair_request->project
    cJSON *project = cJSON_GetObjectItemCaseSensitive(maintenance_repair_requestJSON, "project");
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

    // maintenance_repair_request->embedding_command
    cJSON *embedding_command = cJSON_GetObjectItemCaseSensitive(maintenance_repair_requestJSON, "embedding_command");
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

    maintenance_repair_request_local_var = maintenance_repair_request_create_internal (
        path_local_str,
        project_local_str,
        embedding_command_local_str
        );

    if (!maintenance_repair_request_local_var) {
        goto end;
    }

    return maintenance_repair_request_local_var;
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
