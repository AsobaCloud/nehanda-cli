#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "code_project_stats_response.h"



static code_project_stats_response_t *code_project_stats_response_create_internal(
    char *status,
    char *project,
    int *files,
    int *definitions,
    list_t *langs
    ) {
    code_project_stats_response_t *code_project_stats_response_local_var = malloc(sizeof(code_project_stats_response_t));
    if (!code_project_stats_response_local_var) {
        return NULL;
    }
    memset(code_project_stats_response_local_var, 0, sizeof(code_project_stats_response_t));
    code_project_stats_response_local_var->_library_owned = 1;
    code_project_stats_response_local_var->status = status;
    code_project_stats_response_local_var->project = project;
    code_project_stats_response_local_var->files = files;
    code_project_stats_response_local_var->definitions = definitions;
    code_project_stats_response_local_var->langs = langs;
    return code_project_stats_response_local_var;
}

__attribute__((deprecated)) code_project_stats_response_t *code_project_stats_response_create(
    char *status,
    char *project,
    int *files,
    int *definitions,
    list_t *langs
    ) {
    int *files_copy = NULL;
    if (files) {
        files_copy = malloc(sizeof(int));
        if (files_copy) *files_copy = *files;
    }
    int *definitions_copy = NULL;
    if (definitions) {
        definitions_copy = malloc(sizeof(int));
        if (definitions_copy) *definitions_copy = *definitions;
    }
    code_project_stats_response_t *result = code_project_stats_response_create_internal (
        status,
        project,
        files_copy,
        definitions_copy,
        langs
        );
    if (!result) {
        free(files_copy);
        free(definitions_copy);
    }
    return result;
}

void code_project_stats_response_free(code_project_stats_response_t *code_project_stats_response) {
    if(NULL == code_project_stats_response){
        return ;
    }
    if(code_project_stats_response->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "code_project_stats_response_free");
        return ;
    }
    listEntry_t *listEntry;
    if (code_project_stats_response->status) {
        free(code_project_stats_response->status);
        code_project_stats_response->status = NULL;
    }
    if (code_project_stats_response->project) {
        free(code_project_stats_response->project);
        code_project_stats_response->project = NULL;
    }
    if (code_project_stats_response->files) {
        free(code_project_stats_response->files);
        code_project_stats_response->files = NULL;
    }
    if (code_project_stats_response->definitions) {
        free(code_project_stats_response->definitions);
        code_project_stats_response->definitions = NULL;
    }
    if (code_project_stats_response->langs) {
        list_ForEach(listEntry, code_project_stats_response->langs) {
            code_project_language_free(listEntry->data);
        }
        list_freeList(code_project_stats_response->langs);
        code_project_stats_response->langs = NULL;
    }
    free(code_project_stats_response);
}

cJSON *code_project_stats_response_convertToJSON(code_project_stats_response_t *code_project_stats_response) {
    cJSON *item = cJSON_CreateObject();

    // code_project_stats_response->status
    if(code_project_stats_response->status) {
    if(cJSON_AddStringToObject(item, "status", code_project_stats_response->status) == NULL) {
    goto fail; //String
    }
    }


    // code_project_stats_response->project
    if(code_project_stats_response->project) {
    if(cJSON_AddStringToObject(item, "project", code_project_stats_response->project) == NULL) {
    goto fail; //String
    }
    }


    // code_project_stats_response->files
    if(code_project_stats_response->files) {
    if(cJSON_AddNumberToObject(item, "files", *code_project_stats_response->files) == NULL) {
    goto fail; //Numeric
    }
    }


    // code_project_stats_response->definitions
    if(code_project_stats_response->definitions) {
    if(cJSON_AddNumberToObject(item, "definitions", *code_project_stats_response->definitions) == NULL) {
    goto fail; //Numeric
    }
    }


    // code_project_stats_response->langs
    if(code_project_stats_response->langs) {
    cJSON *langs = cJSON_AddArrayToObject(item, "langs");
    if(langs == NULL) {
    goto fail; //nonprimitive container
    }

    listEntry_t *langsListEntry;
    if (code_project_stats_response->langs) {
    list_ForEach(langsListEntry, code_project_stats_response->langs) {
    cJSON *itemLocal = code_project_language_convertToJSON(langsListEntry->data);
    if(itemLocal == NULL) {
    goto fail;
    }
    cJSON_AddItemToArray(langs, itemLocal);
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

code_project_stats_response_t *code_project_stats_response_parseFromJSON(cJSON *code_project_stats_responseJSON){

    code_project_stats_response_t *code_project_stats_response_local_var = NULL;

    char *status_local_str = NULL;

    char *project_local_str = NULL;

    // define the local variable for code_project_stats_response->files
    int *files_local_var = NULL;

    // define the local variable for code_project_stats_response->definitions
    int *definitions_local_var = NULL;

    // define the local list for code_project_stats_response->langs
    list_t *langsList = NULL;

    // code_project_stats_response->status
    cJSON *status = cJSON_GetObjectItemCaseSensitive(code_project_stats_responseJSON, "status");
    if (cJSON_IsNull(status)) {
        status = NULL;
    }
    if (status) { 
    if(!cJSON_IsString(status) && !cJSON_IsNull(status))
    {
    goto end; //String
    }
    }

    // code_project_stats_response->project
    cJSON *project = cJSON_GetObjectItemCaseSensitive(code_project_stats_responseJSON, "project");
    if (cJSON_IsNull(project)) {
        project = NULL;
    }
    if (project) { 
    if(!cJSON_IsString(project) && !cJSON_IsNull(project))
    {
    goto end; //String
    }
    }

    // code_project_stats_response->files
    cJSON *files = cJSON_GetObjectItemCaseSensitive(code_project_stats_responseJSON, "files");
    if (cJSON_IsNull(files)) {
        files = NULL;
    }
    if (files) { 
    if(!cJSON_IsNumber(files))
    {
    goto end; //Numeric
    }
    files_local_var = malloc(sizeof(int));
    if(!files_local_var)
    {
        goto end;
    }
    *files_local_var = files->valuedouble;
    }

    // code_project_stats_response->definitions
    cJSON *definitions = cJSON_GetObjectItemCaseSensitive(code_project_stats_responseJSON, "definitions");
    if (cJSON_IsNull(definitions)) {
        definitions = NULL;
    }
    if (definitions) { 
    if(!cJSON_IsNumber(definitions))
    {
    goto end; //Numeric
    }
    definitions_local_var = malloc(sizeof(int));
    if(!definitions_local_var)
    {
        goto end;
    }
    *definitions_local_var = definitions->valuedouble;
    }

    // code_project_stats_response->langs
    cJSON *langs = cJSON_GetObjectItemCaseSensitive(code_project_stats_responseJSON, "langs");
    if (cJSON_IsNull(langs)) {
        langs = NULL;
    }
    if (langs) { 
    cJSON *langs_local_nonprimitive = NULL;
    if(!cJSON_IsArray(langs)){
        goto end; //nonprimitive container
    }

    langsList = list_createList();

    cJSON_ArrayForEach(langs_local_nonprimitive,langs )
    {
        if(!cJSON_IsObject(langs_local_nonprimitive)){
            goto end;
        }
        code_project_language_t *langsItem = code_project_language_parseFromJSON(langs_local_nonprimitive);

        list_addElement(langsList, langsItem);
    }
    }


    if (status && !cJSON_IsNull(status)) status_local_str = strdup(status->valuestring);
    if (project && !cJSON_IsNull(project)) project_local_str = strdup(project->valuestring);

    code_project_stats_response_local_var = code_project_stats_response_create_internal (
        status_local_str,
        project_local_str,
        files_local_var,
        definitions_local_var,
        langs ? langsList : NULL
        );

    if (!code_project_stats_response_local_var) {
        goto end;
    }

    return code_project_stats_response_local_var;
end:
    if (status_local_str) {
        free(status_local_str);
        status_local_str = NULL;
    }
    if (project_local_str) {
        free(project_local_str);
        project_local_str = NULL;
    }
    if (files_local_var) {
        free(files_local_var);
        files_local_var = NULL;
    }
    if (definitions_local_var) {
        free(definitions_local_var);
        definitions_local_var = NULL;
    }
    if (langsList) {
        listEntry_t *listEntry = NULL;
        list_ForEach(listEntry, langsList) {
            code_project_language_free(listEntry->data);
            listEntry->data = NULL;
        }
        list_freeList(langsList);
        langsList = NULL;
    }
    return NULL;

}
