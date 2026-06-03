#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "entity_search_response.h"



static entity_search_response_t *entity_search_response_create_internal(
    list_t *entities,
    char *next_cursor
    ) {
    entity_search_response_t *entity_search_response_local_var = malloc(sizeof(entity_search_response_t));
    if (!entity_search_response_local_var) {
        return NULL;
    }
    memset(entity_search_response_local_var, 0, sizeof(entity_search_response_t));
    entity_search_response_local_var->_library_owned = 1;
    entity_search_response_local_var->entities = entities;
    entity_search_response_local_var->next_cursor = next_cursor;
    return entity_search_response_local_var;
}

__attribute__((deprecated)) entity_search_response_t *entity_search_response_create(
    list_t *entities,
    char *next_cursor
    ) {
    entity_search_response_t *result = entity_search_response_create_internal (
        entities,
        next_cursor
        );
    if (!result) {
    }
    return result;
}

void entity_search_response_free(entity_search_response_t *entity_search_response) {
    if(NULL == entity_search_response){
        return ;
    }
    if(entity_search_response->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "entity_search_response_free");
        return ;
    }
    listEntry_t *listEntry;
    if (entity_search_response->entities) {
        list_ForEach(listEntry, entity_search_response->entities) {
            entity_search_response_entities_inner_free(listEntry->data);
        }
        list_freeList(entity_search_response->entities);
        entity_search_response->entities = NULL;
    }
    if (entity_search_response->next_cursor) {
        free(entity_search_response->next_cursor);
        entity_search_response->next_cursor = NULL;
    }
    free(entity_search_response);
}

cJSON *entity_search_response_convertToJSON(entity_search_response_t *entity_search_response) {
    cJSON *item = cJSON_CreateObject();

    // entity_search_response->entities
    if(entity_search_response->entities) {
    cJSON *entities = cJSON_AddArrayToObject(item, "entities");
    if(entities == NULL) {
    goto fail; //nonprimitive container
    }

    listEntry_t *entitiesListEntry;
    if (entity_search_response->entities) {
    list_ForEach(entitiesListEntry, entity_search_response->entities) {
    cJSON *itemLocal = entity_search_response_entities_inner_convertToJSON(entitiesListEntry->data);
    if(itemLocal == NULL) {
    goto fail;
    }
    cJSON_AddItemToArray(entities, itemLocal);
    }
    }
    }


    // entity_search_response->next_cursor
    if(entity_search_response->next_cursor) {
    if(cJSON_AddStringToObject(item, "next_cursor", entity_search_response->next_cursor) == NULL) {
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

entity_search_response_t *entity_search_response_parseFromJSON(cJSON *entity_search_responseJSON){

    entity_search_response_t *entity_search_response_local_var = NULL;

    // define the local list for entity_search_response->entities
    list_t *entitiesList = NULL;

    char *next_cursor_local_str = NULL;

    // entity_search_response->entities
    cJSON *entities = cJSON_GetObjectItemCaseSensitive(entity_search_responseJSON, "entities");
    if (cJSON_IsNull(entities)) {
        entities = NULL;
    }
    if (entities) { 
    cJSON *entities_local_nonprimitive = NULL;
    if(!cJSON_IsArray(entities)){
        goto end; //nonprimitive container
    }

    entitiesList = list_createList();

    cJSON_ArrayForEach(entities_local_nonprimitive,entities )
    {
        if(!cJSON_IsObject(entities_local_nonprimitive)){
            goto end;
        }
        entity_search_response_entities_inner_t *entitiesItem = entity_search_response_entities_inner_parseFromJSON(entities_local_nonprimitive);

        list_addElement(entitiesList, entitiesItem);
    }
    }

    // entity_search_response->next_cursor
    cJSON *next_cursor = cJSON_GetObjectItemCaseSensitive(entity_search_responseJSON, "next_cursor");
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

    entity_search_response_local_var = entity_search_response_create_internal (
        entities ? entitiesList : NULL,
        next_cursor_local_str
        );

    if (!entity_search_response_local_var) {
        goto end;
    }

    return entity_search_response_local_var;
end:
    if (entitiesList) {
        listEntry_t *listEntry = NULL;
        list_ForEach(listEntry, entitiesList) {
            entity_search_response_entities_inner_free(listEntry->data);
            listEntry->data = NULL;
        }
        list_freeList(entitiesList);
        entitiesList = NULL;
    }
    if (next_cursor_local_str) {
        free(next_cursor_local_str);
        next_cursor_local_str = NULL;
    }
    return NULL;

}
