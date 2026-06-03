#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "code_project.h"



static code_project_t *code_project_create_internal(
    char *name,
    char *root,
    char *scanned_at
    ) {
    code_project_t *code_project_local_var = malloc(sizeof(code_project_t));
    if (!code_project_local_var) {
        return NULL;
    }
    memset(code_project_local_var, 0, sizeof(code_project_t));
    code_project_local_var->_library_owned = 1;
    code_project_local_var->name = name;
    code_project_local_var->root = root;
    code_project_local_var->scanned_at = scanned_at;
    return code_project_local_var;
}

__attribute__((deprecated)) code_project_t *code_project_create(
    char *name,
    char *root,
    char *scanned_at
    ) {
    code_project_t *result = code_project_create_internal (
        name,
        root,
        scanned_at
        );
    if (!result) {
    }
    return result;
}

void code_project_free(code_project_t *code_project) {
    if(NULL == code_project){
        return ;
    }
    if(code_project->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "code_project_free");
        return ;
    }
    listEntry_t *listEntry;
    if (code_project->name) {
        free(code_project->name);
        code_project->name = NULL;
    }
    if (code_project->root) {
        free(code_project->root);
        code_project->root = NULL;
    }
    if (code_project->scanned_at) {
        free(code_project->scanned_at);
        code_project->scanned_at = NULL;
    }
    free(code_project);
}

cJSON *code_project_convertToJSON(code_project_t *code_project) {
    cJSON *item = cJSON_CreateObject();

    // code_project->name
    if(code_project->name) {
    if(cJSON_AddStringToObject(item, "name", code_project->name) == NULL) {
    goto fail; //String
    }
    }


    // code_project->root
    if(code_project->root) {
    if(cJSON_AddStringToObject(item, "root", code_project->root) == NULL) {
    goto fail; //String
    }
    }


    // code_project->scanned_at
    if(code_project->scanned_at) {
    if(cJSON_AddStringToObject(item, "scanned_at", code_project->scanned_at) == NULL) {
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

code_project_t *code_project_parseFromJSON(cJSON *code_projectJSON){

    code_project_t *code_project_local_var = NULL;

    char *name_local_str = NULL;

    char *root_local_str = NULL;

    char *scanned_at_local_str = NULL;

    // code_project->name
    cJSON *name = cJSON_GetObjectItemCaseSensitive(code_projectJSON, "name");
    if (cJSON_IsNull(name)) {
        name = NULL;
    }
    if (name) { 
    if(!cJSON_IsString(name) && !cJSON_IsNull(name))
    {
    goto end; //String
    }
    }

    // code_project->root
    cJSON *root = cJSON_GetObjectItemCaseSensitive(code_projectJSON, "root");
    if (cJSON_IsNull(root)) {
        root = NULL;
    }
    if (root) { 
    if(!cJSON_IsString(root) && !cJSON_IsNull(root))
    {
    goto end; //String
    }
    }

    // code_project->scanned_at
    cJSON *scanned_at = cJSON_GetObjectItemCaseSensitive(code_projectJSON, "scanned_at");
    if (cJSON_IsNull(scanned_at)) {
        scanned_at = NULL;
    }
    if (scanned_at) { 
    if(!cJSON_IsString(scanned_at) && !cJSON_IsNull(scanned_at))
    {
    goto end; //String
    }
    }


    if (name && !cJSON_IsNull(name)) name_local_str = strdup(name->valuestring);
    if (root && !cJSON_IsNull(root)) root_local_str = strdup(root->valuestring);
    if (scanned_at && !cJSON_IsNull(scanned_at)) scanned_at_local_str = strdup(scanned_at->valuestring);

    code_project_local_var = code_project_create_internal (
        name_local_str,
        root_local_str,
        scanned_at_local_str
        );

    if (!code_project_local_var) {
        goto end;
    }

    return code_project_local_var;
end:
    if (name_local_str) {
        free(name_local_str);
        name_local_str = NULL;
    }
    if (root_local_str) {
        free(root_local_str);
        root_local_str = NULL;
    }
    if (scanned_at_local_str) {
        free(scanned_at_local_str);
        scanned_at_local_str = NULL;
    }
    return NULL;

}
