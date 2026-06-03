#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "workers_response.h"



static workers_response_t *workers_response_create_internal(
    char *status,
    int *configured,
    list_t *slots,
    list_t *threads,
    list_t *background
    ) {
    workers_response_t *workers_response_local_var = malloc(sizeof(workers_response_t));
    if (!workers_response_local_var) {
        return NULL;
    }
    memset(workers_response_local_var, 0, sizeof(workers_response_t));
    workers_response_local_var->_library_owned = 1;
    workers_response_local_var->status = status;
    workers_response_local_var->configured = configured;
    workers_response_local_var->slots = slots;
    workers_response_local_var->threads = threads;
    workers_response_local_var->background = background;
    return workers_response_local_var;
}

__attribute__((deprecated)) workers_response_t *workers_response_create(
    char *status,
    int *configured,
    list_t *slots,
    list_t *threads,
    list_t *background
    ) {
    int *configured_copy = NULL;
    if (configured) {
        configured_copy = malloc(sizeof(int));
        if (configured_copy) *configured_copy = *configured;
    }
    workers_response_t *result = workers_response_create_internal (
        status,
        configured_copy,
        slots,
        threads,
        background
        );
    if (!result) {
        free(configured_copy);
    }
    return result;
}

void workers_response_free(workers_response_t *workers_response) {
    if(NULL == workers_response){
        return ;
    }
    if(workers_response->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "workers_response_free");
        return ;
    }
    listEntry_t *listEntry;
    if (workers_response->status) {
        free(workers_response->status);
        workers_response->status = NULL;
    }
    if (workers_response->configured) {
        free(workers_response->configured);
        workers_response->configured = NULL;
    }
    if (workers_response->slots) {
        list_ForEach(listEntry, workers_response->slots) {
            object_free(listEntry->data);
        }
        list_freeList(workers_response->slots);
        workers_response->slots = NULL;
    }
    if (workers_response->threads) {
        list_ForEach(listEntry, workers_response->threads) {
            object_free(listEntry->data);
        }
        list_freeList(workers_response->threads);
        workers_response->threads = NULL;
    }
    if (workers_response->background) {
        list_ForEach(listEntry, workers_response->background) {
            object_free(listEntry->data);
        }
        list_freeList(workers_response->background);
        workers_response->background = NULL;
    }
    free(workers_response);
}

cJSON *workers_response_convertToJSON(workers_response_t *workers_response) {
    cJSON *item = cJSON_CreateObject();

    // workers_response->status
    if(workers_response->status) {
    if(cJSON_AddStringToObject(item, "status", workers_response->status) == NULL) {
    goto fail; //String
    }
    }


    // workers_response->configured
    if(workers_response->configured) {
    if(cJSON_AddNumberToObject(item, "configured", *workers_response->configured) == NULL) {
    goto fail; //Numeric
    }
    }


    // workers_response->slots
    if(workers_response->slots) {
    cJSON *slots = cJSON_AddArrayToObject(item, "slots");
    if(slots == NULL) {
    goto fail; //nonprimitive container
    }

    listEntry_t *slotsListEntry;
    if (workers_response->slots) {
    list_ForEach(slotsListEntry, workers_response->slots) {
    cJSON *itemLocal = object_convertToJSON(slotsListEntry->data);
    if(itemLocal == NULL) {
    goto fail;
    }
    cJSON_AddItemToArray(slots, itemLocal);
    }
    }
    }


    // workers_response->threads
    if(workers_response->threads) {
    cJSON *threads = cJSON_AddArrayToObject(item, "threads");
    if(threads == NULL) {
    goto fail; //nonprimitive container
    }

    listEntry_t *threadsListEntry;
    if (workers_response->threads) {
    list_ForEach(threadsListEntry, workers_response->threads) {
    cJSON *itemLocal = object_convertToJSON(threadsListEntry->data);
    if(itemLocal == NULL) {
    goto fail;
    }
    cJSON_AddItemToArray(threads, itemLocal);
    }
    }
    }


    // workers_response->background
    if(workers_response->background) {
    cJSON *background = cJSON_AddArrayToObject(item, "background");
    if(background == NULL) {
    goto fail; //nonprimitive container
    }

    listEntry_t *backgroundListEntry;
    if (workers_response->background) {
    list_ForEach(backgroundListEntry, workers_response->background) {
    cJSON *itemLocal = object_convertToJSON(backgroundListEntry->data);
    if(itemLocal == NULL) {
    goto fail;
    }
    cJSON_AddItemToArray(background, itemLocal);
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

workers_response_t *workers_response_parseFromJSON(cJSON *workers_responseJSON){

    workers_response_t *workers_response_local_var = NULL;

    char *status_local_str = NULL;

    // define the local variable for workers_response->configured
    int *configured_local_var = NULL;

    // define the local list for workers_response->slots
    list_t *slotsList = NULL;

    // define the local list for workers_response->threads
    list_t *threadsList = NULL;

    // define the local list for workers_response->background
    list_t *backgroundList = NULL;

    // workers_response->status
    cJSON *status = cJSON_GetObjectItemCaseSensitive(workers_responseJSON, "status");
    if (cJSON_IsNull(status)) {
        status = NULL;
    }
    if (status) { 
    if(!cJSON_IsString(status) && !cJSON_IsNull(status))
    {
    goto end; //String
    }
    }

    // workers_response->configured
    cJSON *configured = cJSON_GetObjectItemCaseSensitive(workers_responseJSON, "configured");
    if (cJSON_IsNull(configured)) {
        configured = NULL;
    }
    if (configured) { 
    if(!cJSON_IsNumber(configured))
    {
    goto end; //Numeric
    }
    configured_local_var = malloc(sizeof(int));
    if(!configured_local_var)
    {
        goto end;
    }
    *configured_local_var = configured->valuedouble;
    }

    // workers_response->slots
    cJSON *slots = cJSON_GetObjectItemCaseSensitive(workers_responseJSON, "slots");
    if (cJSON_IsNull(slots)) {
        slots = NULL;
    }
    if (slots) { 
    cJSON *slots_local_nonprimitive = NULL;
    if(!cJSON_IsArray(slots)){
        goto end; //nonprimitive container
    }

    slotsList = list_createList();

    cJSON_ArrayForEach(slots_local_nonprimitive,slots )
    {
        if(!cJSON_IsObject(slots_local_nonprimitive)){
            goto end;
        }
        object_t *slotsItem = object_parseFromJSON(slots_local_nonprimitive);

        list_addElement(slotsList, slotsItem);
    }
    }

    // workers_response->threads
    cJSON *threads = cJSON_GetObjectItemCaseSensitive(workers_responseJSON, "threads");
    if (cJSON_IsNull(threads)) {
        threads = NULL;
    }
    if (threads) { 
    cJSON *threads_local_nonprimitive = NULL;
    if(!cJSON_IsArray(threads)){
        goto end; //nonprimitive container
    }

    threadsList = list_createList();

    cJSON_ArrayForEach(threads_local_nonprimitive,threads )
    {
        if(!cJSON_IsObject(threads_local_nonprimitive)){
            goto end;
        }
        object_t *threadsItem = object_parseFromJSON(threads_local_nonprimitive);

        list_addElement(threadsList, threadsItem);
    }
    }

    // workers_response->background
    cJSON *background = cJSON_GetObjectItemCaseSensitive(workers_responseJSON, "background");
    if (cJSON_IsNull(background)) {
        background = NULL;
    }
    if (background) { 
    cJSON *background_local_nonprimitive = NULL;
    if(!cJSON_IsArray(background)){
        goto end; //nonprimitive container
    }

    backgroundList = list_createList();

    cJSON_ArrayForEach(background_local_nonprimitive,background )
    {
        if(!cJSON_IsObject(background_local_nonprimitive)){
            goto end;
        }
        object_t *backgroundItem = object_parseFromJSON(background_local_nonprimitive);

        list_addElement(backgroundList, backgroundItem);
    }
    }


    if (status && !cJSON_IsNull(status)) status_local_str = strdup(status->valuestring);

    workers_response_local_var = workers_response_create_internal (
        status_local_str,
        configured_local_var,
        slots ? slotsList : NULL,
        threads ? threadsList : NULL,
        background ? backgroundList : NULL
        );

    if (!workers_response_local_var) {
        goto end;
    }

    return workers_response_local_var;
end:
    if (status_local_str) {
        free(status_local_str);
        status_local_str = NULL;
    }
    if (configured_local_var) {
        free(configured_local_var);
        configured_local_var = NULL;
    }
    if (slotsList) {
        listEntry_t *listEntry = NULL;
        list_ForEach(listEntry, slotsList) {
            object_free(listEntry->data);
            listEntry->data = NULL;
        }
        list_freeList(slotsList);
        slotsList = NULL;
    }
    if (threadsList) {
        listEntry_t *listEntry = NULL;
        list_ForEach(listEntry, threadsList) {
            object_free(listEntry->data);
            listEntry->data = NULL;
        }
        list_freeList(threadsList);
        threadsList = NULL;
    }
    if (backgroundList) {
        listEntry_t *listEntry = NULL;
        list_ForEach(listEntry, backgroundList) {
            object_free(listEntry->data);
            listEntry->data = NULL;
        }
        list_freeList(backgroundList);
        backgroundList = NULL;
    }
    return NULL;

}
