#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "review_reject_request.h"



static review_reject_request_t *review_reject_request_create_internal(
    char *reason
    ) {
    review_reject_request_t *review_reject_request_local_var = malloc(sizeof(review_reject_request_t));
    if (!review_reject_request_local_var) {
        return NULL;
    }
    memset(review_reject_request_local_var, 0, sizeof(review_reject_request_t));
    review_reject_request_local_var->_library_owned = 1;
    review_reject_request_local_var->reason = reason;
    return review_reject_request_local_var;
}

__attribute__((deprecated)) review_reject_request_t *review_reject_request_create(
    char *reason
    ) {
    review_reject_request_t *result = review_reject_request_create_internal (
        reason
        );
    if (!result) {
    }
    return result;
}

void review_reject_request_free(review_reject_request_t *review_reject_request) {
    if(NULL == review_reject_request){
        return ;
    }
    if(review_reject_request->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "review_reject_request_free");
        return ;
    }
    listEntry_t *listEntry;
    if (review_reject_request->reason) {
        free(review_reject_request->reason);
        review_reject_request->reason = NULL;
    }
    free(review_reject_request);
}

cJSON *review_reject_request_convertToJSON(review_reject_request_t *review_reject_request) {
    cJSON *item = cJSON_CreateObject();

    // review_reject_request->reason
    if (!review_reject_request->reason) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "reason", review_reject_request->reason) == NULL) {
    goto fail; //String
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

review_reject_request_t *review_reject_request_parseFromJSON(cJSON *review_reject_requestJSON){

    review_reject_request_t *review_reject_request_local_var = NULL;

    char *reason_local_str = NULL;

    // review_reject_request->reason
    cJSON *reason = cJSON_GetObjectItemCaseSensitive(review_reject_requestJSON, "reason");
    if (cJSON_IsNull(reason)) {
        reason = NULL;
    }
    if (!reason) {
        goto end;
    }

    
    if(!cJSON_IsString(reason))
    {
    goto end; //String
    }


    if (reason && !cJSON_IsNull(reason)) reason_local_str = strdup(reason->valuestring);

    review_reject_request_local_var = review_reject_request_create_internal (
        reason_local_str
        );

    if (!review_reject_request_local_var) {
        goto end;
    }

    return review_reject_request_local_var;
end:
    if (reason_local_str) {
        free(reason_local_str);
        reason_local_str = NULL;
    }
    return NULL;

}
