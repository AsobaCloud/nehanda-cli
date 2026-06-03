#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "code_scan_request.h"



static code_scan_request_t *code_scan_request_create_internal(
    char *project,
    char *root_path,
    int *force
    ) {
    code_scan_request_t *code_scan_request_local_var = malloc(sizeof(code_scan_request_t));
    if (!code_scan_request_local_var) {
        return NULL;
    }
    memset(code_scan_request_local_var, 0, sizeof(code_scan_request_t));
    code_scan_request_local_var->_library_owned = 1;
    code_scan_request_local_var->project = project;
    code_scan_request_local_var->root_path = root_path;
    code_scan_request_local_var->force = force;
    return code_scan_request_local_var;
}

__attribute__((deprecated)) code_scan_request_t *code_scan_request_create(
    char *project,
    char *root_path,
    int *force
    ) {
    int *force_copy = NULL;
    if (force) {
        force_copy = malloc(sizeof(int));
        if (force_copy) *force_copy = *force;
    }
    code_scan_request_t *result = code_scan_request_create_internal (
        project,
        root_path,
        force_copy
        );
    if (!result) {
        free(force_copy);
    }
    return result;
}

void code_scan_request_free(code_scan_request_t *code_scan_request) {
    if(NULL == code_scan_request){
        return ;
    }
    if(code_scan_request->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "code_scan_request_free");
        return ;
    }
    listEntry_t *listEntry;
    if (code_scan_request->project) {
        free(code_scan_request->project);
        code_scan_request->project = NULL;
    }
    if (code_scan_request->root_path) {
        free(code_scan_request->root_path);
        code_scan_request->root_path = NULL;
    }
    if (code_scan_request->force) {
        free(code_scan_request->force);
        code_scan_request->force = NULL;
    }
    free(code_scan_request);
}

cJSON *code_scan_request_convertToJSON(code_scan_request_t *code_scan_request) {
    cJSON *item = cJSON_CreateObject();

    // code_scan_request->project
    if (!code_scan_request->project) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "project", code_scan_request->project) == NULL) {
    goto fail; //String
    }


    // code_scan_request->root_path
    if (!code_scan_request->root_path) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "root_path", code_scan_request->root_path) == NULL) {
    goto fail; //String
    }


    // code_scan_request->force
    if(code_scan_request->force) {
    if(cJSON_AddBoolToObject(item, "force", *code_scan_request->force) == NULL) {
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

code_scan_request_t *code_scan_request_parseFromJSON(cJSON *code_scan_requestJSON){

    code_scan_request_t *code_scan_request_local_var = NULL;

    char *project_local_str = NULL;

    char *root_path_local_str = NULL;

    // define the local variable for code_scan_request->force
    int *force_local_var = NULL;

    // code_scan_request->project
    cJSON *project = cJSON_GetObjectItemCaseSensitive(code_scan_requestJSON, "project");
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

    // code_scan_request->root_path
    cJSON *root_path = cJSON_GetObjectItemCaseSensitive(code_scan_requestJSON, "root_path");
    if (cJSON_IsNull(root_path)) {
        root_path = NULL;
    }
    if (!root_path) {
        goto end;
    }

    
    if(!cJSON_IsString(root_path))
    {
    goto end; //String
    }

    // code_scan_request->force
    cJSON *force = cJSON_GetObjectItemCaseSensitive(code_scan_requestJSON, "force");
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


    if (project && !cJSON_IsNull(project)) project_local_str = strdup(project->valuestring);
    if (root_path && !cJSON_IsNull(root_path)) root_path_local_str = strdup(root_path->valuestring);

    code_scan_request_local_var = code_scan_request_create_internal (
        project_local_str,
        root_path_local_str,
        force_local_var
        );

    if (!code_scan_request_local_var) {
        goto end;
    }

    return code_scan_request_local_var;
end:
    if (project_local_str) {
        free(project_local_str);
        project_local_str = NULL;
    }
    if (root_path_local_str) {
        free(root_path_local_str);
        root_path_local_str = NULL;
    }
    if (force_local_var) {
        free(force_local_var);
        force_local_var = NULL;
    }
    return NULL;

}
