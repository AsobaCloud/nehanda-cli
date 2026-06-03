#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "review_queue_response.h"



static review_queue_response_t *review_queue_response_create_internal(
    list_t *docs,
    long *next_cursor
    ) {
    review_queue_response_t *review_queue_response_local_var = malloc(sizeof(review_queue_response_t));
    if (!review_queue_response_local_var) {
        return NULL;
    }
    memset(review_queue_response_local_var, 0, sizeof(review_queue_response_t));
    review_queue_response_local_var->_library_owned = 1;
    review_queue_response_local_var->docs = docs;
    review_queue_response_local_var->next_cursor = next_cursor;
    return review_queue_response_local_var;
}

__attribute__((deprecated)) review_queue_response_t *review_queue_response_create(
    list_t *docs,
    long *next_cursor
    ) {
    long *next_cursor_copy = NULL;
    if (next_cursor) {
        next_cursor_copy = malloc(sizeof(long));
        if (next_cursor_copy) *next_cursor_copy = *next_cursor;
    }
    review_queue_response_t *result = review_queue_response_create_internal (
        docs,
        next_cursor_copy
        );
    if (!result) {
        free(next_cursor_copy);
    }
    return result;
}

void review_queue_response_free(review_queue_response_t *review_queue_response) {
    if(NULL == review_queue_response){
        return ;
    }
    if(review_queue_response->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "review_queue_response_free");
        return ;
    }
    listEntry_t *listEntry;
    if (review_queue_response->docs) {
        list_ForEach(listEntry, review_queue_response->docs) {
            doc_metadata_response_free(listEntry->data);
        }
        list_freeList(review_queue_response->docs);
        review_queue_response->docs = NULL;
    }
    if (review_queue_response->next_cursor) {
        free(review_queue_response->next_cursor);
        review_queue_response->next_cursor = NULL;
    }
    free(review_queue_response);
}

cJSON *review_queue_response_convertToJSON(review_queue_response_t *review_queue_response) {
    cJSON *item = cJSON_CreateObject();

    // review_queue_response->docs
    if(review_queue_response->docs) {
    cJSON *docs = cJSON_AddArrayToObject(item, "docs");
    if(docs == NULL) {
    goto fail; //nonprimitive container
    }

    listEntry_t *docsListEntry;
    if (review_queue_response->docs) {
    list_ForEach(docsListEntry, review_queue_response->docs) {
    cJSON *itemLocal = doc_metadata_response_convertToJSON(docsListEntry->data);
    if(itemLocal == NULL) {
    goto fail;
    }
    cJSON_AddItemToArray(docs, itemLocal);
    }
    }
    }


    // review_queue_response->next_cursor
    if(review_queue_response->next_cursor) {
    if(cJSON_AddNumberToObject(item, "next_cursor", *review_queue_response->next_cursor) == NULL) {
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

review_queue_response_t *review_queue_response_parseFromJSON(cJSON *review_queue_responseJSON){

    review_queue_response_t *review_queue_response_local_var = NULL;

    // define the local list for review_queue_response->docs
    list_t *docsList = NULL;

    // define the local variable for review_queue_response->next_cursor
    long *next_cursor_local_var = NULL;

    // review_queue_response->docs
    cJSON *docs = cJSON_GetObjectItemCaseSensitive(review_queue_responseJSON, "docs");
    if (cJSON_IsNull(docs)) {
        docs = NULL;
    }
    if (docs) { 
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
        doc_metadata_response_t *docsItem = doc_metadata_response_parseFromJSON(docs_local_nonprimitive);

        list_addElement(docsList, docsItem);
    }
    }

    // review_queue_response->next_cursor
    cJSON *next_cursor = cJSON_GetObjectItemCaseSensitive(review_queue_responseJSON, "next_cursor");
    if (cJSON_IsNull(next_cursor)) {
        next_cursor = NULL;
    }
    if (next_cursor) { 
    if(!cJSON_IsNumber(next_cursor))
    {
    goto end; //Numeric
    }
    next_cursor_local_var = malloc(sizeof(long));
    if(!next_cursor_local_var)
    {
        goto end;
    }
    *next_cursor_local_var = next_cursor->valuedouble;
    }



    review_queue_response_local_var = review_queue_response_create_internal (
        docs ? docsList : NULL,
        next_cursor_local_var
        );

    if (!review_queue_response_local_var) {
        goto end;
    }

    return review_queue_response_local_var;
end:
    if (docsList) {
        listEntry_t *listEntry = NULL;
        list_ForEach(listEntry, docsList) {
            doc_metadata_response_free(listEntry->data);
            listEntry->data = NULL;
        }
        list_freeList(docsList);
        docsList = NULL;
    }
    if (next_cursor_local_var) {
        free(next_cursor_local_var);
        next_cursor_local_var = NULL;
    }
    return NULL;

}
