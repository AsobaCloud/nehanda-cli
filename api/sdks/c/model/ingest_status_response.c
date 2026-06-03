#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "ingest_status_response.h"



static ingest_status_response_t *ingest_status_response_create_internal(
    char *status,
    ingest_status_response_queue_t *queue,
    ingest_status_response_workers_t *workers,
    list_t *recent
    ) {
    ingest_status_response_t *ingest_status_response_local_var = malloc(sizeof(ingest_status_response_t));
    if (!ingest_status_response_local_var) {
        return NULL;
    }
    memset(ingest_status_response_local_var, 0, sizeof(ingest_status_response_t));
    ingest_status_response_local_var->_library_owned = 1;
    ingest_status_response_local_var->status = status;
    ingest_status_response_local_var->queue = queue;
    ingest_status_response_local_var->workers = workers;
    ingest_status_response_local_var->recent = recent;
    return ingest_status_response_local_var;
}

__attribute__((deprecated)) ingest_status_response_t *ingest_status_response_create(
    char *status,
    ingest_status_response_queue_t *queue,
    ingest_status_response_workers_t *workers,
    list_t *recent
    ) {
    ingest_status_response_t *result = ingest_status_response_create_internal (
        status,
        queue,
        workers,
        recent
        );
    if (!result) {
    }
    return result;
}

void ingest_status_response_free(ingest_status_response_t *ingest_status_response) {
    if(NULL == ingest_status_response){
        return ;
    }
    if(ingest_status_response->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "ingest_status_response_free");
        return ;
    }
    listEntry_t *listEntry;
    if (ingest_status_response->status) {
        free(ingest_status_response->status);
        ingest_status_response->status = NULL;
    }
    if (ingest_status_response->queue) {
        ingest_status_response_queue_free(ingest_status_response->queue);
        ingest_status_response->queue = NULL;
    }
    if (ingest_status_response->workers) {
        ingest_status_response_workers_free(ingest_status_response->workers);
        ingest_status_response->workers = NULL;
    }
    if (ingest_status_response->recent) {
        list_ForEach(listEntry, ingest_status_response->recent) {
            ingest_status_response_recent_inner_free(listEntry->data);
        }
        list_freeList(ingest_status_response->recent);
        ingest_status_response->recent = NULL;
    }
    free(ingest_status_response);
}

cJSON *ingest_status_response_convertToJSON(ingest_status_response_t *ingest_status_response) {
    cJSON *item = cJSON_CreateObject();

    // ingest_status_response->status
    if(ingest_status_response->status) {
    if(cJSON_AddStringToObject(item, "status", ingest_status_response->status) == NULL) {
    goto fail; //String
    }
    }


    // ingest_status_response->queue
    if(ingest_status_response->queue) {
    cJSON *queue_local_JSON = ingest_status_response_queue_convertToJSON(ingest_status_response->queue);
    if(queue_local_JSON == NULL) {
    goto fail; //model
    }
    cJSON_AddItemToObject(item, "queue", queue_local_JSON);
    if(item->child == NULL) {
    goto fail;
    }
    }


    // ingest_status_response->workers
    if(ingest_status_response->workers) {
    cJSON *workers_local_JSON = ingest_status_response_workers_convertToJSON(ingest_status_response->workers);
    if(workers_local_JSON == NULL) {
    goto fail; //model
    }
    cJSON_AddItemToObject(item, "workers", workers_local_JSON);
    if(item->child == NULL) {
    goto fail;
    }
    }


    // ingest_status_response->recent
    if(ingest_status_response->recent) {
    cJSON *recent = cJSON_AddArrayToObject(item, "recent");
    if(recent == NULL) {
    goto fail; //nonprimitive container
    }

    listEntry_t *recentListEntry;
    if (ingest_status_response->recent) {
    list_ForEach(recentListEntry, ingest_status_response->recent) {
    cJSON *itemLocal = ingest_status_response_recent_inner_convertToJSON(recentListEntry->data);
    if(itemLocal == NULL) {
    goto fail;
    }
    cJSON_AddItemToArray(recent, itemLocal);
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

ingest_status_response_t *ingest_status_response_parseFromJSON(cJSON *ingest_status_responseJSON){

    ingest_status_response_t *ingest_status_response_local_var = NULL;

    char *status_local_str = NULL;

    // define the local variable for ingest_status_response->queue
    ingest_status_response_queue_t *queue_local_nonprim = NULL;

    // define the local variable for ingest_status_response->workers
    ingest_status_response_workers_t *workers_local_nonprim = NULL;

    // define the local list for ingest_status_response->recent
    list_t *recentList = NULL;

    // ingest_status_response->status
    cJSON *status = cJSON_GetObjectItemCaseSensitive(ingest_status_responseJSON, "status");
    if (cJSON_IsNull(status)) {
        status = NULL;
    }
    if (status) { 
    if(!cJSON_IsString(status) && !cJSON_IsNull(status))
    {
    goto end; //String
    }
    }

    // ingest_status_response->queue
    cJSON *queue = cJSON_GetObjectItemCaseSensitive(ingest_status_responseJSON, "queue");
    if (cJSON_IsNull(queue)) {
        queue = NULL;
    }
    if (queue) { 
    queue_local_nonprim = ingest_status_response_queue_parseFromJSON(queue); //nonprimitive
    }

    // ingest_status_response->workers
    cJSON *workers = cJSON_GetObjectItemCaseSensitive(ingest_status_responseJSON, "workers");
    if (cJSON_IsNull(workers)) {
        workers = NULL;
    }
    if (workers) { 
    workers_local_nonprim = ingest_status_response_workers_parseFromJSON(workers); //nonprimitive
    }

    // ingest_status_response->recent
    cJSON *recent = cJSON_GetObjectItemCaseSensitive(ingest_status_responseJSON, "recent");
    if (cJSON_IsNull(recent)) {
        recent = NULL;
    }
    if (recent) { 
    cJSON *recent_local_nonprimitive = NULL;
    if(!cJSON_IsArray(recent)){
        goto end; //nonprimitive container
    }

    recentList = list_createList();

    cJSON_ArrayForEach(recent_local_nonprimitive,recent )
    {
        if(!cJSON_IsObject(recent_local_nonprimitive)){
            goto end;
        }
        ingest_status_response_recent_inner_t *recentItem = ingest_status_response_recent_inner_parseFromJSON(recent_local_nonprimitive);

        list_addElement(recentList, recentItem);
    }
    }


    if (status && !cJSON_IsNull(status)) status_local_str = strdup(status->valuestring);

    ingest_status_response_local_var = ingest_status_response_create_internal (
        status_local_str,
        queue ? queue_local_nonprim : NULL,
        workers ? workers_local_nonprim : NULL,
        recent ? recentList : NULL
        );

    if (!ingest_status_response_local_var) {
        goto end;
    }

    return ingest_status_response_local_var;
end:
    if (status_local_str) {
        free(status_local_str);
        status_local_str = NULL;
    }
    if (queue_local_nonprim) {
        ingest_status_response_queue_free(queue_local_nonprim);
        queue_local_nonprim = NULL;
    }
    if (workers_local_nonprim) {
        ingest_status_response_workers_free(workers_local_nonprim);
        workers_local_nonprim = NULL;
    }
    if (recentList) {
        listEntry_t *listEntry = NULL;
        list_ForEach(listEntry, recentList) {
            ingest_status_response_recent_inner_free(listEntry->data);
            listEntry->data = NULL;
        }
        list_freeList(recentList);
        recentList = NULL;
    }
    return NULL;

}
