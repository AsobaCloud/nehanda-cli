#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "code_project_language.h"



static code_project_language_t *code_project_language_create_internal(
    char *lang,
    int *count
    ) {
    code_project_language_t *code_project_language_local_var = malloc(sizeof(code_project_language_t));
    if (!code_project_language_local_var) {
        return NULL;
    }
    memset(code_project_language_local_var, 0, sizeof(code_project_language_t));
    code_project_language_local_var->_library_owned = 1;
    code_project_language_local_var->lang = lang;
    code_project_language_local_var->count = count;
    return code_project_language_local_var;
}

__attribute__((deprecated)) code_project_language_t *code_project_language_create(
    char *lang,
    int *count
    ) {
    int *count_copy = NULL;
    if (count) {
        count_copy = malloc(sizeof(int));
        if (count_copy) *count_copy = *count;
    }
    code_project_language_t *result = code_project_language_create_internal (
        lang,
        count_copy
        );
    if (!result) {
        free(count_copy);
    }
    return result;
}

void code_project_language_free(code_project_language_t *code_project_language) {
    if(NULL == code_project_language){
        return ;
    }
    if(code_project_language->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "code_project_language_free");
        return ;
    }
    listEntry_t *listEntry;
    if (code_project_language->lang) {
        free(code_project_language->lang);
        code_project_language->lang = NULL;
    }
    if (code_project_language->count) {
        free(code_project_language->count);
        code_project_language->count = NULL;
    }
    free(code_project_language);
}

cJSON *code_project_language_convertToJSON(code_project_language_t *code_project_language) {
    cJSON *item = cJSON_CreateObject();

    // code_project_language->lang
    if(code_project_language->lang) {
    if(cJSON_AddStringToObject(item, "lang", code_project_language->lang) == NULL) {
    goto fail; //String
    }
    }


    // code_project_language->count
    if(code_project_language->count) {
    if(cJSON_AddNumberToObject(item, "count", *code_project_language->count) == NULL) {
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

code_project_language_t *code_project_language_parseFromJSON(cJSON *code_project_languageJSON){

    code_project_language_t *code_project_language_local_var = NULL;

    char *lang_local_str = NULL;

    // define the local variable for code_project_language->count
    int *count_local_var = NULL;

    // code_project_language->lang
    cJSON *lang = cJSON_GetObjectItemCaseSensitive(code_project_languageJSON, "lang");
    if (cJSON_IsNull(lang)) {
        lang = NULL;
    }
    if (lang) { 
    if(!cJSON_IsString(lang) && !cJSON_IsNull(lang))
    {
    goto end; //String
    }
    }

    // code_project_language->count
    cJSON *count = cJSON_GetObjectItemCaseSensitive(code_project_languageJSON, "count");
    if (cJSON_IsNull(count)) {
        count = NULL;
    }
    if (count) { 
    if(!cJSON_IsNumber(count))
    {
    goto end; //Numeric
    }
    count_local_var = malloc(sizeof(int));
    if(!count_local_var)
    {
        goto end;
    }
    *count_local_var = count->valuedouble;
    }


    if (lang && !cJSON_IsNull(lang)) lang_local_str = strdup(lang->valuestring);

    code_project_language_local_var = code_project_language_create_internal (
        lang_local_str,
        count_local_var
        );

    if (!code_project_language_local_var) {
        goto end;
    }

    return code_project_language_local_var;
end:
    if (lang_local_str) {
        free(lang_local_str);
        lang_local_str = NULL;
    }
    if (count_local_var) {
        free(count_local_var);
        count_local_var = NULL;
    }
    return NULL;

}
