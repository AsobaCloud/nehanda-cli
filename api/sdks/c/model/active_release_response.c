#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "active_release_response.h"



static active_release_response_t *active_release_response_create_internal(
    long *release_id,
    char *name,
    char *state,
    char *promoted_at
    ) {
    active_release_response_t *active_release_response_local_var = malloc(sizeof(active_release_response_t));
    if (!active_release_response_local_var) {
        return NULL;
    }
    memset(active_release_response_local_var, 0, sizeof(active_release_response_t));
    active_release_response_local_var->_library_owned = 1;
    active_release_response_local_var->release_id = release_id;
    active_release_response_local_var->name = name;
    active_release_response_local_var->state = state;
    active_release_response_local_var->promoted_at = promoted_at;
    return active_release_response_local_var;
}

__attribute__((deprecated)) active_release_response_t *active_release_response_create(
    long *release_id,
    char *name,
    char *state,
    char *promoted_at
    ) {
    long *release_id_copy = NULL;
    if (release_id) {
        release_id_copy = malloc(sizeof(long));
        if (release_id_copy) *release_id_copy = *release_id;
    }
    active_release_response_t *result = active_release_response_create_internal (
        release_id_copy,
        name,
        state,
        promoted_at
        );
    if (!result) {
        free(release_id_copy);
    }
    return result;
}

void active_release_response_free(active_release_response_t *active_release_response) {
    if(NULL == active_release_response){
        return ;
    }
    if(active_release_response->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "active_release_response_free");
        return ;
    }
    listEntry_t *listEntry;
    if (active_release_response->release_id) {
        free(active_release_response->release_id);
        active_release_response->release_id = NULL;
    }
    if (active_release_response->name) {
        free(active_release_response->name);
        active_release_response->name = NULL;
    }
    if (active_release_response->state) {
        free(active_release_response->state);
        active_release_response->state = NULL;
    }
    if (active_release_response->promoted_at) {
        free(active_release_response->promoted_at);
        active_release_response->promoted_at = NULL;
    }
    free(active_release_response);
}

cJSON *active_release_response_convertToJSON(active_release_response_t *active_release_response) {
    cJSON *item = cJSON_CreateObject();

    // active_release_response->release_id
    if(active_release_response->release_id) {
    if(cJSON_AddNumberToObject(item, "release_id", *active_release_response->release_id) == NULL) {
    goto fail; //Numeric
    }
    }


    // active_release_response->name
    if(active_release_response->name) {
    if(cJSON_AddStringToObject(item, "name", active_release_response->name) == NULL) {
    goto fail; //String
    }
    }


    // active_release_response->state
    if(active_release_response->state) {
    if(cJSON_AddStringToObject(item, "state", active_release_response->state) == NULL) {
    goto fail; //String
    }
    }


    // active_release_response->promoted_at
    if(active_release_response->promoted_at) {
    if(cJSON_AddStringToObject(item, "promoted_at", active_release_response->promoted_at) == NULL) {
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

active_release_response_t *active_release_response_parseFromJSON(cJSON *active_release_responseJSON){

    active_release_response_t *active_release_response_local_var = NULL;

    // define the local variable for active_release_response->release_id
    long *release_id_local_var = NULL;

    char *name_local_str = NULL;

    char *state_local_str = NULL;

    char *promoted_at_local_str = NULL;

    // active_release_response->release_id
    cJSON *release_id = cJSON_GetObjectItemCaseSensitive(active_release_responseJSON, "release_id");
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

    // active_release_response->name
    cJSON *name = cJSON_GetObjectItemCaseSensitive(active_release_responseJSON, "name");
    if (cJSON_IsNull(name)) {
        name = NULL;
    }
    if (name) { 
    if(!cJSON_IsString(name) && !cJSON_IsNull(name))
    {
    goto end; //String
    }
    }

    // active_release_response->state
    cJSON *state = cJSON_GetObjectItemCaseSensitive(active_release_responseJSON, "state");
    if (cJSON_IsNull(state)) {
        state = NULL;
    }
    if (state) { 
    if(!cJSON_IsString(state) && !cJSON_IsNull(state))
    {
    goto end; //String
    }
    }

    // active_release_response->promoted_at
    cJSON *promoted_at = cJSON_GetObjectItemCaseSensitive(active_release_responseJSON, "promoted_at");
    if (cJSON_IsNull(promoted_at)) {
        promoted_at = NULL;
    }
    if (promoted_at) { 
    if(!cJSON_IsString(promoted_at) && !cJSON_IsNull(promoted_at))
    {
    goto end; //String
    }
    }


    if (name && !cJSON_IsNull(name)) name_local_str = strdup(name->valuestring);
    if (state && !cJSON_IsNull(state)) state_local_str = strdup(state->valuestring);
    if (promoted_at && !cJSON_IsNull(promoted_at)) promoted_at_local_str = strdup(promoted_at->valuestring);

    active_release_response_local_var = active_release_response_create_internal (
        release_id_local_var,
        name_local_str,
        state_local_str,
        promoted_at_local_str
        );

    if (!active_release_response_local_var) {
        goto end;
    }

    return active_release_response_local_var;
end:
    if (release_id_local_var) {
        free(release_id_local_var);
        release_id_local_var = NULL;
    }
    if (name_local_str) {
        free(name_local_str);
        name_local_str = NULL;
    }
    if (state_local_str) {
        free(state_local_str);
        state_local_str = NULL;
    }
    if (promoted_at_local_str) {
        free(promoted_at_local_str);
        promoted_at_local_str = NULL;
    }
    return NULL;

}
