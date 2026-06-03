#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "code_search_hit.h"



static code_search_hit_t *code_search_hit_create_internal(
    char *project,
    char *file_path,
    char *snippet,
    double *rank
    ) {
    code_search_hit_t *code_search_hit_local_var = malloc(sizeof(code_search_hit_t));
    if (!code_search_hit_local_var) {
        return NULL;
    }
    memset(code_search_hit_local_var, 0, sizeof(code_search_hit_t));
    code_search_hit_local_var->_library_owned = 1;
    code_search_hit_local_var->project = project;
    code_search_hit_local_var->file_path = file_path;
    code_search_hit_local_var->snippet = snippet;
    code_search_hit_local_var->rank = rank;
    return code_search_hit_local_var;
}

__attribute__((deprecated)) code_search_hit_t *code_search_hit_create(
    char *project,
    char *file_path,
    char *snippet,
    double *rank
    ) {
    double *rank_copy = NULL;
    if (rank) {
        rank_copy = malloc(sizeof(double));
        if (rank_copy) *rank_copy = *rank;
    }
    code_search_hit_t *result = code_search_hit_create_internal (
        project,
        file_path,
        snippet,
        rank_copy
        );
    if (!result) {
        free(rank_copy);
    }
    return result;
}

void code_search_hit_free(code_search_hit_t *code_search_hit) {
    if(NULL == code_search_hit){
        return ;
    }
    if(code_search_hit->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "code_search_hit_free");
        return ;
    }
    listEntry_t *listEntry;
    if (code_search_hit->project) {
        free(code_search_hit->project);
        code_search_hit->project = NULL;
    }
    if (code_search_hit->file_path) {
        free(code_search_hit->file_path);
        code_search_hit->file_path = NULL;
    }
    if (code_search_hit->snippet) {
        free(code_search_hit->snippet);
        code_search_hit->snippet = NULL;
    }
    if (code_search_hit->rank) {
        free(code_search_hit->rank);
        code_search_hit->rank = NULL;
    }
    free(code_search_hit);
}

cJSON *code_search_hit_convertToJSON(code_search_hit_t *code_search_hit) {
    cJSON *item = cJSON_CreateObject();

    // code_search_hit->project
    if(code_search_hit->project) {
    if(cJSON_AddStringToObject(item, "project", code_search_hit->project) == NULL) {
    goto fail; //String
    }
    }


    // code_search_hit->file_path
    if(code_search_hit->file_path) {
    if(cJSON_AddStringToObject(item, "file_path", code_search_hit->file_path) == NULL) {
    goto fail; //String
    }
    }


    // code_search_hit->snippet
    if(code_search_hit->snippet) {
    if(cJSON_AddStringToObject(item, "snippet", code_search_hit->snippet) == NULL) {
    goto fail; //String
    }
    }


    // code_search_hit->rank
    if(code_search_hit->rank) {
    if(cJSON_AddNumberToObject(item, "rank", *code_search_hit->rank) == NULL) {
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

code_search_hit_t *code_search_hit_parseFromJSON(cJSON *code_search_hitJSON){

    code_search_hit_t *code_search_hit_local_var = NULL;

    char *project_local_str = NULL;

    char *file_path_local_str = NULL;

    char *snippet_local_str = NULL;

    // define the local variable for code_search_hit->rank
    double *rank_local_var = NULL;

    // code_search_hit->project
    cJSON *project = cJSON_GetObjectItemCaseSensitive(code_search_hitJSON, "project");
    if (cJSON_IsNull(project)) {
        project = NULL;
    }
    if (project) { 
    if(!cJSON_IsString(project) && !cJSON_IsNull(project))
    {
    goto end; //String
    }
    }

    // code_search_hit->file_path
    cJSON *file_path = cJSON_GetObjectItemCaseSensitive(code_search_hitJSON, "file_path");
    if (cJSON_IsNull(file_path)) {
        file_path = NULL;
    }
    if (file_path) { 
    if(!cJSON_IsString(file_path) && !cJSON_IsNull(file_path))
    {
    goto end; //String
    }
    }

    // code_search_hit->snippet
    cJSON *snippet = cJSON_GetObjectItemCaseSensitive(code_search_hitJSON, "snippet");
    if (cJSON_IsNull(snippet)) {
        snippet = NULL;
    }
    if (snippet) { 
    if(!cJSON_IsString(snippet) && !cJSON_IsNull(snippet))
    {
    goto end; //String
    }
    }

    // code_search_hit->rank
    cJSON *rank = cJSON_GetObjectItemCaseSensitive(code_search_hitJSON, "rank");
    if (cJSON_IsNull(rank)) {
        rank = NULL;
    }
    if (rank) { 
    if(!cJSON_IsNumber(rank))
    {
    goto end; //Numeric
    }
    rank_local_var = malloc(sizeof(double));
    if(!rank_local_var)
    {
        goto end;
    }
    *rank_local_var = rank->valuedouble;
    }


    if (project && !cJSON_IsNull(project)) project_local_str = strdup(project->valuestring);
    if (file_path && !cJSON_IsNull(file_path)) file_path_local_str = strdup(file_path->valuestring);
    if (snippet && !cJSON_IsNull(snippet)) snippet_local_str = strdup(snippet->valuestring);

    code_search_hit_local_var = code_search_hit_create_internal (
        project_local_str,
        file_path_local_str,
        snippet_local_str,
        rank_local_var
        );

    if (!code_search_hit_local_var) {
        goto end;
    }

    return code_search_hit_local_var;
end:
    if (project_local_str) {
        free(project_local_str);
        project_local_str = NULL;
    }
    if (file_path_local_str) {
        free(file_path_local_str);
        file_path_local_str = NULL;
    }
    if (snippet_local_str) {
        free(snippet_local_str);
        snippet_local_str = NULL;
    }
    if (rank_local_var) {
        free(rank_local_var);
        rank_local_var = NULL;
    }
    return NULL;

}
