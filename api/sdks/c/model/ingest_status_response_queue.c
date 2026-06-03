#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "ingest_status_response_queue.h"



static ingest_status_response_queue_t *ingest_status_response_queue_create_internal(
    int *pending,
    int *running,
    int *done_last_24h,
    int *failed_last_24h
    ) {
    ingest_status_response_queue_t *ingest_status_response_queue_local_var = malloc(sizeof(ingest_status_response_queue_t));
    if (!ingest_status_response_queue_local_var) {
        return NULL;
    }
    memset(ingest_status_response_queue_local_var, 0, sizeof(ingest_status_response_queue_t));
    ingest_status_response_queue_local_var->_library_owned = 1;
    ingest_status_response_queue_local_var->pending = pending;
    ingest_status_response_queue_local_var->running = running;
    ingest_status_response_queue_local_var->done_last_24h = done_last_24h;
    ingest_status_response_queue_local_var->failed_last_24h = failed_last_24h;
    return ingest_status_response_queue_local_var;
}

__attribute__((deprecated)) ingest_status_response_queue_t *ingest_status_response_queue_create(
    int *pending,
    int *running,
    int *done_last_24h,
    int *failed_last_24h
    ) {
    int *pending_copy = NULL;
    if (pending) {
        pending_copy = malloc(sizeof(int));
        if (pending_copy) *pending_copy = *pending;
    }
    int *running_copy = NULL;
    if (running) {
        running_copy = malloc(sizeof(int));
        if (running_copy) *running_copy = *running;
    }
    int *done_last_24h_copy = NULL;
    if (done_last_24h) {
        done_last_24h_copy = malloc(sizeof(int));
        if (done_last_24h_copy) *done_last_24h_copy = *done_last_24h;
    }
    int *failed_last_24h_copy = NULL;
    if (failed_last_24h) {
        failed_last_24h_copy = malloc(sizeof(int));
        if (failed_last_24h_copy) *failed_last_24h_copy = *failed_last_24h;
    }
    ingest_status_response_queue_t *result = ingest_status_response_queue_create_internal (
        pending_copy,
        running_copy,
        done_last_24h_copy,
        failed_last_24h_copy
        );
    if (!result) {
        free(pending_copy);
        free(running_copy);
        free(done_last_24h_copy);
        free(failed_last_24h_copy);
    }
    return result;
}

void ingest_status_response_queue_free(ingest_status_response_queue_t *ingest_status_response_queue) {
    if(NULL == ingest_status_response_queue){
        return ;
    }
    if(ingest_status_response_queue->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "ingest_status_response_queue_free");
        return ;
    }
    listEntry_t *listEntry;
    if (ingest_status_response_queue->pending) {
        free(ingest_status_response_queue->pending);
        ingest_status_response_queue->pending = NULL;
    }
    if (ingest_status_response_queue->running) {
        free(ingest_status_response_queue->running);
        ingest_status_response_queue->running = NULL;
    }
    if (ingest_status_response_queue->done_last_24h) {
        free(ingest_status_response_queue->done_last_24h);
        ingest_status_response_queue->done_last_24h = NULL;
    }
    if (ingest_status_response_queue->failed_last_24h) {
        free(ingest_status_response_queue->failed_last_24h);
        ingest_status_response_queue->failed_last_24h = NULL;
    }
    free(ingest_status_response_queue);
}

cJSON *ingest_status_response_queue_convertToJSON(ingest_status_response_queue_t *ingest_status_response_queue) {
    cJSON *item = cJSON_CreateObject();

    // ingest_status_response_queue->pending
    if(ingest_status_response_queue->pending) {
    if(cJSON_AddNumberToObject(item, "pending", *ingest_status_response_queue->pending) == NULL) {
    goto fail; //Numeric
    }
    }


    // ingest_status_response_queue->running
    if(ingest_status_response_queue->running) {
    if(cJSON_AddNumberToObject(item, "running", *ingest_status_response_queue->running) == NULL) {
    goto fail; //Numeric
    }
    }


    // ingest_status_response_queue->done_last_24h
    if(ingest_status_response_queue->done_last_24h) {
    if(cJSON_AddNumberToObject(item, "done_last_24h", *ingest_status_response_queue->done_last_24h) == NULL) {
    goto fail; //Numeric
    }
    }


    // ingest_status_response_queue->failed_last_24h
    if(ingest_status_response_queue->failed_last_24h) {
    if(cJSON_AddNumberToObject(item, "failed_last_24h", *ingest_status_response_queue->failed_last_24h) == NULL) {
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

ingest_status_response_queue_t *ingest_status_response_queue_parseFromJSON(cJSON *ingest_status_response_queueJSON){

    ingest_status_response_queue_t *ingest_status_response_queue_local_var = NULL;

    // define the local variable for ingest_status_response_queue->pending
    int *pending_local_var = NULL;

    // define the local variable for ingest_status_response_queue->running
    int *running_local_var = NULL;

    // define the local variable for ingest_status_response_queue->done_last_24h
    int *done_last_24h_local_var = NULL;

    // define the local variable for ingest_status_response_queue->failed_last_24h
    int *failed_last_24h_local_var = NULL;

    // ingest_status_response_queue->pending
    cJSON *pending = cJSON_GetObjectItemCaseSensitive(ingest_status_response_queueJSON, "pending");
    if (cJSON_IsNull(pending)) {
        pending = NULL;
    }
    if (pending) { 
    if(!cJSON_IsNumber(pending))
    {
    goto end; //Numeric
    }
    pending_local_var = malloc(sizeof(int));
    if(!pending_local_var)
    {
        goto end;
    }
    *pending_local_var = pending->valuedouble;
    }

    // ingest_status_response_queue->running
    cJSON *running = cJSON_GetObjectItemCaseSensitive(ingest_status_response_queueJSON, "running");
    if (cJSON_IsNull(running)) {
        running = NULL;
    }
    if (running) { 
    if(!cJSON_IsNumber(running))
    {
    goto end; //Numeric
    }
    running_local_var = malloc(sizeof(int));
    if(!running_local_var)
    {
        goto end;
    }
    *running_local_var = running->valuedouble;
    }

    // ingest_status_response_queue->done_last_24h
    cJSON *done_last_24h = cJSON_GetObjectItemCaseSensitive(ingest_status_response_queueJSON, "done_last_24h");
    if (cJSON_IsNull(done_last_24h)) {
        done_last_24h = NULL;
    }
    if (done_last_24h) { 
    if(!cJSON_IsNumber(done_last_24h))
    {
    goto end; //Numeric
    }
    done_last_24h_local_var = malloc(sizeof(int));
    if(!done_last_24h_local_var)
    {
        goto end;
    }
    *done_last_24h_local_var = done_last_24h->valuedouble;
    }

    // ingest_status_response_queue->failed_last_24h
    cJSON *failed_last_24h = cJSON_GetObjectItemCaseSensitive(ingest_status_response_queueJSON, "failed_last_24h");
    if (cJSON_IsNull(failed_last_24h)) {
        failed_last_24h = NULL;
    }
    if (failed_last_24h) { 
    if(!cJSON_IsNumber(failed_last_24h))
    {
    goto end; //Numeric
    }
    failed_last_24h_local_var = malloc(sizeof(int));
    if(!failed_last_24h_local_var)
    {
        goto end;
    }
    *failed_last_24h_local_var = failed_last_24h->valuedouble;
    }



    ingest_status_response_queue_local_var = ingest_status_response_queue_create_internal (
        pending_local_var,
        running_local_var,
        done_last_24h_local_var,
        failed_last_24h_local_var
        );

    if (!ingest_status_response_queue_local_var) {
        goto end;
    }

    return ingest_status_response_queue_local_var;
end:
    if (pending_local_var) {
        free(pending_local_var);
        pending_local_var = NULL;
    }
    if (running_local_var) {
        free(running_local_var);
        running_local_var = NULL;
    }
    if (done_last_24h_local_var) {
        free(done_last_24h_local_var);
        done_last_24h_local_var = NULL;
    }
    if (failed_last_24h_local_var) {
        free(failed_last_24h_local_var);
        failed_last_24h_local_var = NULL;
    }
    return NULL;

}
