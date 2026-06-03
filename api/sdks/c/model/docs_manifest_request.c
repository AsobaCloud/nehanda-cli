#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "docs_manifest_request.h"



static docs_manifest_request_t *docs_manifest_request_create_internal(
    char *scope,
    list_t *docs
    ) {
    docs_manifest_request_t *docs_manifest_request_local_var = malloc(sizeof(docs_manifest_request_t));
    if (!docs_manifest_request_local_var) {
        return NULL;
    }
    memset(docs_manifest_request_local_var, 0, sizeof(docs_manifest_request_t));
    docs_manifest_request_local_var->_library_owned = 1;
    docs_manifest_request_local_var->scope = scope;
    docs_manifest_request_local_var->docs = docs;
    return docs_manifest_request_local_var;
}

__attribute__((deprecated)) docs_manifest_request_t *docs_manifest_request_create(
    char *scope,
    list_t *docs
    ) {
    docs_manifest_request_t *result = docs_manifest_request_create_internal (
        scope,
        docs
        );
    if (!result) {
    }
    return result;
}

void docs_manifest_request_free(docs_manifest_request_t *docs_manifest_request) {
    if(NULL == docs_manifest_request){
        return ;
    }
    if(docs_manifest_request->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "docs_manifest_request_free");
        return ;
    }
    listEntry_t *listEntry;
    if (docs_manifest_request->scope) {
        free(docs_manifest_request->scope);
        docs_manifest_request->scope = NULL;
    }
    if (docs_manifest_request->docs) {
        list_ForEach(listEntry, docs_manifest_request->docs) {
            docs_manifest_request_docs_inner_free(listEntry->data);
        }
        list_freeList(docs_manifest_request->docs);
        docs_manifest_request->docs = NULL;
    }
    free(docs_manifest_request);
}

cJSON *docs_manifest_request_convertToJSON(docs_manifest_request_t *docs_manifest_request) {
    cJSON *item = cJSON_CreateObject();

    // docs_manifest_request->scope
    if(docs_manifest_request->scope) {
    if(cJSON_AddStringToObject(item, "scope", docs_manifest_request->scope) == NULL) {
    goto fail; //String
    }
    }


    // docs_manifest_request->docs
    if (!docs_manifest_request->docs) {
        goto fail;
    }
    cJSON *docs = cJSON_AddArrayToObject(item, "docs");
    if(docs == NULL) {
    goto fail; //nonprimitive container
    }

    listEntry_t *docsListEntry;
    if (docs_manifest_request->docs) {
    list_ForEach(docsListEntry, docs_manifest_request->docs) {
    cJSON *itemLocal = docs_manifest_request_docs_inner_convertToJSON(docsListEntry->data);
    if(itemLocal == NULL) {
    goto fail;
    }
    cJSON_AddItemToArray(docs, itemLocal);
    }
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

docs_manifest_request_t *docs_manifest_request_parseFromJSON(cJSON *docs_manifest_requestJSON){

    docs_manifest_request_t *docs_manifest_request_local_var = NULL;

    char *scope_local_str = NULL;

    // define the local list for docs_manifest_request->docs
    list_t *docsList = NULL;

    // docs_manifest_request->scope
    cJSON *scope = cJSON_GetObjectItemCaseSensitive(docs_manifest_requestJSON, "scope");
    if (cJSON_IsNull(scope)) {
        scope = NULL;
    }
    if (scope) { 
    if(!cJSON_IsString(scope) && !cJSON_IsNull(scope))
    {
    goto end; //String
    }
    }

    // docs_manifest_request->docs
    cJSON *docs = cJSON_GetObjectItemCaseSensitive(docs_manifest_requestJSON, "docs");
    if (cJSON_IsNull(docs)) {
        docs = NULL;
    }
    if (!docs) {
        goto end;
    }

    
    cJSON *docs_local_nonprimitive = NULL;
    if(!cJSON_IsArray(docs)){
        goto end; //nonprimitive container
    }

    docsList = list_createList();

    cJSON_ArrayForEach(docs_local_nonprimitive,docs )
    {
        if(!cJSON_IsObject(docs_local_nonprimitive)){
            goto end;
        }
        docs_manifest_request_docs_inner_t *docsItem = docs_manifest_request_docs_inner_parseFromJSON(docs_local_nonprimitive);

        list_addElement(docsList, docsItem);
    }


    if (scope && !cJSON_IsNull(scope)) scope_local_str = strdup(scope->valuestring);

    docs_manifest_request_local_var = docs_manifest_request_create_internal (
        scope_local_str,
        docsList
        );

    if (!docs_manifest_request_local_var) {
        goto end;
    }

    return docs_manifest_request_local_var;
end:
    if (scope_local_str) {
        free(scope_local_str);
        scope_local_str = NULL;
    }
    if (docsList) {
        listEntry_t *listEntry = NULL;
        list_ForEach(listEntry, docsList) {
            docs_manifest_request_docs_inner_free(listEntry->data);
            listEntry->data = NULL;
        }
        list_freeList(docsList);
        docsList = NULL;
    }
    return NULL;

}
