#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "docs_manifest_response_missing_inner.h"



static docs_manifest_response_missing_inner_t *docs_manifest_response_missing_inner_create_internal(
    char *doc_key,
    char *content_hash,
    char *scope
    ) {
    docs_manifest_response_missing_inner_t *docs_manifest_response_missing_inner_local_var = malloc(sizeof(docs_manifest_response_missing_inner_t));
    if (!docs_manifest_response_missing_inner_local_var) {
        return NULL;
    }
    memset(docs_manifest_response_missing_inner_local_var, 0, sizeof(docs_manifest_response_missing_inner_t));
    docs_manifest_response_missing_inner_local_var->_library_owned = 1;
    docs_manifest_response_missing_inner_local_var->doc_key = doc_key;
    docs_manifest_response_missing_inner_local_var->content_hash = content_hash;
    docs_manifest_response_missing_inner_local_var->scope = scope;
    return docs_manifest_response_missing_inner_local_var;
}

__attribute__((deprecated)) docs_manifest_response_missing_inner_t *docs_manifest_response_missing_inner_create(
    char *doc_key,
    char *content_hash,
    char *scope
    ) {
    docs_manifest_response_missing_inner_t *result = docs_manifest_response_missing_inner_create_internal (
        doc_key,
        content_hash,
        scope
        );
    if (!result) {
    }
    return result;
}

void docs_manifest_response_missing_inner_free(docs_manifest_response_missing_inner_t *docs_manifest_response_missing_inner) {
    if(NULL == docs_manifest_response_missing_inner){
        return ;
    }
    if(docs_manifest_response_missing_inner->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "docs_manifest_response_missing_inner_free");
        return ;
    }
    listEntry_t *listEntry;
    if (docs_manifest_response_missing_inner->doc_key) {
        free(docs_manifest_response_missing_inner->doc_key);
        docs_manifest_response_missing_inner->doc_key = NULL;
    }
    if (docs_manifest_response_missing_inner->content_hash) {
        free(docs_manifest_response_missing_inner->content_hash);
        docs_manifest_response_missing_inner->content_hash = NULL;
    }
    if (docs_manifest_response_missing_inner->scope) {
        free(docs_manifest_response_missing_inner->scope);
        docs_manifest_response_missing_inner->scope = NULL;
    }
    free(docs_manifest_response_missing_inner);
}

cJSON *docs_manifest_response_missing_inner_convertToJSON(docs_manifest_response_missing_inner_t *docs_manifest_response_missing_inner) {
    cJSON *item = cJSON_CreateObject();

    // docs_manifest_response_missing_inner->doc_key
    if(docs_manifest_response_missing_inner->doc_key) {
    if(cJSON_AddStringToObject(item, "doc_key", docs_manifest_response_missing_inner->doc_key) == NULL) {
    goto fail; //String
    }
    }


    // docs_manifest_response_missing_inner->content_hash
    if(docs_manifest_response_missing_inner->content_hash) {
    if(cJSON_AddStringToObject(item, "content_hash", docs_manifest_response_missing_inner->content_hash) == NULL) {
    goto fail; //String
    }
    }


    // docs_manifest_response_missing_inner->scope
    if(docs_manifest_response_missing_inner->scope) {
    if(cJSON_AddStringToObject(item, "scope", docs_manifest_response_missing_inner->scope) == NULL) {
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

docs_manifest_response_missing_inner_t *docs_manifest_response_missing_inner_parseFromJSON(cJSON *docs_manifest_response_missing_innerJSON){

    docs_manifest_response_missing_inner_t *docs_manifest_response_missing_inner_local_var = NULL;

    char *doc_key_local_str = NULL;

    char *content_hash_local_str = NULL;

    char *scope_local_str = NULL;

    // docs_manifest_response_missing_inner->doc_key
    cJSON *doc_key = cJSON_GetObjectItemCaseSensitive(docs_manifest_response_missing_innerJSON, "doc_key");
    if (cJSON_IsNull(doc_key)) {
        doc_key = NULL;
    }
    if (doc_key) { 
    if(!cJSON_IsString(doc_key) && !cJSON_IsNull(doc_key))
    {
    goto end; //String
    }
    }

    // docs_manifest_response_missing_inner->content_hash
    cJSON *content_hash = cJSON_GetObjectItemCaseSensitive(docs_manifest_response_missing_innerJSON, "content_hash");
    if (cJSON_IsNull(content_hash)) {
        content_hash = NULL;
    }
    if (content_hash) { 
    if(!cJSON_IsString(content_hash) && !cJSON_IsNull(content_hash))
    {
    goto end; //String
    }
    }

    // docs_manifest_response_missing_inner->scope
    cJSON *scope = cJSON_GetObjectItemCaseSensitive(docs_manifest_response_missing_innerJSON, "scope");
    if (cJSON_IsNull(scope)) {
        scope = NULL;
    }
    if (scope) { 
    if(!cJSON_IsString(scope) && !cJSON_IsNull(scope))
    {
    goto end; //String
    }
    }


    if (doc_key && !cJSON_IsNull(doc_key)) doc_key_local_str = strdup(doc_key->valuestring);
    if (content_hash && !cJSON_IsNull(content_hash)) content_hash_local_str = strdup(content_hash->valuestring);
    if (scope && !cJSON_IsNull(scope)) scope_local_str = strdup(scope->valuestring);

    docs_manifest_response_missing_inner_local_var = docs_manifest_response_missing_inner_create_internal (
        doc_key_local_str,
        content_hash_local_str,
        scope_local_str
        );

    if (!docs_manifest_response_missing_inner_local_var) {
        goto end;
    }

    return docs_manifest_response_missing_inner_local_var;
end:
    if (doc_key_local_str) {
        free(doc_key_local_str);
        doc_key_local_str = NULL;
    }
    if (content_hash_local_str) {
        free(content_hash_local_str);
        content_hash_local_str = NULL;
    }
    if (scope_local_str) {
        free(scope_local_str);
        scope_local_str = NULL;
    }
    return NULL;

}
