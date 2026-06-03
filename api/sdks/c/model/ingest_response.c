#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "ingest_response.h"



static ingest_response_t *ingest_response_create_internal(
    char *status,
    int *projects_queued,
    char *message
    ) {
    ingest_response_t *ingest_response_local_var = malloc(sizeof(ingest_response_t));
    if (!ingest_response_local_var) {
        return NULL;
    }
    memset(ingest_response_local_var, 0, sizeof(ingest_response_t));
    ingest_response_local_var->_library_owned = 1;
    ingest_response_local_var->status = status;
    ingest_response_local_var->projects_queued = projects_queued;
    ingest_response_local_var->message = message;
    return ingest_response_local_var;
}

__attribute__((deprecated)) ingest_response_t *ingest_response_create(
    char *status,
    int *projects_queued,
    char *message
    ) {
    int *projects_queued_copy = NULL;
    if (projects_queued) {
        projects_queued_copy = malloc(sizeof(int));
        if (projects_queued_copy) *projects_queued_copy = *projects_queued;
    }
    ingest_response_t *result = ingest_response_create_internal (
        status,
        projects_queued_copy,
        message
        );
    if (!result) {
        free(projects_queued_copy);
    }
    return result;
}

void ingest_response_free(ingest_response_t *ingest_response) {
    if(NULL == ingest_response){
        return ;
    }
    if(ingest_response->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "ingest_response_free");
        return ;
    }
    listEntry_t *listEntry;
    if (ingest_response->status) {
        free(ingest_response->status);
        ingest_response->status = NULL;
    }
    if (ingest_response->projects_queued) {
        free(ingest_response->projects_queued);
        ingest_response->projects_queued = NULL;
    }
    if (ingest_response->message) {
        free(ingest_response->message);
        ingest_response->message = NULL;
    }
    free(ingest_response);
}

cJSON *ingest_response_convertToJSON(ingest_response_t *ingest_response) {
    cJSON *item = cJSON_CreateObject();

    // ingest_response->status
    if(ingest_response->status) {
    if(cJSON_AddStringToObject(item, "status", ingest_response->status) == NULL) {
    goto fail; //String
    }
    }


    // ingest_response->projects_queued
    if(ingest_response->projects_queued) {
    if(cJSON_AddNumberToObject(item, "projects_queued", *ingest_response->projects_queued) == NULL) {
    goto fail; //Numeric
    }
    }


    // ingest_response->message
    if(ingest_response->message) {
    if(cJSON_AddStringToObject(item, "message", ingest_response->message) == NULL) {
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

ingest_response_t *ingest_response_parseFromJSON(cJSON *ingest_responseJSON){

    ingest_response_t *ingest_response_local_var = NULL;

    char *status_local_str = NULL;

    // define the local variable for ingest_response->projects_queued
    int *projects_queued_local_var = NULL;

    char *message_local_str = NULL;

    // ingest_response->status
    cJSON *status = cJSON_GetObjectItemCaseSensitive(ingest_responseJSON, "status");
    if (cJSON_IsNull(status)) {
        status = NULL;
    }
    if (status) { 
    if(!cJSON_IsString(status) && !cJSON_IsNull(status))
    {
    goto end; //String
    }
    }

    // ingest_response->projects_queued
    cJSON *projects_queued = cJSON_GetObjectItemCaseSensitive(ingest_responseJSON, "projects_queued");
    if (cJSON_IsNull(projects_queued)) {
        projects_queued = NULL;
    }
    if (projects_queued) { 
    if(!cJSON_IsNumber(projects_queued))
    {
    goto end; //Numeric
    }
    projects_queued_local_var = malloc(sizeof(int));
    if(!projects_queued_local_var)
    {
        goto end;
    }
    *projects_queued_local_var = projects_queued->valuedouble;
    }

    // ingest_response->message
    cJSON *message = cJSON_GetObjectItemCaseSensitive(ingest_responseJSON, "message");
    if (cJSON_IsNull(message)) {
        message = NULL;
    }
    if (message) { 
    if(!cJSON_IsString(message) && !cJSON_IsNull(message))
    {
    goto end; //String
    }
    }


    if (status && !cJSON_IsNull(status)) status_local_str = strdup(status->valuestring);
    if (message && !cJSON_IsNull(message)) message_local_str = strdup(message->valuestring);

    ingest_response_local_var = ingest_response_create_internal (
        status_local_str,
        projects_queued_local_var,
        message_local_str
        );

    if (!ingest_response_local_var) {
        goto end;
    }

    return ingest_response_local_var;
end:
    if (status_local_str) {
        free(status_local_str);
        status_local_str = NULL;
    }
    if (projects_queued_local_var) {
        free(projects_queued_local_var);
        projects_queued_local_var = NULL;
    }
    if (message_local_str) {
        free(message_local_str);
        message_local_str = NULL;
    }
    return NULL;

}
