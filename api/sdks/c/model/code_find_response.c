#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "code_find_response.h"



static code_find_response_t *code_find_response_create_internal(
    list_t *hits,
    char *next_cursor
    ) {
    code_find_response_t *code_find_response_local_var = malloc(sizeof(code_find_response_t));
    if (!code_find_response_local_var) {
        return NULL;
    }
    memset(code_find_response_local_var, 0, sizeof(code_find_response_t));
    code_find_response_local_var->_library_owned = 1;
    code_find_response_local_var->hits = hits;
    code_find_response_local_var->next_cursor = next_cursor;
    return code_find_response_local_var;
}

__attribute__((deprecated)) code_find_response_t *code_find_response_create(
    list_t *hits,
    char *next_cursor
    ) {
    code_find_response_t *result = code_find_response_create_internal (
        hits,
        next_cursor
        );
    if (!result) {
    }
    return result;
}

void code_find_response_free(code_find_response_t *code_find_response) {
    if(NULL == code_find_response){
        return ;
    }
    if(code_find_response->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "code_find_response_free");
        return ;
    }
    listEntry_t *listEntry;
    if (code_find_response->hits) {
        list_ForEach(listEntry, code_find_response->hits) {
            code_find_hit_free(listEntry->data);
        }
        list_freeList(code_find_response->hits);
        code_find_response->hits = NULL;
    }
    if (code_find_response->next_cursor) {
        free(code_find_response->next_cursor);
        code_find_response->next_cursor = NULL;
    }
    free(code_find_response);
}

cJSON *code_find_response_convertToJSON(code_find_response_t *code_find_response) {
    cJSON *item = cJSON_CreateObject();

    // code_find_response->hits
    if(code_find_response->hits) {
    cJSON *hits = cJSON_AddArrayToObject(item, "hits");
    if(hits == NULL) {
    goto fail; //nonprimitive container
    }

    listEntry_t *hitsListEntry;
    if (code_find_response->hits) {
    list_ForEach(hitsListEntry, code_find_response->hits) {
    cJSON *itemLocal = code_find_hit_convertToJSON(hitsListEntry->data);
    if(itemLocal == NULL) {
    goto fail;
    }
    cJSON_AddItemToArray(hits, itemLocal);
    }
    }
    }


    // code_find_response->next_cursor
    if(code_find_response->next_cursor) {
    if(cJSON_AddStringToObject(item, "next_cursor", code_find_response->next_cursor) == NULL) {
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

code_find_response_t *code_find_response_parseFromJSON(cJSON *code_find_responseJSON){

    code_find_response_t *code_find_response_local_var = NULL;

    // define the local list for code_find_response->hits
    list_t *hitsList = NULL;

    char *next_cursor_local_str = NULL;

    // code_find_response->hits
    cJSON *hits = cJSON_GetObjectItemCaseSensitive(code_find_responseJSON, "hits");
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
        code_find_hit_t *hitsItem = code_find_hit_parseFromJSON(hits_local_nonprimitive);

        list_addElement(hitsList, hitsItem);
    }
    }

    // code_find_response->next_cursor
    cJSON *next_cursor = cJSON_GetObjectItemCaseSensitive(code_find_responseJSON, "next_cursor");
    if (cJSON_IsNull(next_cursor)) {
        next_cursor = NULL;
    }
    if (next_cursor) { 
    if(!cJSON_IsString(next_cursor) && !cJSON_IsNull(next_cursor))
    {
    goto end; //String
    }
    }


    if (next_cursor && !cJSON_IsNull(next_cursor)) next_cursor_local_str = strdup(next_cursor->valuestring);

    code_find_response_local_var = code_find_response_create_internal (
        hits ? hitsList : NULL,
        next_cursor_local_str
        );

    if (!code_find_response_local_var) {
        goto end;
    }

    return code_find_response_local_var;
end:
    if (hitsList) {
        listEntry_t *listEntry = NULL;
        list_ForEach(listEntry, hitsList) {
            code_find_hit_free(listEntry->data);
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
