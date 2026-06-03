#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "rollback_request.h"



static rollback_request_t *rollback_request_create_internal(
    long *target_release_id
    ) {
    rollback_request_t *rollback_request_local_var = malloc(sizeof(rollback_request_t));
    if (!rollback_request_local_var) {
        return NULL;
    }
    memset(rollback_request_local_var, 0, sizeof(rollback_request_t));
    rollback_request_local_var->_library_owned = 1;
    rollback_request_local_var->target_release_id = target_release_id;
    return rollback_request_local_var;
}

__attribute__((deprecated)) rollback_request_t *rollback_request_create(
    long *target_release_id
    ) {
    long *target_release_id_copy = NULL;
    if (target_release_id) {
        target_release_id_copy = malloc(sizeof(long));
        if (target_release_id_copy) *target_release_id_copy = *target_release_id;
    }
    rollback_request_t *result = rollback_request_create_internal (
        target_release_id_copy
        );
    if (!result) {
        free(target_release_id_copy);
    }
    return result;
}

void rollback_request_free(rollback_request_t *rollback_request) {
    if(NULL == rollback_request){
        return ;
    }
    if(rollback_request->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "rollback_request_free");
        return ;
    }
    listEntry_t *listEntry;
    if (rollback_request->target_release_id) {
        free(rollback_request->target_release_id);
        rollback_request->target_release_id = NULL;
    }
    free(rollback_request);
}

cJSON *rollback_request_convertToJSON(rollback_request_t *rollback_request) {
    cJSON *item = cJSON_CreateObject();

    // rollback_request->target_release_id
    if(rollback_request->target_release_id) {
    if(cJSON_AddNumberToObject(item, "target_release_id", *rollback_request->target_release_id) == NULL) {
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

rollback_request_t *rollback_request_parseFromJSON(cJSON *rollback_requestJSON){

    rollback_request_t *rollback_request_local_var = NULL;

    // define the local variable for rollback_request->target_release_id
    long *target_release_id_local_var = NULL;

    // rollback_request->target_release_id
    cJSON *target_release_id = cJSON_GetObjectItemCaseSensitive(rollback_requestJSON, "target_release_id");
    if (cJSON_IsNull(target_release_id)) {
        target_release_id = NULL;
    }
    if (target_release_id) { 
    if(!cJSON_IsNumber(target_release_id))
    {
    goto end; //Numeric
    }
    target_release_id_local_var = malloc(sizeof(long));
    if(!target_release_id_local_var)
    {
        goto end;
    }
    *target_release_id_local_var = target_release_id->valuedouble;
    }



    rollback_request_local_var = rollback_request_create_internal (
        target_release_id_local_var
        );

    if (!rollback_request_local_var) {
        goto end;
    }

    return rollback_request_local_var;
end:
    if (target_release_id_local_var) {
        free(target_release_id_local_var);
        target_release_id_local_var = NULL;
    }
    return NULL;

}
