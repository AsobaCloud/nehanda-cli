#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "entity_profile_response.h"



static entity_profile_response_t *entity_profile_response_create_internal(
    char *entity,
    char *kind,
    char *summary,
    list_t *facts,
    list_t *tags,
    char *updated_at
    ) {
    entity_profile_response_t *entity_profile_response_local_var = malloc(sizeof(entity_profile_response_t));
    if (!entity_profile_response_local_var) {
        return NULL;
    }
    memset(entity_profile_response_local_var, 0, sizeof(entity_profile_response_t));
    entity_profile_response_local_var->_library_owned = 1;
    entity_profile_response_local_var->entity = entity;
    entity_profile_response_local_var->kind = kind;
    entity_profile_response_local_var->summary = summary;
    entity_profile_response_local_var->facts = facts;
    entity_profile_response_local_var->tags = tags;
    entity_profile_response_local_var->updated_at = updated_at;
    return entity_profile_response_local_var;
}

__attribute__((deprecated)) entity_profile_response_t *entity_profile_response_create(
    char *entity,
    char *kind,
    char *summary,
    list_t *facts,
    list_t *tags,
    char *updated_at
    ) {
    entity_profile_response_t *result = entity_profile_response_create_internal (
        entity,
        kind,
        summary,
        facts,
        tags,
        updated_at
        );
    if (!result) {
    }
    return result;
}

void entity_profile_response_free(entity_profile_response_t *entity_profile_response) {
    if(NULL == entity_profile_response){
        return ;
    }
    if(entity_profile_response->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "entity_profile_response_free");
        return ;
    }
    listEntry_t *listEntry;
    if (entity_profile_response->entity) {
        free(entity_profile_response->entity);
        entity_profile_response->entity = NULL;
    }
    if (entity_profile_response->kind) {
        free(entity_profile_response->kind);
        entity_profile_response->kind = NULL;
    }
    if (entity_profile_response->summary) {
        free(entity_profile_response->summary);
        entity_profile_response->summary = NULL;
    }
    if (entity_profile_response->facts) {
        list_ForEach(listEntry, entity_profile_response->facts) {
            free(listEntry->data);
        }
        list_freeList(entity_profile_response->facts);
        entity_profile_response->facts = NULL;
    }
    if (entity_profile_response->tags) {
        list_ForEach(listEntry, entity_profile_response->tags) {
            free(listEntry->data);
        }
        list_freeList(entity_profile_response->tags);
        entity_profile_response->tags = NULL;
    }
    if (entity_profile_response->updated_at) {
        free(entity_profile_response->updated_at);
        entity_profile_response->updated_at = NULL;
    }
    free(entity_profile_response);
}

cJSON *entity_profile_response_convertToJSON(entity_profile_response_t *entity_profile_response) {
    cJSON *item = cJSON_CreateObject();

    // entity_profile_response->entity
    if(entity_profile_response->entity) {
    if(cJSON_AddStringToObject(item, "entity", entity_profile_response->entity) == NULL) {
    goto fail; //String
    }
    }


    // entity_profile_response->kind
    if(entity_profile_response->kind) {
    if(cJSON_AddStringToObject(item, "kind", entity_profile_response->kind) == NULL) {
    goto fail; //String
    }
    }


    // entity_profile_response->summary
    if(entity_profile_response->summary) {
    if(cJSON_AddStringToObject(item, "summary", entity_profile_response->summary) == NULL) {
    goto fail; //String
    }
    }


    // entity_profile_response->facts
    if(entity_profile_response->facts) {
    cJSON *facts = cJSON_AddArrayToObject(item, "facts");
    if(facts == NULL) {
        goto fail; //primitive container
    }

    listEntry_t *factsListEntry;
    list_ForEach(factsListEntry, entity_profile_response->facts) {
    if(cJSON_AddStringToObject(facts, "", factsListEntry->data) == NULL)
    {
        goto fail;
    }
    }
    }


    // entity_profile_response->tags
    if(entity_profile_response->tags) {
    cJSON *tags = cJSON_AddArrayToObject(item, "tags");
    if(tags == NULL) {
        goto fail; //primitive container
    }

    listEntry_t *tagsListEntry;
    list_ForEach(tagsListEntry, entity_profile_response->tags) {
    if(cJSON_AddStringToObject(tags, "", tagsListEntry->data) == NULL)
    {
        goto fail;
    }
    }
    }


    // entity_profile_response->updated_at
    if(entity_profile_response->updated_at) {
    if(cJSON_AddStringToObject(item, "updated_at", entity_profile_response->updated_at) == NULL) {
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

entity_profile_response_t *entity_profile_response_parseFromJSON(cJSON *entity_profile_responseJSON){

    entity_profile_response_t *entity_profile_response_local_var = NULL;

    char *entity_local_str = NULL;

    char *kind_local_str = NULL;

    char *summary_local_str = NULL;

    // define the local list for entity_profile_response->facts
    list_t *factsList = NULL;

    // define the local list for entity_profile_response->tags
    list_t *tagsList = NULL;

    char *updated_at_local_str = NULL;

    // entity_profile_response->entity
    cJSON *entity = cJSON_GetObjectItemCaseSensitive(entity_profile_responseJSON, "entity");
    if (cJSON_IsNull(entity)) {
        entity = NULL;
    }
    if (entity) { 
    if(!cJSON_IsString(entity) && !cJSON_IsNull(entity))
    {
    goto end; //String
    }
    }

    // entity_profile_response->kind
    cJSON *kind = cJSON_GetObjectItemCaseSensitive(entity_profile_responseJSON, "kind");
    if (cJSON_IsNull(kind)) {
        kind = NULL;
    }
    if (kind) { 
    if(!cJSON_IsString(kind) && !cJSON_IsNull(kind))
    {
    goto end; //String
    }
    }

    // entity_profile_response->summary
    cJSON *summary = cJSON_GetObjectItemCaseSensitive(entity_profile_responseJSON, "summary");
    if (cJSON_IsNull(summary)) {
        summary = NULL;
    }
    if (summary) { 
    if(!cJSON_IsString(summary) && !cJSON_IsNull(summary))
    {
    goto end; //String
    }
    }

    // entity_profile_response->facts
    cJSON *facts = cJSON_GetObjectItemCaseSensitive(entity_profile_responseJSON, "facts");
    if (cJSON_IsNull(facts)) {
        facts = NULL;
    }
    if (facts) { 
    cJSON *facts_local = NULL;
    if(!cJSON_IsArray(facts)) {
        goto end;//primitive container
    }
    factsList = list_createList();

    cJSON_ArrayForEach(facts_local, facts)
    {
        if(!cJSON_IsString(facts_local))
        {
            goto end;
        }
        list_addElement(factsList , strdup(facts_local->valuestring));
    }
    }

    // entity_profile_response->tags
    cJSON *tags = cJSON_GetObjectItemCaseSensitive(entity_profile_responseJSON, "tags");
    if (cJSON_IsNull(tags)) {
        tags = NULL;
    }
    if (tags) { 
    cJSON *tags_local = NULL;
    if(!cJSON_IsArray(tags)) {
        goto end;//primitive container
    }
    tagsList = list_createList();

    cJSON_ArrayForEach(tags_local, tags)
    {
        if(!cJSON_IsString(tags_local))
        {
            goto end;
        }
        list_addElement(tagsList , strdup(tags_local->valuestring));
    }
    }

    // entity_profile_response->updated_at
    cJSON *updated_at = cJSON_GetObjectItemCaseSensitive(entity_profile_responseJSON, "updated_at");
    if (cJSON_IsNull(updated_at)) {
        updated_at = NULL;
    }
    if (updated_at) { 
    if(!cJSON_IsString(updated_at) && !cJSON_IsNull(updated_at))
    {
    goto end; //String
    }
    }


    if (entity && !cJSON_IsNull(entity)) entity_local_str = strdup(entity->valuestring);
    if (kind && !cJSON_IsNull(kind)) kind_local_str = strdup(kind->valuestring);
    if (summary && !cJSON_IsNull(summary)) summary_local_str = strdup(summary->valuestring);
    if (updated_at && !cJSON_IsNull(updated_at)) updated_at_local_str = strdup(updated_at->valuestring);

    entity_profile_response_local_var = entity_profile_response_create_internal (
        entity_local_str,
        kind_local_str,
        summary_local_str,
        facts ? factsList : NULL,
        tags ? tagsList : NULL,
        updated_at_local_str
        );

    if (!entity_profile_response_local_var) {
        goto end;
    }

    return entity_profile_response_local_var;
end:
    if (entity_local_str) {
        free(entity_local_str);
        entity_local_str = NULL;
    }
    if (kind_local_str) {
        free(kind_local_str);
        kind_local_str = NULL;
    }
    if (summary_local_str) {
        free(summary_local_str);
        summary_local_str = NULL;
    }
    if (factsList) {
        listEntry_t *listEntry = NULL;
        list_ForEach(listEntry, factsList) {
            free(listEntry->data);
            listEntry->data = NULL;
        }
        list_freeList(factsList);
        factsList = NULL;
    }
    if (tagsList) {
        listEntry_t *listEntry = NULL;
        list_ForEach(listEntry, tagsList) {
            free(listEntry->data);
            listEntry->data = NULL;
        }
        list_freeList(tagsList);
        tagsList = NULL;
    }
    if (updated_at_local_str) {
        free(updated_at_local_str);
        updated_at_local_str = NULL;
    }
    return NULL;

}
