#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "review_accept_request.h"



static review_accept_request_t *review_accept_request_create_internal(
    long *release_id
    ) {
    review_accept_request_t *review_accept_request_local_var = malloc(sizeof(review_accept_request_t));
    if (!review_accept_request_local_var) {
        return NULL;
    }
    memset(review_accept_request_local_var, 0, sizeof(review_accept_request_t));
    review_accept_request_local_var->_library_owned = 1;
    review_accept_request_local_var->release_id = release_id;
    return review_accept_request_local_var;
}

__attribute__((deprecated)) review_accept_request_t *review_accept_request_create(
    long *release_id
    ) {
    long *release_id_copy = NULL;
    if (release_id) {
        release_id_copy = malloc(sizeof(long));
        if (release_id_copy) *release_id_copy = *release_id;
    }
    review_accept_request_t *result = review_accept_request_create_internal (
        release_id_copy
        );
    if (!result) {
        free(release_id_copy);
    }
    return result;
}

void review_accept_request_free(review_accept_request_t *review_accept_request) {
    if(NULL == review_accept_request){
        return ;
    }
    if(review_accept_request->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "review_accept_request_free");
        return ;
    }
    listEntry_t *listEntry;
    if (review_accept_request->release_id) {
        free(review_accept_request->release_id);
        review_accept_request->release_id = NULL;
    }
    free(review_accept_request);
}

cJSON *review_accept_request_convertToJSON(review_accept_request_t *review_accept_request) {
    cJSON *item = cJSON_CreateObject();

    // review_accept_request->release_id
    if(review_accept_request->release_id) {
    if(cJSON_AddNumberToObject(item, "release_id", *review_accept_request->release_id) == NULL) {
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

review_accept_request_t *review_accept_request_parseFromJSON(cJSON *review_accept_requestJSON){

    review_accept_request_t *review_accept_request_local_var = NULL;

    // define the local variable for review_accept_request->release_id
    long *release_id_local_var = NULL;

    // review_accept_request->release_id
    cJSON *release_id = cJSON_GetObjectItemCaseSensitive(review_accept_requestJSON, "release_id");
    if (cJSON_IsNull(release_id)) {
        release_id = NULL;
    }
    if (release_id) { 
    if(!cJSON_IsNumber(release_id))
    {
    goto end; //Numeric
    }
    release_id_local_var = malloc(sizeof(long));
    if(!release_id_local_var)
    {
        goto end;
    }
    *release_id_local_var = release_id->valuedouble;
    }



    review_accept_request_local_var = review_accept_request_create_internal (
        release_id_local_var
        );

    if (!review_accept_request_local_var) {
        goto end;
    }

    return review_accept_request_local_var;
end:
    if (release_id_local_var) {
        free(release_id_local_var);
        release_id_local_var = NULL;
    }
    return NULL;

}
