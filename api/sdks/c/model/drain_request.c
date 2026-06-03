#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "drain_request.h"



static drain_request_t *drain_request_create_internal(
    char *embedding_command,
    int *timeout
    ) {
    drain_request_t *drain_request_local_var = malloc(sizeof(drain_request_t));
    if (!drain_request_local_var) {
        return NULL;
    }
    memset(drain_request_local_var, 0, sizeof(drain_request_t));
    drain_request_local_var->_library_owned = 1;
    drain_request_local_var->embedding_command = embedding_command;
    drain_request_local_var->timeout = timeout;
    return drain_request_local_var;
}

__attribute__((deprecated)) drain_request_t *drain_request_create(
    char *embedding_command,
    int *timeout
    ) {
    int *timeout_copy = NULL;
    if (timeout) {
        timeout_copy = malloc(sizeof(int));
        if (timeout_copy) *timeout_copy = *timeout;
    }
    drain_request_t *result = drain_request_create_internal (
        embedding_command,
        timeout_copy
        );
    if (!result) {
        free(timeout_copy);
    }
    return result;
}

void drain_request_free(drain_request_t *drain_request) {
    if(NULL == drain_request){
        return ;
    }
    if(drain_request->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "drain_request_free");
        return ;
    }
    listEntry_t *listEntry;
    if (drain_request->embedding_command) {
        free(drain_request->embedding_command);
        drain_request->embedding_command = NULL;
    }
    if (drain_request->timeout) {
        free(drain_request->timeout);
        drain_request->timeout = NULL;
    }
    free(drain_request);
}

cJSON *drain_request_convertToJSON(drain_request_t *drain_request) {
    cJSON *item = cJSON_CreateObject();

    // drain_request->embedding_command
    if(drain_request->embedding_command) {
    if(cJSON_AddStringToObject(item, "embedding_command", drain_request->embedding_command) == NULL) {
    goto fail; //String
    }
    }


    // drain_request->timeout
    if(drain_request->timeout) {
    if(cJSON_AddNumberToObject(item, "timeout", *drain_request->timeout) == NULL) {
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

drain_request_t *drain_request_parseFromJSON(cJSON *drain_requestJSON){

    drain_request_t *drain_request_local_var = NULL;

    char *embedding_command_local_str = NULL;

    // define the local variable for drain_request->timeout
    int *timeout_local_var = NULL;

    // drain_request->embedding_command
    cJSON *embedding_command = cJSON_GetObjectItemCaseSensitive(drain_requestJSON, "embedding_command");
    if (cJSON_IsNull(embedding_command)) {
        embedding_command = NULL;
    }
    if (embedding_command) { 
    if(!cJSON_IsString(embedding_command) && !cJSON_IsNull(embedding_command))
    {
    goto end; //String
    }
    }

    // drain_request->timeout
    cJSON *timeout = cJSON_GetObjectItemCaseSensitive(drain_requestJSON, "timeout");
    if (cJSON_IsNull(timeout)) {
        timeout = NULL;
    }
    if (timeout) { 
    if(!cJSON_IsNumber(timeout))
    {
    goto end; //Numeric
    }
    timeout_local_var = malloc(sizeof(int));
    if(!timeout_local_var)
    {
        goto end;
    }
    *timeout_local_var = timeout->valuedouble;
    }


    if (embedding_command && !cJSON_IsNull(embedding_command)) embedding_command_local_str = strdup(embedding_command->valuestring);

    drain_request_local_var = drain_request_create_internal (
        embedding_command_local_str,
        timeout_local_var
        );

    if (!drain_request_local_var) {
        goto end;
    }

    return drain_request_local_var;
end:
    if (embedding_command_local_str) {
        free(embedding_command_local_str);
        embedding_command_local_str = NULL;
    }
    if (timeout_local_var) {
        free(timeout_local_var);
        timeout_local_var = NULL;
    }
    return NULL;

}
