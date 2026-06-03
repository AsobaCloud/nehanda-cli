#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "create_release_response.h"


char* create_release_response_state_ToString(aimee_kb_api_create_release_response_STATE_e state) {
    char* stateArray[] =  { "NULL", "pending" };
    return stateArray[state];
}

aimee_kb_api_create_release_response_STATE_e create_release_response_state_FromString(char* state){
    int stringToReturn = 0;
    char *stateArray[] =  { "NULL", "pending" };
    size_t sizeofArray = sizeof(stateArray) / sizeof(stateArray[0]);
    while(stringToReturn < sizeofArray) {
        if(strcmp(state, stateArray[stringToReturn]) == 0) {
            return stringToReturn;
        }
        stringToReturn++;
    }
    return 0;
}

static create_release_response_t *create_release_response_create_internal(
    long *release_id,
    aimee_kb_api_create_release_response_STATE_e state
    ) {
    create_release_response_t *create_release_response_local_var = malloc(sizeof(create_release_response_t));
    if (!create_release_response_local_var) {
        return NULL;
    }
    memset(create_release_response_local_var, 0, sizeof(create_release_response_t));
    create_release_response_local_var->_library_owned = 1;
    create_release_response_local_var->release_id = release_id;
    create_release_response_local_var->state = state;
    return create_release_response_local_var;
}

__attribute__((deprecated)) create_release_response_t *create_release_response_create(
    long *release_id,
    aimee_kb_api_create_release_response_STATE_e state
    ) {
    long *release_id_copy = NULL;
    if (release_id) {
        release_id_copy = malloc(sizeof(long));
        if (release_id_copy) *release_id_copy = *release_id;
    }
    create_release_response_t *result = create_release_response_create_internal (
        release_id_copy,
        state
        );
    if (!result) {
        free(release_id_copy);
    }
    return result;
}

void create_release_response_free(create_release_response_t *create_release_response) {
    if(NULL == create_release_response){
        return ;
    }
    if(create_release_response->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "create_release_response_free");
        return ;
    }
    listEntry_t *listEntry;
    if (create_release_response->release_id) {
        free(create_release_response->release_id);
        create_release_response->release_id = NULL;
    }
    free(create_release_response);
}

cJSON *create_release_response_convertToJSON(create_release_response_t *create_release_response) {
    cJSON *item = cJSON_CreateObject();

    // create_release_response->release_id
    if(create_release_response->release_id) {
    if(cJSON_AddNumberToObject(item, "release_id", *create_release_response->release_id) == NULL) {
    goto fail; //Numeric
    }
    }


    // create_release_response->state
    if(create_release_response->state != aimee_kb_api_create_release_response_STATE_NULL) {
    if(cJSON_AddStringToObject(item, "state", create_release_response_state_ToString(create_release_response->state)) == NULL)
    {
    goto fail; //Enum
    }
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

create_release_response_t *create_release_response_parseFromJSON(cJSON *create_release_responseJSON){

    create_release_response_t *create_release_response_local_var = NULL;

    // define the local variable for create_release_response->release_id
    long *release_id_local_var = NULL;

    // create_release_response->release_id
    cJSON *release_id = cJSON_GetObjectItemCaseSensitive(create_release_responseJSON, "release_id");
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

    // create_release_response->state
    cJSON *state = cJSON_GetObjectItemCaseSensitive(create_release_responseJSON, "state");
    if (cJSON_IsNull(state)) {
        state = NULL;
    }
    aimee_kb_api_create_release_response_STATE_e stateVariable;
    if (state) { 
    if(!cJSON_IsString(state))
    {
    goto end; //Enum
    }
    stateVariable = create_release_response_state_FromString(state->valuestring);
    }



    create_release_response_local_var = create_release_response_create_internal (
        release_id_local_var,
        state ? stateVariable : aimee_kb_api_create_release_response_STATE_NULL
        );

    if (!create_release_response_local_var) {
        goto end;
    }

    return create_release_response_local_var;
end:
    if (release_id_local_var) {
        free(release_id_local_var);
        release_id_local_var = NULL;
    }
    return NULL;

}
