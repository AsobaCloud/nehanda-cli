#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "doc_ingest_response.h"


char* doc_ingest_response_state_ToString(aimee_kb_api_doc_ingest_response_STATE_e state) {
    char* stateArray[] =  { "NULL", "staged" };
    return stateArray[state];
}

aimee_kb_api_doc_ingest_response_STATE_e doc_ingest_response_state_FromString(char* state){
    int stringToReturn = 0;
    char *stateArray[] =  { "NULL", "staged" };
    size_t sizeofArray = sizeof(stateArray) / sizeof(stateArray[0]);
    while(stringToReturn < sizeofArray) {
        if(strcmp(state, stateArray[stringToReturn]) == 0) {
            return stringToReturn;
        }
        stringToReturn++;
    }
    return 0;
}

static doc_ingest_response_t *doc_ingest_response_create_internal(
    long *doc_id,
    aimee_kb_api_doc_ingest_response_STATE_e state
    ) {
    doc_ingest_response_t *doc_ingest_response_local_var = malloc(sizeof(doc_ingest_response_t));
    if (!doc_ingest_response_local_var) {
        return NULL;
    }
    memset(doc_ingest_response_local_var, 0, sizeof(doc_ingest_response_t));
    doc_ingest_response_local_var->_library_owned = 1;
    doc_ingest_response_local_var->doc_id = doc_id;
    doc_ingest_response_local_var->state = state;
    return doc_ingest_response_local_var;
}

__attribute__((deprecated)) doc_ingest_response_t *doc_ingest_response_create(
    long *doc_id,
    aimee_kb_api_doc_ingest_response_STATE_e state
    ) {
    long *doc_id_copy = NULL;
    if (doc_id) {
        doc_id_copy = malloc(sizeof(long));
        if (doc_id_copy) *doc_id_copy = *doc_id;
    }
    doc_ingest_response_t *result = doc_ingest_response_create_internal (
        doc_id_copy,
        state
        );
    if (!result) {
        free(doc_id_copy);
    }
    return result;
}

void doc_ingest_response_free(doc_ingest_response_t *doc_ingest_response) {
    if(NULL == doc_ingest_response){
        return ;
    }
    if(doc_ingest_response->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "doc_ingest_response_free");
        return ;
    }
    listEntry_t *listEntry;
    if (doc_ingest_response->doc_id) {
        free(doc_ingest_response->doc_id);
        doc_ingest_response->doc_id = NULL;
    }
    free(doc_ingest_response);
}

cJSON *doc_ingest_response_convertToJSON(doc_ingest_response_t *doc_ingest_response) {
    cJSON *item = cJSON_CreateObject();

    // doc_ingest_response->doc_id
    if(doc_ingest_response->doc_id) {
    if(cJSON_AddNumberToObject(item, "doc_id", *doc_ingest_response->doc_id) == NULL) {
    goto fail; //Numeric
    }
    }


    // doc_ingest_response->state
    if(doc_ingest_response->state != aimee_kb_api_doc_ingest_response_STATE_NULL) {
    if(cJSON_AddStringToObject(item, "state", doc_ingest_response_state_ToString(doc_ingest_response->state)) == NULL)
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

doc_ingest_response_t *doc_ingest_response_parseFromJSON(cJSON *doc_ingest_responseJSON){

    doc_ingest_response_t *doc_ingest_response_local_var = NULL;

    // define the local variable for doc_ingest_response->doc_id
    long *doc_id_local_var = NULL;

    // doc_ingest_response->doc_id
    cJSON *doc_id = cJSON_GetObjectItemCaseSensitive(doc_ingest_responseJSON, "doc_id");
    if (cJSON_IsNull(doc_id)) {
        doc_id = NULL;
    }
    if (doc_id) { 
    if(!cJSON_IsNumber(doc_id))
    {
    goto end; //Numeric
    }
    doc_id_local_var = malloc(sizeof(long));
    if(!doc_id_local_var)
    {
        goto end;
    }
    *doc_id_local_var = doc_id->valuedouble;
    }

    // doc_ingest_response->state
    cJSON *state = cJSON_GetObjectItemCaseSensitive(doc_ingest_responseJSON, "state");
    if (cJSON_IsNull(state)) {
        state = NULL;
    }
    aimee_kb_api_doc_ingest_response_STATE_e stateVariable;
    if (state) { 
    if(!cJSON_IsString(state))
    {
    goto end; //Enum
    }
    stateVariable = doc_ingest_response_state_FromString(state->valuestring);
    }



    doc_ingest_response_local_var = doc_ingest_response_create_internal (
        doc_id_local_var,
        state ? stateVariable : aimee_kb_api_doc_ingest_response_STATE_NULL
        );

    if (!doc_ingest_response_local_var) {
        goto end;
    }

    return doc_ingest_response_local_var;
end:
    if (doc_id_local_var) {
        free(doc_id_local_var);
        doc_id_local_var = NULL;
    }
    return NULL;

}
