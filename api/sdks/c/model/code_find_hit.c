#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "code_find_hit.h"



static code_find_hit_t *code_find_hit_create_internal(
    char *project,
    char *file_path,
    int *line,
    char *kind
    ) {
    code_find_hit_t *code_find_hit_local_var = malloc(sizeof(code_find_hit_t));
    if (!code_find_hit_local_var) {
        return NULL;
    }
    memset(code_find_hit_local_var, 0, sizeof(code_find_hit_t));
    code_find_hit_local_var->_library_owned = 1;
    code_find_hit_local_var->project = project;
    code_find_hit_local_var->file_path = file_path;
    code_find_hit_local_var->line = line;
    code_find_hit_local_var->kind = kind;
    return code_find_hit_local_var;
}

__attribute__((deprecated)) code_find_hit_t *code_find_hit_create(
    char *project,
    char *file_path,
    int *line,
    char *kind
    ) {
    int *line_copy = NULL;
    if (line) {
        line_copy = malloc(sizeof(int));
        if (line_copy) *line_copy = *line;
    }
    code_find_hit_t *result = code_find_hit_create_internal (
        project,
        file_path,
        line_copy,
        kind
        );
    if (!result) {
        free(line_copy);
    }
    return result;
}

void code_find_hit_free(code_find_hit_t *code_find_hit) {
    if(NULL == code_find_hit){
        return ;
    }
    if(code_find_hit->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "code_find_hit_free");
        return ;
    }
    listEntry_t *listEntry;
    if (code_find_hit->project) {
        free(code_find_hit->project);
        code_find_hit->project = NULL;
    }
    if (code_find_hit->file_path) {
        free(code_find_hit->file_path);
        code_find_hit->file_path = NULL;
    }
    if (code_find_hit->line) {
        free(code_find_hit->line);
        code_find_hit->line = NULL;
    }
    if (code_find_hit->kind) {
        free(code_find_hit->kind);
        code_find_hit->kind = NULL;
    }
    free(code_find_hit);
}

cJSON *code_find_hit_convertToJSON(code_find_hit_t *code_find_hit) {
    cJSON *item = cJSON_CreateObject();

    // code_find_hit->project
    if(code_find_hit->project) {
    if(cJSON_AddStringToObject(item, "project", code_find_hit->project) == NULL) {
    goto fail; //String
    }
    }


    // code_find_hit->file_path
    if(code_find_hit->file_path) {
    if(cJSON_AddStringToObject(item, "file_path", code_find_hit->file_path) == NULL) {
    goto fail; //String
    }
    }


    // code_find_hit->line
    if(code_find_hit->line) {
    if(cJSON_AddNumberToObject(item, "line", *code_find_hit->line) == NULL) {
    goto fail; //Numeric
    }
    }


    // code_find_hit->kind
    if(code_find_hit->kind) {
    if(cJSON_AddStringToObject(item, "kind", code_find_hit->kind) == NULL) {
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

code_find_hit_t *code_find_hit_parseFromJSON(cJSON *code_find_hitJSON){

    code_find_hit_t *code_find_hit_local_var = NULL;

    char *project_local_str = NULL;

    char *file_path_local_str = NULL;

    // define the local variable for code_find_hit->line
    int *line_local_var = NULL;

    char *kind_local_str = NULL;

    // code_find_hit->project
    cJSON *project = cJSON_GetObjectItemCaseSensitive(code_find_hitJSON, "project");
    if (cJSON_IsNull(project)) {
        project = NULL;
    }
    if (project) { 
    if(!cJSON_IsString(project) && !cJSON_IsNull(project))
    {
    goto end; //String
    }
    }

    // code_find_hit->file_path
    cJSON *file_path = cJSON_GetObjectItemCaseSensitive(code_find_hitJSON, "file_path");
    if (cJSON_IsNull(file_path)) {
        file_path = NULL;
    }
    if (file_path) { 
    if(!cJSON_IsString(file_path) && !cJSON_IsNull(file_path))
    {
    goto end; //String
    }
    }

    // code_find_hit->line
    cJSON *line = cJSON_GetObjectItemCaseSensitive(code_find_hitJSON, "line");
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

    // code_find_hit->kind
    cJSON *kind = cJSON_GetObjectItemCaseSensitive(code_find_hitJSON, "kind");
    if (cJSON_IsNull(kind)) {
        kind = NULL;
    }
    if (kind) { 
    if(!cJSON_IsString(kind) && !cJSON_IsNull(kind))
    {
    goto end; //String
    }
    }


    if (project && !cJSON_IsNull(project)) project_local_str = strdup(project->valuestring);
    if (file_path && !cJSON_IsNull(file_path)) file_path_local_str = strdup(file_path->valuestring);
    if (kind && !cJSON_IsNull(kind)) kind_local_str = strdup(kind->valuestring);

    code_find_hit_local_var = code_find_hit_create_internal (
        project_local_str,
        file_path_local_str,
        line_local_var,
        kind_local_str
        );

    if (!code_find_hit_local_var) {
        goto end;
    }

    return code_find_hit_local_var;
end:
    if (project_local_str) {
        free(project_local_str);
        project_local_str = NULL;
    }
    if (file_path_local_str) {
        free(file_path_local_str);
        file_path_local_str = NULL;
    }
    if (line_local_var) {
        free(line_local_var);
        line_local_var = NULL;
    }
    if (kind_local_str) {
        free(kind_local_str);
        kind_local_str = NULL;
    }
    return NULL;

}
