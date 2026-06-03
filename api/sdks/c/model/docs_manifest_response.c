#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "docs_manifest_response.h"



static docs_manifest_response_t *docs_manifest_response_create_internal(
    list_t *missing,
    int *total,
    int *present,
    int *missing_count
    ) {
    docs_manifest_response_t *docs_manifest_response_local_var = malloc(sizeof(docs_manifest_response_t));
    if (!docs_manifest_response_local_var) {
        return NULL;
    }
    memset(docs_manifest_response_local_var, 0, sizeof(docs_manifest_response_t));
    docs_manifest_response_local_var->_library_owned = 1;
    docs_manifest_response_local_var->missing = missing;
    docs_manifest_response_local_var->total = total;
    docs_manifest_response_local_var->present = present;
    docs_manifest_response_local_var->missing_count = missing_count;
    return docs_manifest_response_local_var;
}

__attribute__((deprecated)) docs_manifest_response_t *docs_manifest_response_create(
    list_t *missing,
    int *total,
    int *present,
    int *missing_count
    ) {
    int *total_copy = NULL;
    if (total) {
        total_copy = malloc(sizeof(int));
        if (total_copy) *total_copy = *total;
    }
    int *present_copy = NULL;
    if (present) {
        present_copy = malloc(sizeof(int));
        if (present_copy) *present_copy = *present;
    }
    int *missing_count_copy = NULL;
    if (missing_count) {
        missing_count_copy = malloc(sizeof(int));
        if (missing_count_copy) *missing_count_copy = *missing_count;
    }
    docs_manifest_response_t *result = docs_manifest_response_create_internal (
        missing,
        total_copy,
        present_copy,
        missing_count_copy
        );
    if (!result) {
        free(total_copy);
        free(present_copy);
        free(missing_count_copy);
    }
    return result;
}

void docs_manifest_response_free(docs_manifest_response_t *docs_manifest_response) {
    if(NULL == docs_manifest_response){
        return ;
    }
    if(docs_manifest_response->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "docs_manifest_response_free");
        return ;
    }
    listEntry_t *listEntry;
    if (docs_manifest_response->missing) {
        list_ForEach(listEntry, docs_manifest_response->missing) {
            docs_manifest_response_missing_inner_free(listEntry->data);
        }
        list_freeList(docs_manifest_response->missing);
        docs_manifest_response->missing = NULL;
    }
    if (docs_manifest_response->total) {
        free(docs_manifest_response->total);
        docs_manifest_response->total = NULL;
    }
    if (docs_manifest_response->present) {
        free(docs_manifest_response->present);
        docs_manifest_response->present = NULL;
    }
    if (docs_manifest_response->missing_count) {
        free(docs_manifest_response->missing_count);
        docs_manifest_response->missing_count = NULL;
    }
    free(docs_manifest_response);
}

cJSON *docs_manifest_response_convertToJSON(docs_manifest_response_t *docs_manifest_response) {
    cJSON *item = cJSON_CreateObject();

    // docs_manifest_response->missing
    if (!docs_manifest_response->missing) {
        goto fail;
    }
    cJSON *missing = cJSON_AddArrayToObject(item, "missing");
    if(missing == NULL) {
    goto fail; //nonprimitive container
    }

    listEntry_t *missingListEntry;
    if (docs_manifest_response->missing) {
    list_ForEach(missingListEntry, docs_manifest_response->missing) {
    cJSON *itemLocal = docs_manifest_response_missing_inner_convertToJSON(missingListEntry->data);
    if(itemLocal == NULL) {
    goto fail;
    }
    cJSON_AddItemToArray(missing, itemLocal);
    }
    }


    // docs_manifest_response->total
    if (!docs_manifest_response->total) {
        goto fail;
    }
    if(cJSON_AddNumberToObject(item, "total", *docs_manifest_response->total) == NULL) {
    goto fail; //Numeric
    }


    // docs_manifest_response->present
    if (!docs_manifest_response->present) {
        goto fail;
    }
    if(cJSON_AddNumberToObject(item, "present", *docs_manifest_response->present) == NULL) {
    goto fail; //Numeric
    }


    // docs_manifest_response->missing_count
    if (!docs_manifest_response->missing_count) {
        goto fail;
    }
    if(cJSON_AddNumberToObject(item, "missing_count", *docs_manifest_response->missing_count) == NULL) {
    goto fail; //Numeric
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

docs_manifest_response_t *docs_manifest_response_parseFromJSON(cJSON *docs_manifest_responseJSON){

    docs_manifest_response_t *docs_manifest_response_local_var = NULL;

    // define the local list for docs_manifest_response->missing
    list_t *missingList = NULL;

    // define the local variable for docs_manifest_response->total
    int *total_local_var = NULL;

    // define the local variable for docs_manifest_response->present
    int *present_local_var = NULL;

    // define the local variable for docs_manifest_response->missing_count
    int *missing_count_local_var = NULL;

    // docs_manifest_response->missing
    cJSON *missing = cJSON_GetObjectItemCaseSensitive(docs_manifest_responseJSON, "missing");
    if (cJSON_IsNull(missing)) {
        missing = NULL;
    }
    if (!missing) {
        goto end;
    }

    
    cJSON *missing_local_nonprimitive = NULL;
    if(!cJSON_IsArray(missing)){
        goto end; //nonprimitive container
    }

    missingList = list_createList();

    cJSON_ArrayForEach(missing_local_nonprimitive,missing )
    {
        if(!cJSON_IsObject(missing_local_nonprimitive)){
            goto end;
        }
        docs_manifest_response_missing_inner_t *missingItem = docs_manifest_response_missing_inner_parseFromJSON(missing_local_nonprimitive);

        list_addElement(missingList, missingItem);
    }

    // docs_manifest_response->total
    cJSON *total = cJSON_GetObjectItemCaseSensitive(docs_manifest_responseJSON, "total");
    if (cJSON_IsNull(total)) {
        total = NULL;
    }
    if (!total) {
        goto end;
    }

    
    if(!cJSON_IsNumber(total))
    {
    goto end; //Numeric
    }
    total_local_var = malloc(sizeof(int));
    if(!total_local_var)
    {
        goto end;
    }
    *total_local_var = total->valuedouble;

    // docs_manifest_response->present
    cJSON *present = cJSON_GetObjectItemCaseSensitive(docs_manifest_responseJSON, "present");
    if (cJSON_IsNull(present)) {
        present = NULL;
    }
    if (!present) {
        goto end;
    }

    
    if(!cJSON_IsNumber(present))
    {
    goto end; //Numeric
    }
    present_local_var = malloc(sizeof(int));
    if(!present_local_var)
    {
        goto end;
    }
    *present_local_var = present->valuedouble;

    // docs_manifest_response->missing_count
    cJSON *missing_count = cJSON_GetObjectItemCaseSensitive(docs_manifest_responseJSON, "missing_count");
    if (cJSON_IsNull(missing_count)) {
        missing_count = NULL;
    }
    if (!missing_count) {
        goto end;
    }

    
    if(!cJSON_IsNumber(missing_count))
    {
    goto end; //Numeric
    }
    missing_count_local_var = malloc(sizeof(int));
    if(!missing_count_local_var)
    {
        goto end;
    }
    *missing_count_local_var = missing_count->valuedouble;



    docs_manifest_response_local_var = docs_manifest_response_create_internal (
        missingList,
        total_local_var,
        present_local_var,
        missing_count_local_var
        );

    if (!docs_manifest_response_local_var) {
        goto end;
    }

    return docs_manifest_response_local_var;
end:
    if (missingList) {
        listEntry_t *listEntry = NULL;
        list_ForEach(listEntry, missingList) {
            docs_manifest_response_missing_inner_free(listEntry->data);
            listEntry->data = NULL;
        }
        list_freeList(missingList);
        missingList = NULL;
    }
    if (total_local_var) {
        free(total_local_var);
        total_local_var = NULL;
    }
    if (present_local_var) {
        free(present_local_var);
        present_local_var = NULL;
    }
    if (missing_count_local_var) {
        free(missing_count_local_var);
        missing_count_local_var = NULL;
    }
    return NULL;

}
