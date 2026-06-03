#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "search_hit.h"



static search_hit_t *search_hit_create_internal(
    char *artifact_id,
    double *score,
    char *kind,
    char *excerpt,
    list_t *citations
    ) {
    search_hit_t *search_hit_local_var = malloc(sizeof(search_hit_t));
    if (!search_hit_local_var) {
        return NULL;
    }
    memset(search_hit_local_var, 0, sizeof(search_hit_t));
    search_hit_local_var->_library_owned = 1;
    search_hit_local_var->artifact_id = artifact_id;
    search_hit_local_var->score = score;
    search_hit_local_var->kind = kind;
    search_hit_local_var->excerpt = excerpt;
    search_hit_local_var->citations = citations;
    return search_hit_local_var;
}

__attribute__((deprecated)) search_hit_t *search_hit_create(
    char *artifact_id,
    double *score,
    char *kind,
    char *excerpt,
    list_t *citations
    ) {
    double *score_copy = NULL;
    if (score) {
        score_copy = malloc(sizeof(double));
        if (score_copy) *score_copy = *score;
    }
    search_hit_t *result = search_hit_create_internal (
        artifact_id,
        score_copy,
        kind,
        excerpt,
        citations
        );
    if (!result) {
        free(score_copy);
    }
    return result;
}

void search_hit_free(search_hit_t *search_hit) {
    if(NULL == search_hit){
        return ;
    }
    if(search_hit->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "search_hit_free");
        return ;
    }
    listEntry_t *listEntry;
    if (search_hit->artifact_id) {
        free(search_hit->artifact_id);
        search_hit->artifact_id = NULL;
    }
    if (search_hit->score) {
        free(search_hit->score);
        search_hit->score = NULL;
    }
    if (search_hit->kind) {
        free(search_hit->kind);
        search_hit->kind = NULL;
    }
    if (search_hit->excerpt) {
        free(search_hit->excerpt);
        search_hit->excerpt = NULL;
    }
    if (search_hit->citations) {
        list_ForEach(listEntry, search_hit->citations) {
            search_hit_citations_inner_free(listEntry->data);
        }
        list_freeList(search_hit->citations);
        search_hit->citations = NULL;
    }
    free(search_hit);
}

cJSON *search_hit_convertToJSON(search_hit_t *search_hit) {
    cJSON *item = cJSON_CreateObject();

    // search_hit->artifact_id
    if(search_hit->artifact_id) {
    if(cJSON_AddStringToObject(item, "artifact_id", search_hit->artifact_id) == NULL) {
    goto fail; //String
    }
    }


    // search_hit->score
    if(search_hit->score) {
    if(cJSON_AddNumberToObject(item, "score", *search_hit->score) == NULL) {
    goto fail; //Numeric
    }
    }


    // search_hit->kind
    if(search_hit->kind) {
    if(cJSON_AddStringToObject(item, "kind", search_hit->kind) == NULL) {
    goto fail; //String
    }
    }


    // search_hit->excerpt
    if(search_hit->excerpt) {
    if(cJSON_AddStringToObject(item, "excerpt", search_hit->excerpt) == NULL) {
    goto fail; //String
    }
    }


    // search_hit->citations
    if(search_hit->citations) {
    cJSON *citations = cJSON_AddArrayToObject(item, "citations");
    if(citations == NULL) {
    goto fail; //nonprimitive container
    }

    listEntry_t *citationsListEntry;
    if (search_hit->citations) {
    list_ForEach(citationsListEntry, search_hit->citations) {
    cJSON *itemLocal = search_hit_citations_inner_convertToJSON(citationsListEntry->data);
    if(itemLocal == NULL) {
    goto fail;
    }
    cJSON_AddItemToArray(citations, itemLocal);
    }
    }
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

search_hit_t *search_hit_parseFromJSON(cJSON *search_hitJSON){

    search_hit_t *search_hit_local_var = NULL;

    char *artifact_id_local_str = NULL;

    // define the local variable for search_hit->score
    double *score_local_var = NULL;

    char *kind_local_str = NULL;

    char *excerpt_local_str = NULL;

    // define the local list for search_hit->citations
    list_t *citationsList = NULL;

    // search_hit->artifact_id
    cJSON *artifact_id = cJSON_GetObjectItemCaseSensitive(search_hitJSON, "artifact_id");
    if (cJSON_IsNull(artifact_id)) {
        artifact_id = NULL;
    }
    if (artifact_id) { 
    if(!cJSON_IsString(artifact_id) && !cJSON_IsNull(artifact_id))
    {
    goto end; //String
    }
    }

    // search_hit->score
    cJSON *score = cJSON_GetObjectItemCaseSensitive(search_hitJSON, "score");
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

    // search_hit->kind
    cJSON *kind = cJSON_GetObjectItemCaseSensitive(search_hitJSON, "kind");
    if (cJSON_IsNull(kind)) {
        kind = NULL;
    }
    if (kind) { 
    if(!cJSON_IsString(kind) && !cJSON_IsNull(kind))
    {
    goto end; //String
    }
    }

    // search_hit->excerpt
    cJSON *excerpt = cJSON_GetObjectItemCaseSensitive(search_hitJSON, "excerpt");
    if (cJSON_IsNull(excerpt)) {
        excerpt = NULL;
    }
    if (excerpt) { 
    if(!cJSON_IsString(excerpt) && !cJSON_IsNull(excerpt))
    {
    goto end; //String
    }
    }

    // search_hit->citations
    cJSON *citations = cJSON_GetObjectItemCaseSensitive(search_hitJSON, "citations");
    if (cJSON_IsNull(citations)) {
        citations = NULL;
    }
    if (citations) { 
    cJSON *citations_local_nonprimitive = NULL;
    if(!cJSON_IsArray(citations)){
        goto end; //nonprimitive container
    }

    citationsList = list_createList();

    cJSON_ArrayForEach(citations_local_nonprimitive,citations )
    {
        if(!cJSON_IsObject(citations_local_nonprimitive)){
            goto end;
        }
        search_hit_citations_inner_t *citationsItem = search_hit_citations_inner_parseFromJSON(citations_local_nonprimitive);

        list_addElement(citationsList, citationsItem);
    }
    }


    if (artifact_id && !cJSON_IsNull(artifact_id)) artifact_id_local_str = strdup(artifact_id->valuestring);
    if (kind && !cJSON_IsNull(kind)) kind_local_str = strdup(kind->valuestring);
    if (excerpt && !cJSON_IsNull(excerpt)) excerpt_local_str = strdup(excerpt->valuestring);

    search_hit_local_var = search_hit_create_internal (
        artifact_id_local_str,
        score_local_var,
        kind_local_str,
        excerpt_local_str,
        citations ? citationsList : NULL
        );

    if (!search_hit_local_var) {
        goto end;
    }

    return search_hit_local_var;
end:
    if (artifact_id_local_str) {
        free(artifact_id_local_str);
        artifact_id_local_str = NULL;
    }
    if (score_local_var) {
        free(score_local_var);
        score_local_var = NULL;
    }
    if (kind_local_str) {
        free(kind_local_str);
        kind_local_str = NULL;
    }
    if (excerpt_local_str) {
        free(excerpt_local_str);
        excerpt_local_str = NULL;
    }
    if (citationsList) {
        listEntry_t *listEntry = NULL;
        list_ForEach(listEntry, citationsList) {
            search_hit_citations_inner_free(listEntry->data);
            listEntry->data = NULL;
        }
        list_freeList(citationsList);
        citationsList = NULL;
    }
    return NULL;

}
