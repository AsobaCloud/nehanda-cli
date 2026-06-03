#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "artifact_response.h"



static artifact_response_t *artifact_response_create_internal(
    char *id,
    char *kind,
    char *state,
    char *scope_kind,
    char *scope_id,
    double *confidence,
    object_t *payload,
    list_t *citations,
    char *updated_at
    ) {
    artifact_response_t *artifact_response_local_var = malloc(sizeof(artifact_response_t));
    if (!artifact_response_local_var) {
        return NULL;
    }
    memset(artifact_response_local_var, 0, sizeof(artifact_response_t));
    artifact_response_local_var->_library_owned = 1;
    artifact_response_local_var->id = id;
    artifact_response_local_var->kind = kind;
    artifact_response_local_var->state = state;
    artifact_response_local_var->scope_kind = scope_kind;
    artifact_response_local_var->scope_id = scope_id;
    artifact_response_local_var->confidence = confidence;
    artifact_response_local_var->payload = payload;
    artifact_response_local_var->citations = citations;
    artifact_response_local_var->updated_at = updated_at;
    return artifact_response_local_var;
}

__attribute__((deprecated)) artifact_response_t *artifact_response_create(
    char *id,
    char *kind,
    char *state,
    char *scope_kind,
    char *scope_id,
    double *confidence,
    object_t *payload,
    list_t *citations,
    char *updated_at
    ) {
    double *confidence_copy = NULL;
    if (confidence) {
        confidence_copy = malloc(sizeof(double));
        if (confidence_copy) *confidence_copy = *confidence;
    }
    artifact_response_t *result = artifact_response_create_internal (
        id,
        kind,
        state,
        scope_kind,
        scope_id,
        confidence_copy,
        payload,
        citations,
        updated_at
        );
    if (!result) {
        free(confidence_copy);
    }
    return result;
}

void artifact_response_free(artifact_response_t *artifact_response) {
    if(NULL == artifact_response){
        return ;
    }
    if(artifact_response->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "artifact_response_free");
        return ;
    }
    listEntry_t *listEntry;
    if (artifact_response->id) {
        free(artifact_response->id);
        artifact_response->id = NULL;
    }
    if (artifact_response->kind) {
        free(artifact_response->kind);
        artifact_response->kind = NULL;
    }
    if (artifact_response->state) {
        free(artifact_response->state);
        artifact_response->state = NULL;
    }
    if (artifact_response->scope_kind) {
        free(artifact_response->scope_kind);
        artifact_response->scope_kind = NULL;
    }
    if (artifact_response->scope_id) {
        free(artifact_response->scope_id);
        artifact_response->scope_id = NULL;
    }
    if (artifact_response->confidence) {
        free(artifact_response->confidence);
        artifact_response->confidence = NULL;
    }
    if (artifact_response->payload) {
        object_free(artifact_response->payload);
        artifact_response->payload = NULL;
    }
    if (artifact_response->citations) {
        list_ForEach(listEntry, artifact_response->citations) {
            search_hit_citations_inner_free(listEntry->data);
        }
        list_freeList(artifact_response->citations);
        artifact_response->citations = NULL;
    }
    if (artifact_response->updated_at) {
        free(artifact_response->updated_at);
        artifact_response->updated_at = NULL;
    }
    free(artifact_response);
}

cJSON *artifact_response_convertToJSON(artifact_response_t *artifact_response) {
    cJSON *item = cJSON_CreateObject();

    // artifact_response->id
    if(artifact_response->id) {
    if(cJSON_AddStringToObject(item, "id", artifact_response->id) == NULL) {
    goto fail; //String
    }
    }


    // artifact_response->kind
    if(artifact_response->kind) {
    if(cJSON_AddStringToObject(item, "kind", artifact_response->kind) == NULL) {
    goto fail; //String
    }
    }


    // artifact_response->state
    if(artifact_response->state) {
    if(cJSON_AddStringToObject(item, "state", artifact_response->state) == NULL) {
    goto fail; //String
    }
    }


    // artifact_response->scope_kind
    if(artifact_response->scope_kind) {
    if(cJSON_AddStringToObject(item, "scope_kind", artifact_response->scope_kind) == NULL) {
    goto fail; //String
    }
    }


    // artifact_response->scope_id
    if(artifact_response->scope_id) {
    if(cJSON_AddStringToObject(item, "scope_id", artifact_response->scope_id) == NULL) {
    goto fail; //String
    }
    }


    // artifact_response->confidence
    if(artifact_response->confidence) {
    if(cJSON_AddNumberToObject(item, "confidence", *artifact_response->confidence) == NULL) {
    goto fail; //Numeric
    }
    }


    // artifact_response->payload
    if(artifact_response->payload) {
    cJSON *payload_object = object_convertToJSON(artifact_response->payload);
    if(payload_object == NULL) {
    goto fail; //model
    }
    cJSON_AddItemToObject(item, "payload", payload_object);
    if(item->child == NULL) {
    goto fail;
    }
    }


    // artifact_response->citations
    if(artifact_response->citations) {
    cJSON *citations = cJSON_AddArrayToObject(item, "citations");
    if(citations == NULL) {
    goto fail; //nonprimitive container
    }

    listEntry_t *citationsListEntry;
    if (artifact_response->citations) {
    list_ForEach(citationsListEntry, artifact_response->citations) {
    cJSON *itemLocal = search_hit_citations_inner_convertToJSON(citationsListEntry->data);
    if(itemLocal == NULL) {
    goto fail;
    }
    cJSON_AddItemToArray(citations, itemLocal);
    }
    }
    }


    // artifact_response->updated_at
    if(artifact_response->updated_at) {
    if(cJSON_AddStringToObject(item, "updated_at", artifact_response->updated_at) == NULL) {
    goto fail; //Date-Time
    }
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

artifact_response_t *artifact_response_parseFromJSON(cJSON *artifact_responseJSON){

    artifact_response_t *artifact_response_local_var = NULL;

    char *id_local_str = NULL;

    char *kind_local_str = NULL;

    char *state_local_str = NULL;

    char *scope_kind_local_str = NULL;

    char *scope_id_local_str = NULL;

    // define the local variable for artifact_response->confidence
    double *confidence_local_var = NULL;

    // define the local list for artifact_response->citations
    list_t *citationsList = NULL;

    char *updated_at_local_str = NULL;

    // artifact_response->id
    cJSON *id = cJSON_GetObjectItemCaseSensitive(artifact_responseJSON, "id");
    if (cJSON_IsNull(id)) {
        id = NULL;
    }
    if (id) { 
    if(!cJSON_IsString(id) && !cJSON_IsNull(id))
    {
    goto end; //String
    }
    }

    // artifact_response->kind
    cJSON *kind = cJSON_GetObjectItemCaseSensitive(artifact_responseJSON, "kind");
    if (cJSON_IsNull(kind)) {
        kind = NULL;
    }
    if (kind) { 
    if(!cJSON_IsString(kind) && !cJSON_IsNull(kind))
    {
    goto end; //String
    }
    }

    // artifact_response->state
    cJSON *state = cJSON_GetObjectItemCaseSensitive(artifact_responseJSON, "state");
    if (cJSON_IsNull(state)) {
        state = NULL;
    }
    if (state) { 
    if(!cJSON_IsString(state) && !cJSON_IsNull(state))
    {
    goto end; //String
    }
    }

    // artifact_response->scope_kind
    cJSON *scope_kind = cJSON_GetObjectItemCaseSensitive(artifact_responseJSON, "scope_kind");
    if (cJSON_IsNull(scope_kind)) {
        scope_kind = NULL;
    }
    if (scope_kind) { 
    if(!cJSON_IsString(scope_kind) && !cJSON_IsNull(scope_kind))
    {
    goto end; //String
    }
    }

    // artifact_response->scope_id
    cJSON *scope_id = cJSON_GetObjectItemCaseSensitive(artifact_responseJSON, "scope_id");
    if (cJSON_IsNull(scope_id)) {
        scope_id = NULL;
    }
    if (scope_id) { 
    if(!cJSON_IsString(scope_id) && !cJSON_IsNull(scope_id))
    {
    goto end; //String
    }
    }

    // artifact_response->confidence
    cJSON *confidence = cJSON_GetObjectItemCaseSensitive(artifact_responseJSON, "confidence");
    if (cJSON_IsNull(confidence)) {
        confidence = NULL;
    }
    if (confidence) { 
    if(!cJSON_IsNumber(confidence))
    {
    goto end; //Numeric
    }
    confidence_local_var = malloc(sizeof(double));
    if(!confidence_local_var)
    {
        goto end;
    }
    *confidence_local_var = confidence->valuedouble;
    }

    // artifact_response->payload
    cJSON *payload = cJSON_GetObjectItemCaseSensitive(artifact_responseJSON, "payload");
    if (cJSON_IsNull(payload)) {
        payload = NULL;
    }
    object_t *payload_local_object = NULL;
    if (payload) { 
    payload_local_object = object_parseFromJSON(payload); //object
    }

    // artifact_response->citations
    cJSON *citations = cJSON_GetObjectItemCaseSensitive(artifact_responseJSON, "citations");
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

    // artifact_response->updated_at
    cJSON *updated_at = cJSON_GetObjectItemCaseSensitive(artifact_responseJSON, "updated_at");
    if (cJSON_IsNull(updated_at)) {
        updated_at = NULL;
    }
    if (updated_at) { 
    if(!cJSON_IsString(updated_at) && !cJSON_IsNull(updated_at))
    {
    goto end; //DateTime
    }
    }


    if (id && !cJSON_IsNull(id)) id_local_str = strdup(id->valuestring);
    if (kind && !cJSON_IsNull(kind)) kind_local_str = strdup(kind->valuestring);
    if (state && !cJSON_IsNull(state)) state_local_str = strdup(state->valuestring);
    if (scope_kind && !cJSON_IsNull(scope_kind)) scope_kind_local_str = strdup(scope_kind->valuestring);
    if (scope_id && !cJSON_IsNull(scope_id)) scope_id_local_str = strdup(scope_id->valuestring);
    if (updated_at && !cJSON_IsNull(updated_at)) updated_at_local_str = strdup(updated_at->valuestring);

    artifact_response_local_var = artifact_response_create_internal (
        id_local_str,
        kind_local_str,
        state_local_str,
        scope_kind_local_str,
        scope_id_local_str,
        confidence_local_var,
        payload ? payload_local_object : NULL,
        citations ? citationsList : NULL,
        updated_at_local_str
        );

    if (!artifact_response_local_var) {
        goto end;
    }

    return artifact_response_local_var;
end:
    if (id_local_str) {
        free(id_local_str);
        id_local_str = NULL;
    }
    if (kind_local_str) {
        free(kind_local_str);
        kind_local_str = NULL;
    }
    if (state_local_str) {
        free(state_local_str);
        state_local_str = NULL;
    }
    if (scope_kind_local_str) {
        free(scope_kind_local_str);
        scope_kind_local_str = NULL;
    }
    if (scope_id_local_str) {
        free(scope_id_local_str);
        scope_id_local_str = NULL;
    }
    if (confidence_local_var) {
        free(confidence_local_var);
        confidence_local_var = NULL;
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
    if (updated_at_local_str) {
        free(updated_at_local_str);
        updated_at_local_str = NULL;
    }
    return NULL;

}
