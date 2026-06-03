#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "drain_response.h"


char* drain_response_state_ToString(aimee_kb_api_drain_response_STATE_e state) {
    char* stateArray[] =  { "NULL", "idle", "running", "failed" };
    return stateArray[state];
}

aimee_kb_api_drain_response_STATE_e drain_response_state_FromString(char* state){
    int stringToReturn = 0;
    char *stateArray[] =  { "NULL", "idle", "running", "failed" };
    size_t sizeofArray = sizeof(stateArray) / sizeof(stateArray[0]);
    while(stringToReturn < sizeofArray) {
        if(strcmp(state, stateArray[stringToReturn]) == 0) {
            return stringToReturn;
        }
        stringToReturn++;
    }
    return 0;
}

static drain_response_t *drain_response_create_internal(
    aimee_kb_api_drain_response_STATE_e state,
    int *processed,
    int *pending,
    int *running,
    int *done,
    int *failed,
    int *total
    ) {
    drain_response_t *drain_response_local_var = malloc(sizeof(drain_response_t));
    if (!drain_response_local_var) {
        return NULL;
    }
    memset(drain_response_local_var, 0, sizeof(drain_response_t));
    drain_response_local_var->_library_owned = 1;
    drain_response_local_var->state = state;
    drain_response_local_var->processed = processed;
    drain_response_local_var->pending = pending;
    drain_response_local_var->running = running;
    drain_response_local_var->done = done;
    drain_response_local_var->failed = failed;
    drain_response_local_var->total = total;
    return drain_response_local_var;
}

__attribute__((deprecated)) drain_response_t *drain_response_create(
    aimee_kb_api_drain_response_STATE_e state,
    int *processed,
    int *pending,
    int *running,
    int *done,
    int *failed,
    int *total
    ) {
    int *processed_copy = NULL;
    if (processed) {
        processed_copy = malloc(sizeof(int));
        if (processed_copy) *processed_copy = *processed;
    }
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
    drain_response_t *result = drain_response_create_internal (
        state,
        processed_copy,
        pending_copy,
        running_copy,
        done_copy,
        failed_copy,
        total_copy
        );
    if (!result) {
        free(processed_copy);
        free(pending_copy);
        free(running_copy);
        free(done_copy);
        free(failed_copy);
        free(total_copy);
    }
    return result;
}

void drain_response_free(drain_response_t *drain_response) {
    if(NULL == drain_response){
        return ;
    }
    if(drain_response->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "drain_response_free");
        return ;
    }
    listEntry_t *listEntry;
    if (drain_response->processed) {
        free(drain_response->processed);
        drain_response->processed = NULL;
    }
    if (drain_response->pending) {
        free(drain_response->pending);
        drain_response->pending = NULL;
    }
    if (drain_response->running) {
        free(drain_response->running);
        drain_response->running = NULL;
    }
    if (drain_response->done) {
        free(drain_response->done);
        drain_response->done = NULL;
    }
    if (drain_response->failed) {
        free(drain_response->failed);
        drain_response->failed = NULL;
    }
    if (drain_response->total) {
        free(drain_response->total);
        drain_response->total = NULL;
    }
    free(drain_response);
}

cJSON *drain_response_convertToJSON(drain_response_t *drain_response) {
    cJSON *item = cJSON_CreateObject();

    // drain_response->state
    if(drain_response->state != aimee_kb_api_drain_response_STATE_NULL) {
    if(cJSON_AddStringToObject(item, "state", drain_response_state_ToString(drain_response->state)) == NULL)
    {
    goto fail; //Enum
    }
    }


    // drain_response->processed
    if(drain_response->processed) {
    if(cJSON_AddNumberToObject(item, "processed", *drain_response->processed) == NULL) {
    goto fail; //Numeric
    }
    }


    // drain_response->pending
    if(drain_response->pending) {
    if(cJSON_AddNumberToObject(item, "pending", *drain_response->pending) == NULL) {
    goto fail; //Numeric
    }
    }


    // drain_response->running
    if(drain_response->running) {
    if(cJSON_AddNumberToObject(item, "running", *drain_response->running) == NULL) {
    goto fail; //Numeric
    }
    }


    // drain_response->done
    if(drain_response->done) {
    if(cJSON_AddNumberToObject(item, "done", *drain_response->done) == NULL) {
    goto fail; //Numeric
    }
    }


    // drain_response->failed
    if(drain_response->failed) {
    if(cJSON_AddNumberToObject(item, "failed", *drain_response->failed) == NULL) {
    goto fail; //Numeric
    }
    }


    // drain_response->total
    if(drain_response->total) {
    if(cJSON_AddNumberToObject(item, "total", *drain_response->total) == NULL) {
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

drain_response_t *drain_response_parseFromJSON(cJSON *drain_responseJSON){

    drain_response_t *drain_response_local_var = NULL;

    // define the local variable for drain_response->processed
    int *processed_local_var = NULL;

    // define the local variable for drain_response->pending
    int *pending_local_var = NULL;

    // define the local variable for drain_response->running
    int *running_local_var = NULL;

    // define the local variable for drain_response->done
    int *done_local_var = NULL;

    // define the local variable for drain_response->failed
    int *failed_local_var = NULL;

    // define the local variable for drain_response->total
    int *total_local_var = NULL;

    // drain_response->state
    cJSON *state = cJSON_GetObjectItemCaseSensitive(drain_responseJSON, "state");
    if (cJSON_IsNull(state)) {
        state = NULL;
    }
    aimee_kb_api_drain_response_STATE_e stateVariable;
    if (state) { 
    if(!cJSON_IsString(state))
    {
    goto end; //Enum
    }
    stateVariable = drain_response_state_FromString(state->valuestring);
    }

    // drain_response->processed
    cJSON *processed = cJSON_GetObjectItemCaseSensitive(drain_responseJSON, "processed");
    if (cJSON_IsNull(processed)) {
        processed = NULL;
    }
    if (processed) { 
    if(!cJSON_IsNumber(processed))
    {
    goto end; //Numeric
    }
    processed_local_var = malloc(sizeof(int));
    if(!processed_local_var)
    {
        goto end;
    }
    *processed_local_var = processed->valuedouble;
    }

    // drain_response->pending
    cJSON *pending = cJSON_GetObjectItemCaseSensitive(drain_responseJSON, "pending");
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

    // drain_response->running
    cJSON *running = cJSON_GetObjectItemCaseSensitive(drain_responseJSON, "running");
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

    // drain_response->done
    cJSON *done = cJSON_GetObjectItemCaseSensitive(drain_responseJSON, "done");
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

    // drain_response->failed
    cJSON *failed = cJSON_GetObjectItemCaseSensitive(drain_responseJSON, "failed");
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

    // drain_response->total
    cJSON *total = cJSON_GetObjectItemCaseSensitive(drain_responseJSON, "total");
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



    drain_response_local_var = drain_response_create_internal (
        state ? stateVariable : aimee_kb_api_drain_response_STATE_NULL,
        processed_local_var,
        pending_local_var,
        running_local_var,
        done_local_var,
        failed_local_var,
        total_local_var
        );

    if (!drain_response_local_var) {
        goto end;
    }

    return drain_response_local_var;
end:
    if (processed_local_var) {
        free(processed_local_var);
        processed_local_var = NULL;
    }
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
