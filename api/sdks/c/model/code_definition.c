#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "code_definition.h"



static code_definition_t *code_definition_create_internal(
    char *name,
    char *kind,
    int *line
    ) {
    code_definition_t *code_definition_local_var = malloc(sizeof(code_definition_t));
    if (!code_definition_local_var) {
        return NULL;
    }
    memset(code_definition_local_var, 0, sizeof(code_definition_t));
    code_definition_local_var->_library_owned = 1;
    code_definition_local_var->name = name;
    code_definition_local_var->kind = kind;
    code_definition_local_var->line = line;
    return code_definition_local_var;
}

__attribute__((deprecated)) code_definition_t *code_definition_create(
    char *name,
    char *kind,
    int *line
    ) {
    int *line_copy = NULL;
    if (line) {
        line_copy = malloc(sizeof(int));
        if (line_copy) *line_copy = *line;
    }
    code_definition_t *result = code_definition_create_internal (
        name,
        kind,
        line_copy
        );
    if (!result) {
        free(line_copy);
    }
    return result;
}

void code_definition_free(code_definition_t *code_definition) {
    if(NULL == code_definition){
        return ;
    }
    if(code_definition->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "code_definition_free");
        return ;
    }
    listEntry_t *listEntry;
    if (code_definition->name) {
        free(code_definition->name);
        code_definition->name = NULL;
    }
    if (code_definition->kind) {
        free(code_definition->kind);
        code_definition->kind = NULL;
    }
    if (code_definition->line) {
        free(code_definition->line);
        code_definition->line = NULL;
    }
    free(code_definition);
}

cJSON *code_definition_convertToJSON(code_definition_t *code_definition) {
    cJSON *item = cJSON_CreateObject();

    // code_definition->name
    if(code_definition->name) {
    if(cJSON_AddStringToObject(item, "name", code_definition->name) == NULL) {
    goto fail; //String
    }
    }


    // code_definition->kind
    if(code_definition->kind) {
    if(cJSON_AddStringToObject(item, "kind", code_definition->kind) == NULL) {
    goto fail; //String
    }
    }


    // code_definition->line
    if(code_definition->line) {
    if(cJSON_AddNumberToObject(item, "line", *code_definition->line) == NULL) {
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

code_definition_t *code_definition_parseFromJSON(cJSON *code_definitionJSON){

    code_definition_t *code_definition_local_var = NULL;

    char *name_local_str = NULL;

    char *kind_local_str = NULL;

    // define the local variable for code_definition->line
    int *line_local_var = NULL;

    // code_definition->name
    cJSON *name = cJSON_GetObjectItemCaseSensitive(code_definitionJSON, "name");
    if (cJSON_IsNull(name)) {
        name = NULL;
    }
    if (name) { 
    if(!cJSON_IsString(name) && !cJSON_IsNull(name))
    {
    goto end; //String
    }
    }

    // code_definition->kind
    cJSON *kind = cJSON_GetObjectItemCaseSensitive(code_definitionJSON, "kind");
    if (cJSON_IsNull(kind)) {
        kind = NULL;
    }
    if (kind) { 
    if(!cJSON_IsString(kind) && !cJSON_IsNull(kind))
    {
    goto end; //String
    }
    }

    // code_definition->line
    cJSON *line = cJSON_GetObjectItemCaseSensitive(code_definitionJSON, "line");
    if (cJSON_IsNull(line)) {
        line = NULL;
    }
    if (line) { 
    if(!cJSON_IsNumber(line))
    {
    goto end; //Numeric
    }
    line_local_var = malloc(sizeof(int));
    if(!line_local_var)
    {
        goto end;
    }
    *line_local_var = line->valuedouble;
    }


    if (name && !cJSON_IsNull(name)) name_local_str = strdup(name->valuestring);
    if (kind && !cJSON_IsNull(kind)) kind_local_str = strdup(kind->valuestring);

    code_definition_local_var = code_definition_create_internal (
        name_local_str,
        kind_local_str,
        line_local_var
        );

    if (!code_definition_local_var) {
        goto end;
    }

    return code_definition_local_var;
end:
    if (name_local_str) {
        free(name_local_str);
        name_local_str = NULL;
    }
    if (kind_local_str) {
        free(kind_local_str);
        kind_local_str = NULL;
    }
    if (line_local_var) {
        free(line_local_var);
        line_local_var = NULL;
    }
    return NULL;

}
