#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "search_request.h"



static search_request_t *search_request_create_internal(
    char *query,
    char *project,
    char *scope_kind,
    char *scope_id,
    int *max_results,
    char *fusion_mode,
    char *cursor
    ) {
    search_request_t *search_request_local_var = malloc(sizeof(search_request_t));
    if (!search_request_local_var) {
        return NULL;
    }
    memset(search_request_local_var, 0, sizeof(search_request_t));
    search_request_local_var->_library_owned = 1;
    search_request_local_var->query = query;
    search_request_local_var->project = project;
    search_request_local_var->scope_kind = scope_kind;
    search_request_local_var->scope_id = scope_id;
    search_request_local_var->max_results = max_results;
    search_request_local_var->fusion_mode = fusion_mode;
    search_request_local_var->cursor = cursor;
    return search_request_local_var;
}

__attribute__((deprecated)) search_request_t *search_request_create(
    char *query,
    char *project,
    char *scope_kind,
    char *scope_id,
    int *max_results,
    char *fusion_mode,
    char *cursor
    ) {
    int *max_results_copy = NULL;
    if (max_results) {
        max_results_copy = malloc(sizeof(int));
        if (max_results_copy) *max_results_copy = *max_results;
    }
    search_request_t *result = search_request_create_internal (
        query,
        project,
        scope_kind,
        scope_id,
        max_results_copy,
        fusion_mode,
        cursor
        );
    if (!result) {
        free(max_results_copy);
    }
    return result;
}

void search_request_free(search_request_t *search_request) {
    if(NULL == search_request){
        return ;
    }
    if(search_request->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "search_request_free");
        return ;
    }
    listEntry_t *listEntry;
    if (search_request->query) {
        free(search_request->query);
        search_request->query = NULL;
    }
    if (search_request->project) {
        free(search_request->project);
        search_request->project = NULL;
    }
    if (search_request->scope_kind) {
        free(search_request->scope_kind);
        search_request->scope_kind = NULL;
    }
    if (search_request->scope_id) {
        free(search_request->scope_id);
        search_request->scope_id = NULL;
    }
    if (search_request->max_results) {
        free(search_request->max_results);
        search_request->max_results = NULL;
    }
    if (search_request->fusion_mode) {
        free(search_request->fusion_mode);
        search_request->fusion_mode = NULL;
    }
    if (search_request->cursor) {
        free(search_request->cursor);
        search_request->cursor = NULL;
    }
    free(search_request);
}

cJSON *search_request_convertToJSON(search_request_t *search_request) {
    cJSON *item = cJSON_CreateObject();

    // search_request->query
    if (!search_request->query) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "query", search_request->query) == NULL) {
    goto fail; //String
    }


    // search_request->project
    if(search_request->project) {
    if(cJSON_AddStringToObject(item, "project", search_request->project) == NULL) {
    goto fail; //String
    }
    }


    // search_request->scope_kind
    if(search_request->scope_kind) {
    if(cJSON_AddStringToObject(item, "scope_kind", search_request->scope_kind) == NULL) {
    goto fail; //String
    }
    }


    // search_request->scope_id
    if(search_request->scope_id) {
    if(cJSON_AddStringToObject(item, "scope_id", search_request->scope_id) == NULL) {
    goto fail; //String
    }
    }


    // search_request->max_results
    if(search_request->max_results) {
    if(cJSON_AddNumberToObject(item, "max_results", *search_request->max_results) == NULL) {
    goto fail; //Numeric
    }
    }


    // search_request->fusion_mode
    if(search_request->fusion_mode) {
    if(cJSON_AddStringToObject(item, "fusion_mode", search_request->fusion_mode) == NULL) {
    goto fail; //String
    }
    }


    // search_request->cursor
    if(search_request->cursor) {
    if(cJSON_AddStringToObject(item, "cursor", search_request->cursor) == NULL) {
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

search_request_t *search_request_parseFromJSON(cJSON *search_requestJSON){

    search_request_t *search_request_local_var = NULL;

    char *query_local_str = NULL;

    char *project_local_str = NULL;

    char *scope_kind_local_str = NULL;

    char *scope_id_local_str = NULL;

    // define the local variable for search_request->max_results
    int *max_results_local_var = NULL;

    char *fusion_mode_local_str = NULL;

    char *cursor_local_str = NULL;

    // search_request->query
    cJSON *query = cJSON_GetObjectItemCaseSensitive(search_requestJSON, "query");
    if (cJSON_IsNull(query)) {
        query = NULL;
    }
    if (!query) {
        goto end;
    }

    
    if(!cJSON_IsString(query))
    {
    goto end; //String
    }

    // search_request->project
    cJSON *project = cJSON_GetObjectItemCaseSensitive(search_requestJSON, "project");
    if (cJSON_IsNull(project)) {
        project = NULL;
    }
    if (project) { 
    if(!cJSON_IsString(project) && !cJSON_IsNull(project))
    {
    goto end; //String
    }
    }

    // search_request->scope_kind
    cJSON *scope_kind = cJSON_GetObjectItemCaseSensitive(search_requestJSON, "scope_kind");
    if (cJSON_IsNull(scope_kind)) {
        scope_kind = NULL;
    }
    if (scope_kind) { 
    if(!cJSON_IsString(scope_kind) && !cJSON_IsNull(scope_kind))
    {
    goto end; //String
    }
    }

    // search_request->scope_id
    cJSON *scope_id = cJSON_GetObjectItemCaseSensitive(search_requestJSON, "scope_id");
    if (cJSON_IsNull(scope_id)) {
        scope_id = NULL;
    }
    if (scope_id) { 
    if(!cJSON_IsString(scope_id) && !cJSON_IsNull(scope_id))
    {
    goto end; //String
    }
    }

    // search_request->max_results
    cJSON *max_results = cJSON_GetObjectItemCaseSensitive(search_requestJSON, "max_results");
    if (cJSON_IsNull(max_results)) {
        max_results = NULL;
    }
    if (max_results) { 
    if(!cJSON_IsNumber(max_results))
    {
    goto end; //Numeric
    }
    max_results_local_var = malloc(sizeof(int));
    if(!max_results_local_var)
    {
        goto end;
    }
    *max_results_local_var = max_results->valuedouble;
    }

    // search_request->fusion_mode
    cJSON *fusion_mode = cJSON_GetObjectItemCaseSensitive(search_requestJSON, "fusion_mode");
    if (cJSON_IsNull(fusion_mode)) {
        fusion_mode = NULL;
    }
    if (fusion_mode) { 
    if(!cJSON_IsString(fusion_mode) && !cJSON_IsNull(fusion_mode))
    {
    goto end; //String
    }
    }

    // search_request->cursor
    cJSON *cursor = cJSON_GetObjectItemCaseSensitive(search_requestJSON, "cursor");
    if (cJSON_IsNull(cursor)) {
        cursor = NULL;
    }
    if (cursor) { 
    if(!cJSON_IsString(cursor) && !cJSON_IsNull(cursor))
    {
    goto end; //String
    }
    }


    if (query && !cJSON_IsNull(query)) query_local_str = strdup(query->valuestring);
    if (project && !cJSON_IsNull(project)) project_local_str = strdup(project->valuestring);
    if (scope_kind && !cJSON_IsNull(scope_kind)) scope_kind_local_str = strdup(scope_kind->valuestring);
    if (scope_id && !cJSON_IsNull(scope_id)) scope_id_local_str = strdup(scope_id->valuestring);
    if (fusion_mode && !cJSON_IsNull(fusion_mode)) fusion_mode_local_str = strdup(fusion_mode->valuestring);
    if (cursor && !cJSON_IsNull(cursor)) cursor_local_str = strdup(cursor->valuestring);

    search_request_local_var = search_request_create_internal (
        query_local_str,
        project_local_str,
        scope_kind_local_str,
        scope_id_local_str,
        max_results_local_var,
        fusion_mode_local_str,
        cursor_local_str
        );

    if (!search_request_local_var) {
        goto end;
    }

    return search_request_local_var;
end:
    if (query_local_str) {
        free(query_local_str);
        query_local_str = NULL;
    }
    if (project_local_str) {
        free(project_local_str);
        project_local_str = NULL;
    }
    if (scope_kind_local_str) {
        free(scope_kind_local_str);
        scope_kind_local_str = NULL;
    }
    if (scope_id_local_str) {
        free(scope_id_local_str);
        scope_id_local_str = NULL;
    }
    if (max_results_local_var) {
        free(max_results_local_var);
        max_results_local_var = NULL;
    }
    if (fusion_mode_local_str) {
        free(fusion_mode_local_str);
        fusion_mode_local_str = NULL;
    }
    if (cursor_local_str) {
        free(cursor_local_str);
        cursor_local_str = NULL;
    }
    return NULL;

}
