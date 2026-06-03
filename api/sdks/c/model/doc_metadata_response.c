#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "doc_metadata_response.h"



static doc_metadata_response_t *doc_metadata_response_create_internal(
    long *id,
    char *filename,
    char *content_hash,
    char *converter,
    char *converter_version,
    char *scope,
    char *state,
    int *review_needed,
    char *created_at
    ) {
    doc_metadata_response_t *doc_metadata_response_local_var = malloc(sizeof(doc_metadata_response_t));
    if (!doc_metadata_response_local_var) {
        return NULL;
    }
    memset(doc_metadata_response_local_var, 0, sizeof(doc_metadata_response_t));
    doc_metadata_response_local_var->_library_owned = 1;
    doc_metadata_response_local_var->id = id;
    doc_metadata_response_local_var->filename = filename;
    doc_metadata_response_local_var->content_hash = content_hash;
    doc_metadata_response_local_var->converter = converter;
    doc_metadata_response_local_var->converter_version = converter_version;
    doc_metadata_response_local_var->scope = scope;
    doc_metadata_response_local_var->state = state;
    doc_metadata_response_local_var->review_needed = review_needed;
    doc_metadata_response_local_var->created_at = created_at;
    return doc_metadata_response_local_var;
}

__attribute__((deprecated)) doc_metadata_response_t *doc_metadata_response_create(
    long *id,
    char *filename,
    char *content_hash,
    char *converter,
    char *converter_version,
    char *scope,
    char *state,
    int *review_needed,
    char *created_at
    ) {
    long *id_copy = NULL;
    if (id) {
        id_copy = malloc(sizeof(long));
        if (id_copy) *id_copy = *id;
    }
    int *review_needed_copy = NULL;
    if (review_needed) {
        review_needed_copy = malloc(sizeof(int));
        if (review_needed_copy) *review_needed_copy = *review_needed;
    }
    doc_metadata_response_t *result = doc_metadata_response_create_internal (
        id_copy,
        filename,
        content_hash,
        converter,
        converter_version,
        scope,
        state,
        review_needed_copy,
        created_at
        );
    if (!result) {
        free(id_copy);
        free(review_needed_copy);
    }
    return result;
}

void doc_metadata_response_free(doc_metadata_response_t *doc_metadata_response) {
    if(NULL == doc_metadata_response){
        return ;
    }
    if(doc_metadata_response->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "doc_metadata_response_free");
        return ;
    }
    listEntry_t *listEntry;
    if (doc_metadata_response->id) {
        free(doc_metadata_response->id);
        doc_metadata_response->id = NULL;
    }
    if (doc_metadata_response->filename) {
        free(doc_metadata_response->filename);
        doc_metadata_response->filename = NULL;
    }
    if (doc_metadata_response->content_hash) {
        free(doc_metadata_response->content_hash);
        doc_metadata_response->content_hash = NULL;
    }
    if (doc_metadata_response->converter) {
        free(doc_metadata_response->converter);
        doc_metadata_response->converter = NULL;
    }
    if (doc_metadata_response->converter_version) {
        free(doc_metadata_response->converter_version);
        doc_metadata_response->converter_version = NULL;
    }
    if (doc_metadata_response->scope) {
        free(doc_metadata_response->scope);
        doc_metadata_response->scope = NULL;
    }
    if (doc_metadata_response->state) {
        free(doc_metadata_response->state);
        doc_metadata_response->state = NULL;
    }
    if (doc_metadata_response->review_needed) {
        free(doc_metadata_response->review_needed);
        doc_metadata_response->review_needed = NULL;
    }
    if (doc_metadata_response->created_at) {
        free(doc_metadata_response->created_at);
        doc_metadata_response->created_at = NULL;
    }
    free(doc_metadata_response);
}

cJSON *doc_metadata_response_convertToJSON(doc_metadata_response_t *doc_metadata_response) {
    cJSON *item = cJSON_CreateObject();

    // doc_metadata_response->id
    if(doc_metadata_response->id) {
    if(cJSON_AddNumberToObject(item, "id", *doc_metadata_response->id) == NULL) {
    goto fail; //Numeric
    }
    }


    // doc_metadata_response->filename
    if(doc_metadata_response->filename) {
    if(cJSON_AddStringToObject(item, "filename", doc_metadata_response->filename) == NULL) {
    goto fail; //String
    }
    }


    // doc_metadata_response->content_hash
    if(doc_metadata_response->content_hash) {
    if(cJSON_AddStringToObject(item, "content_hash", doc_metadata_response->content_hash) == NULL) {
    goto fail; //String
    }
    }


    // doc_metadata_response->converter
    if(doc_metadata_response->converter) {
    if(cJSON_AddStringToObject(item, "converter", doc_metadata_response->converter) == NULL) {
    goto fail; //String
    }
    }


    // doc_metadata_response->converter_version
    if(doc_metadata_response->converter_version) {
    if(cJSON_AddStringToObject(item, "converter_version", doc_metadata_response->converter_version) == NULL) {
    goto fail; //String
    }
    }


    // doc_metadata_response->scope
    if(doc_metadata_response->scope) {
    if(cJSON_AddStringToObject(item, "scope", doc_metadata_response->scope) == NULL) {
    goto fail; //String
    }
    }


    // doc_metadata_response->state
    if(doc_metadata_response->state) {
    if(cJSON_AddStringToObject(item, "state", doc_metadata_response->state) == NULL) {
    goto fail; //String
    }
    }


    // doc_metadata_response->review_needed
    if(doc_metadata_response->review_needed) {
    if(cJSON_AddBoolToObject(item, "review_needed", *doc_metadata_response->review_needed) == NULL) {
    goto fail; //Bool
    }
    }


    // doc_metadata_response->created_at
    if(doc_metadata_response->created_at) {
    if(cJSON_AddStringToObject(item, "created_at", doc_metadata_response->created_at) == NULL) {
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

doc_metadata_response_t *doc_metadata_response_parseFromJSON(cJSON *doc_metadata_responseJSON){

    doc_metadata_response_t *doc_metadata_response_local_var = NULL;

    // define the local variable for doc_metadata_response->id
    long *id_local_var = NULL;

    char *filename_local_str = NULL;

    char *content_hash_local_str = NULL;

    char *converter_local_str = NULL;

    char *converter_version_local_str = NULL;

    char *scope_local_str = NULL;

    char *state_local_str = NULL;

    // define the local variable for doc_metadata_response->review_needed
    int *review_needed_local_var = NULL;

    char *created_at_local_str = NULL;

    // doc_metadata_response->id
    cJSON *id = cJSON_GetObjectItemCaseSensitive(doc_metadata_responseJSON, "id");
    if (cJSON_IsNull(id)) {
        id = NULL;
    }
    if (id) { 
    if(!cJSON_IsNumber(id))
    {
    goto end; //Numeric
    }
    id_local_var = malloc(sizeof(long));
    if(!id_local_var)
    {
        goto end;
    }
    *id_local_var = id->valuedouble;
    }

    // doc_metadata_response->filename
    cJSON *filename = cJSON_GetObjectItemCaseSensitive(doc_metadata_responseJSON, "filename");
    if (cJSON_IsNull(filename)) {
        filename = NULL;
    }
    if (filename) { 
    if(!cJSON_IsString(filename) && !cJSON_IsNull(filename))
    {
    goto end; //String
    }
    }

    // doc_metadata_response->content_hash
    cJSON *content_hash = cJSON_GetObjectItemCaseSensitive(doc_metadata_responseJSON, "content_hash");
    if (cJSON_IsNull(content_hash)) {
        content_hash = NULL;
    }
    if (content_hash) { 
    if(!cJSON_IsString(content_hash) && !cJSON_IsNull(content_hash))
    {
    goto end; //String
    }
    }

    // doc_metadata_response->converter
    cJSON *converter = cJSON_GetObjectItemCaseSensitive(doc_metadata_responseJSON, "converter");
    if (cJSON_IsNull(converter)) {
        converter = NULL;
    }
    if (converter) { 
    if(!cJSON_IsString(converter) && !cJSON_IsNull(converter))
    {
    goto end; //String
    }
    }

    // doc_metadata_response->converter_version
    cJSON *converter_version = cJSON_GetObjectItemCaseSensitive(doc_metadata_responseJSON, "converter_version");
    if (cJSON_IsNull(converter_version)) {
        converter_version = NULL;
    }
    if (converter_version) { 
    if(!cJSON_IsString(converter_version) && !cJSON_IsNull(converter_version))
    {
    goto end; //String
    }
    }

    // doc_metadata_response->scope
    cJSON *scope = cJSON_GetObjectItemCaseSensitive(doc_metadata_responseJSON, "scope");
    if (cJSON_IsNull(scope)) {
        scope = NULL;
    }
    if (scope) { 
    if(!cJSON_IsString(scope) && !cJSON_IsNull(scope))
    {
    goto end; //String
    }
    }

    // doc_metadata_response->state
    cJSON *state = cJSON_GetObjectItemCaseSensitive(doc_metadata_responseJSON, "state");
    if (cJSON_IsNull(state)) {
        state = NULL;
    }
    if (state) { 
    if(!cJSON_IsString(state) && !cJSON_IsNull(state))
    {
    goto end; //String
    }
    }

    // doc_metadata_response->review_needed
    cJSON *review_needed = cJSON_GetObjectItemCaseSensitive(doc_metadata_responseJSON, "review_needed");
    if (cJSON_IsNull(review_needed)) {
        review_needed = NULL;
    }
    if (review_needed) { 
    if(!cJSON_IsBool(review_needed))
    {
    goto end; //Bool
    }
    review_needed_local_var = malloc(sizeof(int));
    if(!review_needed_local_var)
    {
        goto end;
    }
    *review_needed_local_var = review_needed->valueint;
    }

    // doc_metadata_response->created_at
    cJSON *created_at = cJSON_GetObjectItemCaseSensitive(doc_metadata_responseJSON, "created_at");
    if (cJSON_IsNull(created_at)) {
        created_at = NULL;
    }
    if (created_at) { 
    if(!cJSON_IsString(created_at) && !cJSON_IsNull(created_at))
    {
    goto end; //DateTime
    }
    }


    if (filename && !cJSON_IsNull(filename)) filename_local_str = strdup(filename->valuestring);
    if (content_hash && !cJSON_IsNull(content_hash)) content_hash_local_str = strdup(content_hash->valuestring);
    if (converter && !cJSON_IsNull(converter)) converter_local_str = strdup(converter->valuestring);
    if (converter_version && !cJSON_IsNull(converter_version)) converter_version_local_str = strdup(converter_version->valuestring);
    if (scope && !cJSON_IsNull(scope)) scope_local_str = strdup(scope->valuestring);
    if (state && !cJSON_IsNull(state)) state_local_str = strdup(state->valuestring);
    if (created_at && !cJSON_IsNull(created_at)) created_at_local_str = strdup(created_at->valuestring);

    doc_metadata_response_local_var = doc_metadata_response_create_internal (
        id_local_var,
        filename_local_str,
        content_hash_local_str,
        converter_local_str,
        converter_version_local_str,
        scope_local_str,
        state_local_str,
        review_needed_local_var,
        created_at_local_str
        );

    if (!doc_metadata_response_local_var) {
        goto end;
    }

    return doc_metadata_response_local_var;
end:
    if (id_local_var) {
        free(id_local_var);
        id_local_var = NULL;
    }
    if (filename_local_str) {
        free(filename_local_str);
        filename_local_str = NULL;
    }
    if (content_hash_local_str) {
        free(content_hash_local_str);
        content_hash_local_str = NULL;
    }
    if (converter_local_str) {
        free(converter_local_str);
        converter_local_str = NULL;
    }
    if (converter_version_local_str) {
        free(converter_version_local_str);
        converter_version_local_str = NULL;
    }
    if (scope_local_str) {
        free(scope_local_str);
        scope_local_str = NULL;
    }
    if (state_local_str) {
        free(state_local_str);
        state_local_str = NULL;
    }
    if (review_needed_local_var) {
        free(review_needed_local_var);
        review_needed_local_var = NULL;
    }
    if (created_at_local_str) {
        free(created_at_local_str);
        created_at_local_str = NULL;
    }
    return NULL;

}
