#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "pipeline_status_response.h"


char* pipeline_status_response_state_ToString(aimee_kb_api_pipeline_status_response_STATE_e state) {
    char* stateArray[] =  { "NULL", "idle", "running", "failed" };
    return stateArray[state];
}

aimee_kb_api_pipeline_status_response_STATE_e pipeline_status_response_state_FromString(char* state){
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

static pipeline_status_response_t *pipeline_status_response_create_internal(
    aimee_kb_api_pipeline_status_response_STATE_e state,
    int *queue_depth,
    list_t *active_jobs,
    pipeline_status_response_queue_t *queue
    ) {
    pipeline_status_response_t *pipeline_status_response_local_var = malloc(sizeof(pipeline_status_response_t));
    if (!pipeline_status_response_local_var) {
        return NULL;
    }
    memset(pipeline_status_response_local_var, 0, sizeof(pipeline_status_response_t));
    pipeline_status_response_local_var->_library_owned = 1;
    pipeline_status_response_local_var->state = state;
    pipeline_status_response_local_var->queue_depth = queue_depth;
    pipeline_status_response_local_var->active_jobs = active_jobs;
    pipeline_status_response_local_var->queue = queue;
    return pipeline_status_response_local_var;
}

__attribute__((deprecated)) pipeline_status_response_t *pipeline_status_response_create(
    aimee_kb_api_pipeline_status_response_STATE_e state,
    int *queue_depth,
    list_t *active_jobs,
    pipeline_status_response_queue_t *queue
    ) {
    int *queue_depth_copy = NULL;
    if (queue_depth) {
        queue_depth_copy = malloc(sizeof(int));
        if (queue_depth_copy) *queue_depth_copy = *queue_depth;
    }
    pipeline_status_response_t *result = pipeline_status_response_create_internal (
        state,
        queue_depth_copy,
        active_jobs,
        queue
        );
    if (!result) {
        free(queue_depth_copy);
    }
    return result;
}

void pipeline_status_response_free(pipeline_status_response_t *pipeline_status_response) {
    if(NULL == pipeline_status_response){
        return ;
    }
    if(pipeline_status_response->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "pipeline_status_response_free");
        return ;
    }
    listEntry_t *listEntry;
    if (pipeline_status_response->queue_depth) {
        free(pipeline_status_response->queue_depth);
        pipeline_status_response->queue_depth = NULL;
    }
    if (pipeline_status_response->active_jobs) {
        list_ForEach(listEntry, pipeline_status_response->active_jobs) {
            object_free(listEntry->data);
        }
        list_freeList(pipeline_status_response->active_jobs);
        pipeline_status_response->active_jobs = NULL;
    }
    if (pipeline_status_response->queue) {
        pipeline_status_response_queue_free(pipeline_status_response->queue);
        pipeline_status_response->queue = NULL;
    }
    free(pipeline_status_response);
}

cJSON *pipeline_status_response_convertToJSON(pipeline_status_response_t *pipeline_status_response) {
    cJSON *item = cJSON_CreateObject();

    // pipeline_status_response->state
    if(pipeline_status_response->state != aimee_kb_api_pipeline_status_response_STATE_NULL) {
    if(cJSON_AddStringToObject(item, "state", pipeline_status_response_state_ToString(pipeline_status_response->state)) == NULL)
    {
    goto fail; //Enum
    }
    }


    // pipeline_status_response->queue_depth
    if(pipeline_status_response->queue_depth) {
    if(cJSON_AddNumberToObject(item, "queue_depth", *pipeline_status_response->queue_depth) == NULL) {
    goto fail; //Numeric
    }
    }


    // pipeline_status_response->active_jobs
    if(pipeline_status_response->active_jobs) {
    cJSON *active_jobs = cJSON_AddArrayToObject(item, "active_jobs");
    if(active_jobs == NULL) {
    goto fail; //nonprimitive container
    }

    listEntry_t *active_jobsListEntry;
    if (pipeline_status_response->active_jobs) {
    list_ForEach(active_jobsListEntry, pipeline_status_response->active_jobs) {
    cJSON *itemLocal = object_convertToJSON(active_jobsListEntry->data);
    if(itemLocal == NULL) {
    goto fail;
    }
    cJSON_AddItemToArray(active_jobs, itemLocal);
    }
    }
    }


    // pipeline_status_response->queue
    if(pipeline_status_response->queue) {
    cJSON *queue_local_JSON = pipeline_status_response_queue_convertToJSON(pipeline_status_response->queue);
    if(queue_local_JSON == NULL) {
    goto fail; //model
    }
    cJSON_AddItemToObject(item, "queue", queue_local_JSON);
    if(item->child == NULL) {
    goto fail;
    }
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

pipeline_status_response_t *pipeline_status_response_parseFromJSON(cJSON *pipeline_status_responseJSON){

    pipeline_status_response_t *pipeline_status_response_local_var = NULL;

    // define the local variable for pipeline_status_response->queue_depth
    int *queue_depth_local_var = NULL;

    // define the local list for pipeline_status_response->active_jobs
    list_t *active_jobsList = NULL;

    // define the local variable for pipeline_status_response->queue
    pipeline_status_response_queue_t *queue_local_nonprim = NULL;

    // pipeline_status_response->state
    cJSON *state = cJSON_GetObjectItemCaseSensitive(pipeline_status_responseJSON, "state");
    if (cJSON_IsNull(state)) {
        state = NULL;
    }
    aimee_kb_api_pipeline_status_response_STATE_e stateVariable;
    if (state) { 
    if(!cJSON_IsString(state))
    {
    goto end; //Enum
    }
    stateVariable = pipeline_status_response_state_FromString(state->valuestring);
    }

    // pipeline_status_response->queue_depth
    cJSON *queue_depth = cJSON_GetObjectItemCaseSensitive(pipeline_status_responseJSON, "queue_depth");
    if (cJSON_IsNull(queue_depth)) {
        queue_depth = NULL;
    }
    if (queue_depth) { 
    if(!cJSON_IsNumber(queue_depth))
    {
    goto end; //Numeric
    }
    queue_depth_local_var = malloc(sizeof(int));
    if(!queue_depth_local_var)
    {
        goto end;
    }
    *queue_depth_local_var = queue_depth->valuedouble;
    }

    // pipeline_status_response->active_jobs
    cJSON *active_jobs = cJSON_GetObjectItemCaseSensitive(pipeline_status_responseJSON, "active_jobs");
    if (cJSON_IsNull(active_jobs)) {
        active_jobs = NULL;
    }
    if (active_jobs) { 
    cJSON *active_jobs_local_nonprimitive = NULL;
    if(!cJSON_IsArray(active_jobs)){
        goto end; //nonprimitive container
    }

    active_jobsList = list_createList();

    cJSON_ArrayForEach(active_jobs_local_nonprimitive,active_jobs )
    {
        if(!cJSON_IsObject(active_jobs_local_nonprimitive)){
            goto end;
        }
        object_t *active_jobsItem = object_parseFromJSON(active_jobs_local_nonprimitive);

        list_addElement(active_jobsList, active_jobsItem);
    }
    }

    // pipeline_status_response->queue
    cJSON *queue = cJSON_GetObjectItemCaseSensitive(pipeline_status_responseJSON, "queue");
    if (cJSON_IsNull(queue)) {
        queue = NULL;
    }
    if (queue) { 
    queue_local_nonprim = pipeline_status_response_queue_parseFromJSON(queue); //nonprimitive
    }



    pipeline_status_response_local_var = pipeline_status_response_create_internal (
        state ? stateVariable : aimee_kb_api_pipeline_status_response_STATE_NULL,
        queue_depth_local_var,
        active_jobs ? active_jobsList : NULL,
        queue ? queue_local_nonprim : NULL
        );

    if (!pipeline_status_response_local_var) {
        goto end;
    }

    return pipeline_status_response_local_var;
end:
    if (queue_depth_local_var) {
        free(queue_depth_local_var);
        queue_depth_local_var = NULL;
    }
    if (active_jobsList) {
        listEntry_t *listEntry = NULL;
        list_ForEach(listEntry, active_jobsList) {
            object_free(listEntry->data);
            listEntry->data = NULL;
        }
        list_freeList(active_jobsList);
        active_jobsList = NULL;
    }
    if (queue_local_nonprim) {
        pipeline_status_response_queue_free(queue_local_nonprim);
        queue_local_nonprim = NULL;
    }
    return NULL;

}
