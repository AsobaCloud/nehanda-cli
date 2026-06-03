#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "search_response.h"



static search_response_t *search_response_create_internal(
    list_t *hits,
    char *next_cursor,
    int *total_hits,
    char *fusion_mode_used
    ) {
    search_response_t *search_response_local_var = malloc(sizeof(search_response_t));
    if (!search_response_local_var) {
        return NULL;
    }
    memset(search_response_local_var, 0, sizeof(search_response_t));
    search_response_local_var->_library_owned = 1;
    search_response_local_var->hits = hits;
    search_response_local_var->next_cursor = next_cursor;
    search_response_local_var->total_hits = total_hits;
    search_response_local_var->fusion_mode_used = fusion_mode_used;
    return search_response_local_var;
}

__attribute__((deprecated)) search_response_t *search_response_create(
    list_t *hits,
    char *next_cursor,
    int *total_hits,
    char *fusion_mode_used
    ) {
    int *total_hits_copy = NULL;
    if (total_hits) {
        total_hits_copy = malloc(sizeof(int));
        if (total_hits_copy) *total_hits_copy = *total_hits;
    }
    search_response_t *result = search_response_create_internal (
        hits,
        next_cursor,
        total_hits_copy,
        fusion_mode_used
        );
    if (!result) {
        free(total_hits_copy);
    }
    return result;
}

void search_response_free(search_response_t *search_response) {
    if(NULL == search_response){
        return ;
    }
    if(search_response->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "search_response_free");
        return ;
    }
    listEntry_t *listEntry;
    if (search_response->hits) {
        list_ForEach(listEntry, search_response->hits) {
            search_hit_free(listEntry->data);
        }
        list_freeList(search_response->hits);
        search_response->hits = NULL;
    }
    if (search_response->next_cursor) {
        free(search_response->next_cursor);
        search_response->next_cursor = NULL;
    }
    if (search_response->total_hits) {
        free(search_response->total_hits);
        search_response->total_hits = NULL;
    }
    if (search_response->fusion_mode_used) {
        free(search_response->fusion_mode_used);
        search_response->fusion_mode_used = NULL;
    }
    free(search_response);
}

cJSON *search_response_convertToJSON(search_response_t *search_response) {
    cJSON *item = cJSON_CreateObject();

    // search_response->hits
    if(search_response->hits) {
    cJSON *hits = cJSON_AddArrayToObject(item, "hits");
    if(hits == NULL) {
    goto fail; //nonprimitive container
    }

    listEntry_t *hitsListEntry;
    if (search_response->hits) {
    list_ForEach(hitsListEntry, search_response->hits) {
    cJSON *itemLocal = search_hit_convertToJSON(hitsListEntry->data);
    if(itemLocal == NULL) {
    goto fail;
    }
    cJSON_AddItemToArray(hits, itemLocal);
    }
    }
    }


    // search_response->next_cursor
    if(search_response->next_cursor) {
    if(cJSON_AddStringToObject(item, "next_cursor", search_response->next_cursor) == NULL) {
    goto fail; //String
    }
    }


    // search_response->total_hits
    if(search_response->total_hits) {
    if(cJSON_AddNumberToObject(item, "total_hits", *search_response->total_hits) == NULL) {
    goto fail; //Numeric
    }
    }


    // search_response->fusion_mode_used
    if(search_response->fusion_mode_used) {
    if(cJSON_AddStringToObject(item, "fusion_mode_used", search_response->fusion_mode_used) == NULL) {
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

search_response_t *search_response_parseFromJSON(cJSON *search_responseJSON){

    search_response_t *search_response_local_var = NULL;

    // define the local list for search_response->hits
    list_t *hitsList = NULL;

    char *next_cursor_local_str = NULL;

    // define the local variable for search_response->total_hits
    int *total_hits_local_var = NULL;

    char *fusion_mode_used_local_str = NULL;

    // search_response->hits
    cJSON *hits = cJSON_GetObjectItemCaseSensitive(search_responseJSON, "hits");
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
        search_hit_t *hitsItem = search_hit_parseFromJSON(hits_local_nonprimitive);

        list_addElement(hitsList, hitsItem);
    }
    }

    // search_response->next_cursor
    cJSON *next_cursor = cJSON_GetObjectItemCaseSensitive(search_responseJSON, "next_cursor");
    if (cJSON_IsNull(next_cursor)) {
        next_cursor = NULL;
    }
    if (next_cursor) { 
    if(!cJSON_IsString(next_cursor) && !cJSON_IsNull(next_cursor))
    {
    goto end; //String
    }
    }

    // search_response->total_hits
    cJSON *total_hits = cJSON_GetObjectItemCaseSensitive(search_responseJSON, "total_hits");
    if (cJSON_IsNull(total_hits)) {
        total_hits = NULL;
    }
    if (total_hits) { 
    if(!cJSON_IsNumber(total_hits))
    {
    goto end; //Numeric
    }
    total_hits_local_var = malloc(sizeof(int));
    if(!total_hits_local_var)
    {
        goto end;
    }
    *total_hits_local_var = total_hits->valuedouble;
    }

    // search_response->fusion_mode_used
    cJSON *fusion_mode_used = cJSON_GetObjectItemCaseSensitive(search_responseJSON, "fusion_mode_used");
    if (cJSON_IsNull(fusion_mode_used)) {
        fusion_mode_used = NULL;
    }
    if (fusion_mode_used) { 
    if(!cJSON_IsString(fusion_mode_used) && !cJSON_IsNull(fusion_mode_used))
    {
    goto end; //String
    }
    }


    if (next_cursor && !cJSON_IsNull(next_cursor)) next_cursor_local_str = strdup(next_cursor->valuestring);
    if (fusion_mode_used && !cJSON_IsNull(fusion_mode_used)) fusion_mode_used_local_str = strdup(fusion_mode_used->valuestring);

    search_response_local_var = search_response_create_internal (
        hits ? hitsList : NULL,
        next_cursor_local_str,
        total_hits_local_var,
        fusion_mode_used_local_str
        );

    if (!search_response_local_var) {
        goto end;
    }

    return search_response_local_var;
end:
    if (hitsList) {
        listEntry_t *listEntry = NULL;
        list_ForEach(listEntry, hitsList) {
            search_hit_free(listEntry->data);
            listEntry->data = NULL;
        }
        list_freeList(hitsList);
        hitsList = NULL;
    }
    if (next_cursor_local_str) {
        free(next_cursor_local_str);
        next_cursor_local_str = NULL;
    }
    if (total_hits_local_var) {
        free(total_hits_local_var);
        total_hits_local_var = NULL;
    }
    if (fusion_mode_used_local_str) {
        free(fusion_mode_used_local_str);
        fusion_mode_used_local_str = NULL;
    }
    return NULL;

}
