#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "entity_search_response_entities_inner.h"



static entity_search_response_entities_inner_t *entity_search_response_entities_inner_create_internal(
    char *entity,
    char *kind,
    char *summary,
    double *score
    ) {
    entity_search_response_entities_inner_t *entity_search_response_entities_inner_local_var = malloc(sizeof(entity_search_response_entities_inner_t));
    if (!entity_search_response_entities_inner_local_var) {
        return NULL;
    }
    memset(entity_search_response_entities_inner_local_var, 0, sizeof(entity_search_response_entities_inner_t));
    entity_search_response_entities_inner_local_var->_library_owned = 1;
    entity_search_response_entities_inner_local_var->entity = entity;
    entity_search_response_entities_inner_local_var->kind = kind;
    entity_search_response_entities_inner_local_var->summary = summary;
    entity_search_response_entities_inner_local_var->score = score;
    return entity_search_response_entities_inner_local_var;
}

__attribute__((deprecated)) entity_search_response_entities_inner_t *entity_search_response_entities_inner_create(
    char *entity,
    char *kind,
    char *summary,
    double *score
    ) {
    double *score_copy = NULL;
    if (score) {
        score_copy = malloc(sizeof(double));
        if (score_copy) *score_copy = *score;
    }
    entity_search_response_entities_inner_t *result = entity_search_response_entities_inner_create_internal (
        entity,
        kind,
        summary,
        score_copy
        );
    if (!result) {
        free(score_copy);
    }
    return result;
}

void entity_search_response_entities_inner_free(entity_search_response_entities_inner_t *entity_search_response_entities_inner) {
    if(NULL == entity_search_response_entities_inner){
        return ;
    }
    if(entity_search_response_entities_inner->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "entity_search_response_entities_inner_free");
        return ;
    }
    listEntry_t *listEntry;
    if (entity_search_response_entities_inner->entity) {
        free(entity_search_response_entities_inner->entity);
        entity_search_response_entities_inner->entity = NULL;
    }
    if (entity_search_response_entities_inner->kind) {
        free(entity_search_response_entities_inner->kind);
        entity_search_response_entities_inner->kind = NULL;
    }
    if (entity_search_response_entities_inner->summary) {
        free(entity_search_response_entities_inner->summary);
        entity_search_response_entities_inner->summary = NULL;
    }
    if (entity_search_response_entities_inner->score) {
        free(entity_search_response_entities_inner->score);
        entity_search_response_entities_inner->score = NULL;
    }
    free(entity_search_response_entities_inner);
}

cJSON *entity_search_response_entities_inner_convertToJSON(entity_search_response_entities_inner_t *entity_search_response_entities_inner) {
    cJSON *item = cJSON_CreateObject();

    // entity_search_response_entities_inner->entity
    if(entity_search_response_entities_inner->entity) {
    if(cJSON_AddStringToObject(item, "entity", entity_search_response_entities_inner->entity) == NULL) {
    goto fail; //String
    }
    }


    // entity_search_response_entities_inner->kind
    if(entity_search_response_entities_inner->kind) {
    if(cJSON_AddStringToObject(item, "kind", entity_search_response_entities_inner->kind) == NULL) {
    goto fail; //String
    }
    }


    // entity_search_response_entities_inner->summary
    if(entity_search_response_entities_inner->summary) {
    if(cJSON_AddStringToObject(item, "summary", entity_search_response_entities_inner->summary) == NULL) {
    goto fail; //String
    }
    }


    // entity_search_response_entities_inner->score
    if(entity_search_response_entities_inner->score) {
    if(cJSON_AddNumberToObject(item, "score", *entity_search_response_entities_inner->score) == NULL) {
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

entity_search_response_entities_inner_t *entity_search_response_entities_inner_parseFromJSON(cJSON *entity_search_response_entities_innerJSON){

    entity_search_response_entities_inner_t *entity_search_response_entities_inner_local_var = NULL;

    char *entity_local_str = NULL;

    char *kind_local_str = NULL;

    char *summary_local_str = NULL;

    // define the local variable for entity_search_response_entities_inner->score
    double *score_local_var = NULL;

    // entity_search_response_entities_inner->entity
    cJSON *entity = cJSON_GetObjectItemCaseSensitive(entity_search_response_entities_innerJSON, "entity");
    if (cJSON_IsNull(entity)) {
        entity = NULL;
    }
    if (entity) { 
    if(!cJSON_IsString(entity) && !cJSON_IsNull(entity))
    {
    goto end; //String
    }
    }

    // entity_search_response_entities_inner->kind
    cJSON *kind = cJSON_GetObjectItemCaseSensitive(entity_search_response_entities_innerJSON, "kind");
    if (cJSON_IsNull(kind)) {
        kind = NULL;
    }
    if (kind) { 
    if(!cJSON_IsString(kind) && !cJSON_IsNull(kind))
    {
    goto end; //String
    }
    }

    // entity_search_response_entities_inner->summary
    cJSON *summary = cJSON_GetObjectItemCaseSensitive(entity_search_response_entities_innerJSON, "summary");
    if (cJSON_IsNull(summary)) {
        summary = NULL;
    }
    if (summary) { 
    if(!cJSON_IsString(summary) && !cJSON_IsNull(summary))
    {
    goto end; //String
    }
    }

    // entity_search_response_entities_inner->score
    cJSON *score = cJSON_GetObjectItemCaseSensitive(entity_search_response_entities_innerJSON, "score");
    if (cJSON_IsNull(score)) {
        score = NULL;
    }
    if (score) { 
    if(!cJSON_IsNumber(score))
    {
    goto end; //Numeric
    }
    score_local_var = malloc(sizeof(double));
    if(!score_local_var)
    {
        goto end;
    }
    *score_local_var = score->valuedouble;
    }


    if (entity && !cJSON_IsNull(entity)) entity_local_str = strdup(entity->valuestring);
    if (kind && !cJSON_IsNull(kind)) kind_local_str = strdup(kind->valuestring);
    if (summary && !cJSON_IsNull(summary)) summary_local_str = strdup(summary->valuestring);

    entity_search_response_entities_inner_local_var = entity_search_response_entities_inner_create_internal (
        entity_local_str,
        kind_local_str,
        summary_local_str,
        score_local_var
        );

    if (!entity_search_response_entities_inner_local_var) {
        goto end;
    }

    return entity_search_response_entities_inner_local_var;
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
    if (score_local_var) {
        free(score_local_var);
        score_local_var = NULL;
    }
    return NULL;

}
