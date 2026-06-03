#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "code_structure_response.h"



static code_structure_response_t *code_structure_response_create_internal(
    char *status,
    list_t *definitions
    ) {
    code_structure_response_t *code_structure_response_local_var = malloc(sizeof(code_structure_response_t));
    if (!code_structure_response_local_var) {
        return NULL;
    }
    memset(code_structure_response_local_var, 0, sizeof(code_structure_response_t));
    code_structure_response_local_var->_library_owned = 1;
    code_structure_response_local_var->status = status;
    code_structure_response_local_var->definitions = definitions;
    return code_structure_response_local_var;
}

__attribute__((deprecated)) code_structure_response_t *code_structure_response_create(
    char *status,
    list_t *definitions
    ) {
    code_structure_response_t *result = code_structure_response_create_internal (
        status,
        definitions
        );
    if (!result) {
    }
    return result;
}

void code_structure_response_free(code_structure_response_t *code_structure_response) {
    if(NULL == code_structure_response){
        return ;
    }
    if(code_structure_response->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "code_structure_response_free");
        return ;
    }
    listEntry_t *listEntry;
    if (code_structure_response->status) {
        free(code_structure_response->status);
        code_structure_response->status = NULL;
    }
    if (code_structure_response->definitions) {
        list_ForEach(listEntry, code_structure_response->definitions) {
            code_definition_free(listEntry->data);
        }
        list_freeList(code_structure_response->definitions);
        code_structure_response->definitions = NULL;
    }
    free(code_structure_response);
}

cJSON *code_structure_response_convertToJSON(code_structure_response_t *code_structure_response) {
    cJSON *item = cJSON_CreateObject();

    // code_structure_response->status
    if(code_structure_response->status) {
    if(cJSON_AddStringToObject(item, "status", code_structure_response->status) == NULL) {
    goto fail; //String
    }
    }


    // code_structure_response->definitions
    if(code_structure_response->definitions) {
    cJSON *definitions = cJSON_AddArrayToObject(item, "definitions");
    if(definitions == NULL) {
    goto fail; //nonprimitive container
    }

    listEntry_t *definitionsListEntry;
    if (code_structure_response->definitions) {
    list_ForEach(definitionsListEntry, code_structure_response->definitions) {
    cJSON *itemLocal = code_definition_convertToJSON(definitionsListEntry->data);
    if(itemLocal == NULL) {
    goto fail;
    }
    cJSON_AddItemToArray(definitions, itemLocal);
    }
    }
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

code_structure_response_t *code_structure_response_parseFromJSON(cJSON *code_structure_responseJSON){

    code_structure_response_t *code_structure_response_local_var = NULL;

    char *status_local_str = NULL;

    // define the local list for code_structure_response->definitions
    list_t *definitionsList = NULL;

    // code_structure_response->status
    cJSON *status = cJSON_GetObjectItemCaseSensitive(code_structure_responseJSON, "status");
    if (cJSON_IsNull(status)) {
        status = NULL;
    }
    if (status) { 
    if(!cJSON_IsString(status) && !cJSON_IsNull(status))
    {
    goto end; //String
    }
    }

    // code_structure_response->definitions
    cJSON *definitions = cJSON_GetObjectItemCaseSensitive(code_structure_responseJSON, "definitions");
    if (cJSON_IsNull(definitions)) {
        definitions = NULL;
    }
    if (definitions) { 
    cJSON *definitions_local_nonprimitive = NULL;
    if(!cJSON_IsArray(definitions)){
        goto end; //nonprimitive container
    }

    definitionsList = list_createList();

    cJSON_ArrayForEach(definitions_local_nonprimitive,definitions )
    {
        if(!cJSON_IsObject(definitions_local_nonprimitive)){
            goto end;
        }
        code_definition_t *definitionsItem = code_definition_parseFromJSON(definitions_local_nonprimitive);

        list_addElement(definitionsList, definitionsItem);
    }
    }


    if (status && !cJSON_IsNull(status)) status_local_str = strdup(status->valuestring);

    code_structure_response_local_var = code_structure_response_create_internal (
        status_local_str,
        definitions ? definitionsList : NULL
        );

    if (!code_structure_response_local_var) {
        goto end;
    }

    return code_structure_response_local_var;
end:
    if (status_local_str) {
        free(status_local_str);
        status_local_str = NULL;
    }
    if (definitionsList) {
        listEntry_t *listEntry = NULL;
        list_ForEach(listEntry, definitionsList) {
            code_definition_free(listEntry->data);
            listEntry->data = NULL;
        }
        list_freeList(definitionsList);
        definitionsList = NULL;
    }
    return NULL;

}
