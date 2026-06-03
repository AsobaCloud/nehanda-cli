#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "code_caller_hit.h"



static code_caller_hit_t *code_caller_hit_create_internal(
    char *project,
    char *file_path,
    char *caller,
    int *line
    ) {
    code_caller_hit_t *code_caller_hit_local_var = malloc(sizeof(code_caller_hit_t));
    if (!code_caller_hit_local_var) {
        return NULL;
    }
    memset(code_caller_hit_local_var, 0, sizeof(code_caller_hit_t));
    code_caller_hit_local_var->_library_owned = 1;
    code_caller_hit_local_var->project = project;
    code_caller_hit_local_var->file_path = file_path;
    code_caller_hit_local_var->caller = caller;
    code_caller_hit_local_var->line = line;
    return code_caller_hit_local_var;
}

__attribute__((deprecated)) code_caller_hit_t *code_caller_hit_create(
    char *project,
    char *file_path,
    char *caller,
    int *line
    ) {
    int *line_copy = NULL;
    if (line) {
        line_copy = malloc(sizeof(int));
        if (line_copy) *line_copy = *line;
    }
    code_caller_hit_t *result = code_caller_hit_create_internal (
        project,
        file_path,
        caller,
        line_copy
        );
    if (!result) {
        free(line_copy);
    }
    return result;
}

void code_caller_hit_free(code_caller_hit_t *code_caller_hit) {
    if(NULL == code_caller_hit){
        return ;
    }
    if(code_caller_hit->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "code_caller_hit_free");
        return ;
    }
    listEntry_t *listEntry;
    if (code_caller_hit->project) {
        free(code_caller_hit->project);
        code_caller_hit->project = NULL;
    }
    if (code_caller_hit->file_path) {
        free(code_caller_hit->file_path);
        code_caller_hit->file_path = NULL;
    }
    if (code_caller_hit->caller) {
        free(code_caller_hit->caller);
        code_caller_hit->caller = NULL;
    }
    if (code_caller_hit->line) {
        free(code_caller_hit->line);
        code_caller_hit->line = NULL;
    }
    free(code_caller_hit);
}

cJSON *code_caller_hit_convertToJSON(code_caller_hit_t *code_caller_hit) {
    cJSON *item = cJSON_CreateObject();

    // code_caller_hit->project
    if(code_caller_hit->project) {
    if(cJSON_AddStringToObject(item, "project", code_caller_hit->project) == NULL) {
    goto fail; //String
    }
    }


    // code_caller_hit->file_path
    if(code_caller_hit->file_path) {
    if(cJSON_AddStringToObject(item, "file_path", code_caller_hit->file_path) == NULL) {
    goto fail; //String
    }
    }


    // code_caller_hit->caller
    if(code_caller_hit->caller) {
    if(cJSON_AddStringToObject(item, "caller", code_caller_hit->caller) == NULL) {
    goto fail; //String
    }
    }


    // code_caller_hit->line
    if(code_caller_hit->line) {
    if(cJSON_AddNumberToObject(item, "line", *code_caller_hit->line) == NULL) {
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

code_caller_hit_t *code_caller_hit_parseFromJSON(cJSON *code_caller_hitJSON){

    code_caller_hit_t *code_caller_hit_local_var = NULL;

    char *project_local_str = NULL;

    char *file_path_local_str = NULL;

    char *caller_local_str = NULL;

    // define the local variable for code_caller_hit->line
    int *line_local_var = NULL;

    // code_caller_hit->project
    cJSON *project = cJSON_GetObjectItemCaseSensitive(code_caller_hitJSON, "project");
    if (cJSON_IsNull(project)) {
        project = NULL;
    }
    if (project) { 
    if(!cJSON_IsString(project) && !cJSON_IsNull(project))
    {
    goto end; //String
    }
    }

    // code_caller_hit->file_path
    cJSON *file_path = cJSON_GetObjectItemCaseSensitive(code_caller_hitJSON, "file_path");
    if (cJSON_IsNull(file_path)) {
        file_path = NULL;
    }
    if (file_path) { 
    if(!cJSON_IsString(file_path) && !cJSON_IsNull(file_path))
    {
    goto end; //String
    }
    }

    // code_caller_hit->caller
    cJSON *caller = cJSON_GetObjectItemCaseSensitive(code_caller_hitJSON, "caller");
    if (cJSON_IsNull(caller)) {
        caller = NULL;
    }
    if (caller) { 
    if(!cJSON_IsString(caller) && !cJSON_IsNull(caller))
    {
    goto end; //String
    }
    }

    // code_caller_hit->line
    cJSON *line = cJSON_GetObjectItemCaseSensitive(code_caller_hitJSON, "line");
    if (cJSON_IsNull(line)) {
        line = NULL;
    }
    if (line) { 
    if(!cJSON_IsNumber(line))
    {
    goto end; //Numeric
    }
    line_local_var = malloc(sizeof(int));
    if(!line_local_var)
    {
        goto end;
    }
    *line_local_var = line->valuedouble;
    }


    if (project && !cJSON_IsNull(project)) project_local_str = strdup(project->valuestring);
    if (file_path && !cJSON_IsNull(file_path)) file_path_local_str = strdup(file_path->valuestring);
    if (caller && !cJSON_IsNull(caller)) caller_local_str = strdup(caller->valuestring);

    code_caller_hit_local_var = code_caller_hit_create_internal (
        project_local_str,
        file_path_local_str,
        caller_local_str,
        line_local_var
        );

    if (!code_caller_hit_local_var) {
        goto end;
    }

    return code_caller_hit_local_var;
end:
    if (project_local_str) {
        free(project_local_str);
        project_local_str = NULL;
    }
    if (file_path_local_str) {
        free(file_path_local_str);
        file_path_local_str = NULL;
    }
    if (caller_local_str) {
        free(caller_local_str);
        caller_local_str = NULL;
    }
    if (line_local_var) {
        free(line_local_var);
        line_local_var = NULL;
    }
    return NULL;

}
