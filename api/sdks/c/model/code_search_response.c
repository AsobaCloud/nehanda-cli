#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "code_search_response.h"



static code_search_response_t *code_search_response_create_internal(
    char *status,
    list_t *hits,
    char *next_cursor
    ) {
    code_search_response_t *code_search_response_local_var = malloc(sizeof(code_search_response_t));
    if (!code_search_response_local_var) {
        return NULL;
    }
    memset(code_search_response_local_var, 0, sizeof(code_search_response_t));
    code_search_response_local_var->_library_owned = 1;
    code_search_response_local_var->status = status;
    code_search_response_local_var->hits = hits;
    code_search_response_local_var->next_cursor = next_cursor;
    return code_search_response_local_var;
}

__attribute__((deprecated)) code_search_response_t *code_search_response_create(
    char *status,
    list_t *hits,
    char *next_cursor
    ) {
    code_search_response_t *result = code_search_response_create_internal (
        status,
        hits,
        next_cursor
        );
    if (!result) {
    }
    return result;
}

void code_search_response_free(code_search_response_t *code_search_response) {
    if(NULL == code_search_response){
        return ;
    }
    if(code_search_response->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "code_search_response_free");
        return ;
    }
    listEntry_t *listEntry;
    if (code_search_response->status) {
        free(code_search_response->status);
        code_search_response->status = NULL;
    }
    if (code_search_response->hits) {
        list_ForEach(listEntry, code_search_response->hits) {
            code_search_hit_free(listEntry->data);
        }
        list_freeList(code_search_response->hits);
        code_search_response->hits = NULL;
    }
    if (code_search_response->next_cursor) {
        free(code_search_response->next_cursor);
        code_search_response->next_cursor = NULL;
    }
    free(code_search_response);
}

cJSON *code_search_response_convertToJSON(code_search_response_t *code_search_response) {
    cJSON *item = cJSON_CreateObject();

    // code_search_response->status
    if(code_search_response->status) {
    if(cJSON_AddStringToObject(item, "status", code_search_response->status) == NULL) {
    goto fail; //String
    }
    }


    // code_search_response->hits
    if(code_search_response->hits) {
    cJSON *hits = cJSON_AddArrayToObject(item, "hits");
    if(hits == NULL) {
    goto fail; //nonprimitive container
    }

    listEntry_t *hitsListEntry;
    if (code_search_response->hits) {
    list_ForEach(hitsListEntry, code_search_response->hits) {
    cJSON *itemLocal = code_search_hit_convertToJSON(hitsListEntry->data);
    if(itemLocal == NULL) {
    goto fail;
    }
    cJSON_AddItemToArray(hits, itemLocal);
    }
    }
    }


    // code_search_response->next_cursor
    if(code_search_response->next_cursor) {
    if(cJSON_AddStringToObject(item, "next_cursor", code_search_response->next_cursor) == NULL) {
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

code_search_response_t *code_search_response_parseFromJSON(cJSON *code_search_responseJSON){

    code_search_response_t *code_search_response_local_var = NULL;

    char *status_local_str = NULL;

    // define the local list for code_search_response->hits
    list_t *hitsList = NULL;

    char *next_cursor_local_str = NULL;

    // code_search_response->status
    cJSON *status = cJSON_GetObjectItemCaseSensitive(code_search_responseJSON, "status");
    if (cJSON_IsNull(status)) {
        status = NULL;
    }
    if (status) { 
    if(!cJSON_IsString(status) && !cJSON_IsNull(status))
    {
    goto end; //String
    }
    }

    // code_search_response->hits
    cJSON *hits = cJSON_GetObjectItemCaseSensitive(code_search_responseJSON, "hits");
    if (cJSON_IsNull(hits)) {
        hits = NULL;
    }
    if (hits) { 
    cJSON *hits_local_nonprimitive = NULL;
    if(!cJSON_IsArray(hits)){
        goto end; //nonprimitive container
    }

    hitsList = list_createList();

    cJSON_ArrayForEach(hits_local_nonprimitive,hits )
    {
        if(!cJSON_IsObject(hits_local_nonprimitive)){
            goto end;
        }
        code_search_hit_t *hitsItem = code_search_hit_parseFromJSON(hits_local_nonprimitive);

        list_addElement(hitsList, hitsItem);
    }
    }

    // code_search_response->next_cursor
    cJSON *next_cursor = cJSON_GetObjectItemCaseSensitive(code_search_responseJSON, "next_cursor");
    if (cJSON_IsNull(next_cursor)) {
        next_cursor = NULL;
    }
    if (next_cursor) { 
    if(!cJSON_IsString(next_cursor) && !cJSON_IsNull(next_cursor))
    {
    goto end; //String
    }
    }


    if (status && !cJSON_IsNull(status)) status_local_str = strdup(status->valuestring);
    if (next_cursor && !cJSON_IsNull(next_cursor)) next_cursor_local_str = strdup(next_cursor->valuestring);

    code_search_response_local_var = code_search_response_create_internal (
        status_local_str,
        hits ? hitsList : NULL,
        next_cursor_local_str
        );

    if (!code_search_response_local_var) {
        goto end;
    }

    return code_search_response_local_var;
end:
    if (status_local_str) {
        free(status_local_str);
        status_local_str = NULL;
    }
    if (hitsList) {
        listEntry_t *listEntry = NULL;
        list_ForEach(listEntry, hitsList) {
            code_search_hit_free(listEntry->data);
            listEntry->data = NULL;
        }
        list_freeList(hitsList);
        hitsList = NULL;
    }
    if (next_cursor_local_str) {
        free(next_cursor_local_str);
        next_cursor_local_str = NULL;
    }
    return NULL;

}
