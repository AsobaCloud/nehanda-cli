#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "error_response.h"



static error_response_t *error_response_create_internal(
    char *error
    ) {
    error_response_t *error_response_local_var = malloc(sizeof(error_response_t));
    if (!error_response_local_var) {
        return NULL;
    }
    memset(error_response_local_var, 0, sizeof(error_response_t));
    error_response_local_var->_library_owned = 1;
    error_response_local_var->error = error;
    return error_response_local_var;
}

__attribute__((deprecated)) error_response_t *error_response_create(
    char *error
    ) {
    error_response_t *result = error_response_create_internal (
        error
        );
    if (!result) {
    }
    return result;
}

void error_response_free(error_response_t *error_response) {
    if(NULL == error_response){
        return ;
    }
    if(error_response->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "error_response_free");
        return ;
    }
    listEntry_t *listEntry;
    if (error_response->error) {
        free(error_response->error);
        error_response->error = NULL;
    }
    free(error_response);
}

cJSON *error_response_convertToJSON(error_response_t *error_response) {
    cJSON *item = cJSON_CreateObject();

    // error_response->error
    if (!error_response->error) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "error", error_response->error) == NULL) {
    goto fail; //String
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

error_response_t *error_response_parseFromJSON(cJSON *error_responseJSON){

    error_response_t *error_response_local_var = NULL;

    char *error_local_str = NULL;

    // error_response->error
    cJSON *error = cJSON_GetObjectItemCaseSensitive(error_responseJSON, "error");
    if (cJSON_IsNull(error)) {
        error = NULL;
    }
    if (!error) {
        goto end;
    }

    
    if(!cJSON_IsString(error))
    {
    goto end; //String
    }


    if (error && !cJSON_IsNull(error)) error_local_str = strdup(error->valuestring);

    error_response_local_var = error_response_create_internal (
        error_local_str
        );

    if (!error_response_local_var) {
        goto end;
    }

    return error_response_local_var;
end:
    if (error_local_str) {
        free(error_local_str);
        error_local_str = NULL;
    }
    return NULL;

}
