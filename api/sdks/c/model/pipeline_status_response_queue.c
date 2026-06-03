#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "pipeline_status_response_queue.h"



static pipeline_status_response_queue_t *pipeline_status_response_queue_create_internal(
    int *pending,
    int *running,
    int *done,
    int *failed,
    int *total
    ) {
    pipeline_status_response_queue_t *pipeline_status_response_queue_local_var = malloc(sizeof(pipeline_status_response_queue_t));
    if (!pipeline_status_response_queue_local_var) {
        return NULL;
    }
    memset(pipeline_status_response_queue_local_var, 0, sizeof(pipeline_status_response_queue_t));
    pipeline_status_response_queue_local_var->_library_owned = 1;
    pipeline_status_response_queue_local_var->pending = pending;
    pipeline_status_response_queue_local_var->running = running;
    pipeline_status_response_queue_local_var->done = done;
    pipeline_status_response_queue_local_var->failed = failed;
    pipeline_status_response_queue_local_var->total = total;
    return pipeline_status_response_queue_local_var;
}

__attribute__((deprecated)) pipeline_status_response_queue_t *pipeline_status_response_queue_create(
    int *pending,
    int *running,
    int *done,
    int *failed,
    int *total
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
    int *done_copy = NULL;
    if (done) {
        done_copy = malloc(sizeof(int));
        if (done_copy) *done_copy = *done;
    }
    int *failed_copy = NULL;
    if (failed) {
        failed_copy = malloc(sizeof(int));
        if (failed_copy) *failed_copy = *failed;
    }
    int *total_copy = NULL;
    if (total) {
        total_copy = malloc(sizeof(int));
        if (total_copy) *total_copy = *total;
    }
    pipeline_status_response_queue_t *result = pipeline_status_response_queue_create_internal (
        pending_copy,
        running_copy,
        done_copy,
        failed_copy,
        total_copy
        );
    if (!result) {
        free(pending_copy);
        free(running_copy);
        free(done_copy);
        free(failed_copy);
        free(total_copy);
    }
    return result;
}

void pipeline_status_response_queue_free(pipeline_status_response_queue_t *pipeline_status_response_queue) {
    if(NULL == pipeline_status_response_queue){
        return ;
    }
    if(pipeline_status_response_queue->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "pipeline_status_response_queue_free");
        return ;
    }
    listEntry_t *listEntry;
    if (pipeline_status_response_queue->pending) {
        free(pipeline_status_response_queue->pending);
        pipeline_status_response_queue->pending = NULL;
    }
    if (pipeline_status_response_queue->running) {
        free(pipeline_status_response_queue->running);
        pipeline_status_response_queue->running = NULL;
    }
    if (pipeline_status_response_queue->done) {
        free(pipeline_status_response_queue->done);
        pipeline_status_response_queue->done = NULL;
    }
    if (pipeline_status_response_queue->failed) {
        free(pipeline_status_response_queue->failed);
        pipeline_status_response_queue->failed = NULL;
    }
    if (pipeline_status_response_queue->total) {
        free(pipeline_status_response_queue->total);
        pipeline_status_response_queue->total = NULL;
    }
    free(pipeline_status_response_queue);
}

cJSON *pipeline_status_response_queue_convertToJSON(pipeline_status_response_queue_t *pipeline_status_response_queue) {
    cJSON *item = cJSON_CreateObject();

    // pipeline_status_response_queue->pending
    if(pipeline_status_response_queue->pending) {
    if(cJSON_AddNumberToObject(item, "pending", *pipeline_status_response_queue->pending) == NULL) {
    goto fail; //Numeric
    }
    }


    // pipeline_status_response_queue->running
    if(pipeline_status_response_queue->running) {
    if(cJSON_AddNumberToObject(item, "running", *pipeline_status_response_queue->running) == NULL) {
    goto fail; //Numeric
    }
    }


    // pipeline_status_response_queue->done
    if(pipeline_status_response_queue->done) {
    if(cJSON_AddNumberToObject(item, "done", *pipeline_status_response_queue->done) == NULL) {
    goto fail; //Numeric
    }
    }


    // pipeline_status_response_queue->failed
    if(pipeline_status_response_queue->failed) {
    if(cJSON_AddNumberToObject(item, "failed", *pipeline_status_response_queue->failed) == NULL) {
    goto fail; //Numeric
    }
    }


    // pipeline_status_response_queue->total
    if(pipeline_status_response_queue->total) {
    if(cJSON_AddNumberToObject(item, "total", *pipeline_status_response_queue->total) == NULL) {
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

pipeline_status_response_queue_t *pipeline_status_response_queue_parseFromJSON(cJSON *pipeline_status_response_queueJSON){

    pipeline_status_response_queue_t *pipeline_status_response_queue_local_var = NULL;

    // define the local variable for pipeline_status_response_queue->pending
    int *pending_local_var = NULL;

    // define the local variable for pipeline_status_response_queue->running
    int *running_local_var = NULL;

    // define the local variable for pipeline_status_response_queue->done
    int *done_local_var = NULL;

    // define the local variable for pipeline_status_response_queue->failed
    int *failed_local_var = NULL;

    // define the local variable for pipeline_status_response_queue->total
    int *total_local_var = NULL;

    // pipeline_status_response_queue->pending
    cJSON *pending = cJSON_GetObjectItemCaseSensitive(pipeline_status_response_queueJSON, "pending");
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

    // pipeline_status_response_queue->running
    cJSON *running = cJSON_GetObjectItemCaseSensitive(pipeline_status_response_queueJSON, "running");
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

    // pipeline_status_response_queue->done
    cJSON *done = cJSON_GetObjectItemCaseSensitive(pipeline_status_response_queueJSON, "done");
    if (cJSON_IsNull(done)) {
        done = NULL;
    }
    if (done) { 
    if(!cJSON_IsNumber(done))
    {
    goto end; //Numeric
    }
    done_local_var = malloc(sizeof(int));
    if(!done_local_var)
    {
        goto end;
    }
    *done_local_var = done->valuedouble;
    }

    // pipeline_status_response_queue->failed
    cJSON *failed = cJSON_GetObjectItemCaseSensitive(pipeline_status_response_queueJSON, "failed");
    if (cJSON_IsNull(failed)) {
        failed = NULL;
    }
    if (failed) { 
    if(!cJSON_IsNumber(failed))
    {
    goto end; //Numeric
    }
    failed_local_var = malloc(sizeof(int));
    if(!failed_local_var)
    {
        goto end;
    }
    *failed_local_var = failed->valuedouble;
    }

    // pipeline_status_response_queue->total
    cJSON *total = cJSON_GetObjectItemCaseSensitive(pipeline_status_response_queueJSON, "total");
    if (cJSON_IsNull(total)) {
        total = NULL;
    }
    if (total) { 
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
    }



    pipeline_status_response_queue_local_var = pipeline_status_response_queue_create_internal (
        pending_local_var,
        running_local_var,
        done_local_var,
        failed_local_var,
        total_local_var
        );

    if (!pipeline_status_response_queue_local_var) {
        goto end;
    }

    return pipeline_status_response_queue_local_var;
end:
    if (pending_local_var) {
        free(pending_local_var);
        pending_local_var = NULL;
    }
    if (running_local_var) {
        free(running_local_var);
        running_local_var = NULL;
    }
    if (done_local_var) {
        free(done_local_var);
        done_local_var = NULL;
    }
    if (failed_local_var) {
        free(failed_local_var);
        failed_local_var = NULL;
    }
    if (total_local_var) {
        free(total_local_var);
        total_local_var = NULL;
    }
    return NULL;

}
