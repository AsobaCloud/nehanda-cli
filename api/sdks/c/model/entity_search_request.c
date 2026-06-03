#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "entity_search_request.h"



static entity_search_request_t *entity_search_request_create_internal(
    char *query,
    int *limit,
    char *cursor
    ) {
    entity_search_request_t *entity_search_request_local_var = malloc(sizeof(entity_search_request_t));
    if (!entity_search_request_local_var) {
        return NULL;
    }
    memset(entity_search_request_local_var, 0, sizeof(entity_search_request_t));
    entity_search_request_local_var->_library_owned = 1;
    entity_search_request_local_var->query = query;
    entity_search_request_local_var->limit = limit;
    entity_search_request_local_var->cursor = cursor;
    return entity_search_request_local_var;
}

__attribute__((deprecated)) entity_search_request_t *entity_search_request_create(
    char *query,
    int *limit,
    char *cursor
    ) {
    int *limit_copy = NULL;
    if (limit) {
        limit_copy = malloc(sizeof(int));
        if (limit_copy) *limit_copy = *limit;
    }
    entity_search_request_t *result = entity_search_request_create_internal (
        query,
        limit_copy,
        cursor
        );
    if (!result) {
        free(limit_copy);
    }
    return result;
}

void entity_search_request_free(entity_search_request_t *entity_search_request) {
    if(NULL == entity_search_request){
        return ;
    }
    if(entity_search_request->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "entity_search_request_free");
        return ;
    }
    listEntry_t *listEntry;
    if (entity_search_request->query) {
        free(entity_search_request->query);
        entity_search_request->query = NULL;
    }
    if (entity_search_request->limit) {
        free(entity_search_request->limit);
        entity_search_request->limit = NULL;
    }
    if (entity_search_request->cursor) {
        free(entity_search_request->cursor);
        entity_search_request->cursor = NULL;
    }
    free(entity_search_request);
}

cJSON *entity_search_request_convertToJSON(entity_search_request_t *entity_search_request) {
    cJSON *item = cJSON_CreateObject();

    // entity_search_request->query
    if (!entity_search_request->query) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "query", entity_search_request->query) == NULL) {
    goto fail; //String
    }


    // entity_search_request->limit
    if(entity_search_request->limit) {
    if(cJSON_AddNumberToObject(item, "limit", *entity_search_request->limit) == NULL) {
    goto fail; //Numeric
    }
    }


    // entity_search_request->cursor
    if(entity_search_request->cursor) {
    if(cJSON_AddStringToObject(item, "cursor", entity_search_request->cursor) == NULL) {
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

entity_search_request_t *entity_search_request_parseFromJSON(cJSON *entity_search_requestJSON){

    entity_search_request_t *entity_search_request_local_var = NULL;

    char *query_local_str = NULL;

    // define the local variable for entity_search_request->limit
    int *limit_local_var = NULL;

    char *cursor_local_str = NULL;

    // entity_search_request->query
    cJSON *query = cJSON_GetObjectItemCaseSensitive(entity_search_requestJSON, "query");
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

    // entity_search_request->limit
    cJSON *limit = cJSON_GetObjectItemCaseSensitive(entity_search_requestJSON, "limit");
    if (cJSON_IsNull(limit)) {
        limit = NULL;
    }
    if (limit) { 
    if(!cJSON_IsNumber(limit))
    {
    goto end; //Numeric
    }
    limit_local_var = malloc(sizeof(int));
    if(!limit_local_var)
    {
        goto end;
    }
    *limit_local_var = limit->valuedouble;
    }

    // entity_search_request->cursor
    cJSON *cursor = cJSON_GetObjectItemCaseSensitive(entity_search_requestJSON, "cursor");
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
    if (cursor && !cJSON_IsNull(cursor)) cursor_local_str = strdup(cursor->valuestring);

    entity_search_request_local_var = entity_search_request_create_internal (
        query_local_str,
        limit_local_var,
        cursor_local_str
        );

    if (!entity_search_request_local_var) {
        goto end;
    }

    return entity_search_request_local_var;
end:
    if (query_local_str) {
        free(query_local_str);
        query_local_str = NULL;
    }
    if (limit_local_var) {
        free(limit_local_var);
        limit_local_var = NULL;
    }
    if (cursor_local_str) {
        free(cursor_local_str);
        cursor_local_str = NULL;
    }
    return NULL;

}
