#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "health_response.h"



static health_response_t *health_response_create_internal(
    char *status,
    int *db2_ok,
    int *db2_kb_tables_ok,
    int *pgvec_ok,
    int *pgvec_collection_ok,
    int *embed_ok,
    char *embed_command,
    int *chunk_count,
    int *embedding_count,
    list_t *warnings
    ) {
    health_response_t *health_response_local_var = malloc(sizeof(health_response_t));
    if (!health_response_local_var) {
        return NULL;
    }
    memset(health_response_local_var, 0, sizeof(health_response_t));
    health_response_local_var->_library_owned = 1;
    health_response_local_var->status = status;
    health_response_local_var->db2_ok = db2_ok;
    health_response_local_var->db2_kb_tables_ok = db2_kb_tables_ok;
    health_response_local_var->pgvec_ok = pgvec_ok;
    health_response_local_var->pgvec_collection_ok = pgvec_collection_ok;
    health_response_local_var->embed_ok = embed_ok;
    health_response_local_var->embed_command = embed_command;
    health_response_local_var->chunk_count = chunk_count;
    health_response_local_var->embedding_count = embedding_count;
    health_response_local_var->warnings = warnings;
    return health_response_local_var;
}

__attribute__((deprecated)) health_response_t *health_response_create(
    char *status,
    int *db2_ok,
    int *db2_kb_tables_ok,
    int *pgvec_ok,
    int *pgvec_collection_ok,
    int *embed_ok,
    char *embed_command,
    int *chunk_count,
    int *embedding_count,
    list_t *warnings
    ) {
    int *db2_ok_copy = NULL;
    if (db2_ok) {
        db2_ok_copy = malloc(sizeof(int));
        if (db2_ok_copy) *db2_ok_copy = *db2_ok;
    }
    int *db2_kb_tables_ok_copy = NULL;
    if (db2_kb_tables_ok) {
        db2_kb_tables_ok_copy = malloc(sizeof(int));
        if (db2_kb_tables_ok_copy) *db2_kb_tables_ok_copy = *db2_kb_tables_ok;
    }
    int *pgvec_ok_copy = NULL;
    if (pgvec_ok) {
        pgvec_ok_copy = malloc(sizeof(int));
        if (pgvec_ok_copy) *pgvec_ok_copy = *pgvec_ok;
    }
    int *pgvec_collection_ok_copy = NULL;
    if (pgvec_collection_ok) {
        pgvec_collection_ok_copy = malloc(sizeof(int));
        if (pgvec_collection_ok_copy) *pgvec_collection_ok_copy = *pgvec_collection_ok;
    }
    int *embed_ok_copy = NULL;
    if (embed_ok) {
        embed_ok_copy = malloc(sizeof(int));
        if (embed_ok_copy) *embed_ok_copy = *embed_ok;
    }
    int *chunk_count_copy = NULL;
    if (chunk_count) {
        chunk_count_copy = malloc(sizeof(int));
        if (chunk_count_copy) *chunk_count_copy = *chunk_count;
    }
    int *embedding_count_copy = NULL;
    if (embedding_count) {
        embedding_count_copy = malloc(sizeof(int));
        if (embedding_count_copy) *embedding_count_copy = *embedding_count;
    }
    health_response_t *result = health_response_create_internal (
        status,
        db2_ok_copy,
        db2_kb_tables_ok_copy,
        pgvec_ok_copy,
        pgvec_collection_ok_copy,
        embed_ok_copy,
        embed_command,
        chunk_count_copy,
        embedding_count_copy,
        warnings
        );
    if (!result) {
        free(db2_ok_copy);
        free(db2_kb_tables_ok_copy);
        free(pgvec_ok_copy);
        free(pgvec_collection_ok_copy);
        free(embed_ok_copy);
        free(chunk_count_copy);
        free(embedding_count_copy);
    }
    return result;
}

void health_response_free(health_response_t *health_response) {
    if(NULL == health_response){
        return ;
    }
    if(health_response->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "health_response_free");
        return ;
    }
    listEntry_t *listEntry;
    if (health_response->status) {
        free(health_response->status);
        health_response->status = NULL;
    }
    if (health_response->db2_ok) {
        free(health_response->db2_ok);
        health_response->db2_ok = NULL;
    }
    if (health_response->db2_kb_tables_ok) {
        free(health_response->db2_kb_tables_ok);
        health_response->db2_kb_tables_ok = NULL;
    }
    if (health_response->pgvec_ok) {
        free(health_response->pgvec_ok);
        health_response->pgvec_ok = NULL;
    }
    if (health_response->pgvec_collection_ok) {
        free(health_response->pgvec_collection_ok);
        health_response->pgvec_collection_ok = NULL;
    }
    if (health_response->embed_ok) {
        free(health_response->embed_ok);
        health_response->embed_ok = NULL;
    }
    if (health_response->embed_command) {
        free(health_response->embed_command);
        health_response->embed_command = NULL;
    }
    if (health_response->chunk_count) {
        free(health_response->chunk_count);
        health_response->chunk_count = NULL;
    }
    if (health_response->embedding_count) {
        free(health_response->embedding_count);
        health_response->embedding_count = NULL;
    }
    if (health_response->warnings) {
        list_ForEach(listEntry, health_response->warnings) {
            free(listEntry->data);
        }
        list_freeList(health_response->warnings);
        health_response->warnings = NULL;
    }
    free(health_response);
}

cJSON *health_response_convertToJSON(health_response_t *health_response) {
    cJSON *item = cJSON_CreateObject();

    // health_response->status
    if (!health_response->status) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "status", health_response->status) == NULL) {
    goto fail; //String
    }


    // health_response->db2_ok
    if(health_response->db2_ok) {
    if(cJSON_AddBoolToObject(item, "db2_ok", *health_response->db2_ok) == NULL) {
    goto fail; //Bool
    }
    }


    // health_response->db2_kb_tables_ok
    if(health_response->db2_kb_tables_ok) {
    if(cJSON_AddBoolToObject(item, "db2_kb_tables_ok", *health_response->db2_kb_tables_ok) == NULL) {
    goto fail; //Bool
    }
    }


    // health_response->pgvec_ok
    if(health_response->pgvec_ok) {
    if(cJSON_AddBoolToObject(item, "pgvec_ok", *health_response->pgvec_ok) == NULL) {
    goto fail; //Bool
    }
    }


    // health_response->pgvec_collection_ok
    if(health_response->pgvec_collection_ok) {
    if(cJSON_AddBoolToObject(item, "pgvec_collection_ok", *health_response->pgvec_collection_ok) == NULL) {
    goto fail; //Bool
    }
    }


    // health_response->embed_ok
    if(health_response->embed_ok) {
    if(cJSON_AddBoolToObject(item, "embed_ok", *health_response->embed_ok) == NULL) {
    goto fail; //Bool
    }
    }


    // health_response->embed_command
    if(health_response->embed_command) {
    if(cJSON_AddStringToObject(item, "embed_command", health_response->embed_command) == NULL) {
    goto fail; //String
    }
    }


    // health_response->chunk_count
    if(health_response->chunk_count) {
    if(cJSON_AddNumberToObject(item, "chunk_count", *health_response->chunk_count) == NULL) {
    goto fail; //Numeric
    }
    }


    // health_response->embedding_count
    if(health_response->embedding_count) {
    if(cJSON_AddNumberToObject(item, "embedding_count", *health_response->embedding_count) == NULL) {
    goto fail; //Numeric
    }
    }


    // health_response->warnings
    if(health_response->warnings) {
    cJSON *warnings = cJSON_AddArrayToObject(item, "warnings");
    if(warnings == NULL) {
        goto fail; //primitive container
    }

    listEntry_t *warningsListEntry;
    list_ForEach(warningsListEntry, health_response->warnings) {
    if(cJSON_AddStringToObject(warnings, "", warningsListEntry->data) == NULL)
    {
        goto fail;
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

health_response_t *health_response_parseFromJSON(cJSON *health_responseJSON){

    health_response_t *health_response_local_var = NULL;

    char *status_local_str = NULL;

    // define the local variable for health_response->db2_ok
    int *db2_ok_local_var = NULL;

    // define the local variable for health_response->db2_kb_tables_ok
    int *db2_kb_tables_ok_local_var = NULL;

    // define the local variable for health_response->pgvec_ok
    int *pgvec_ok_local_var = NULL;

    // define the local variable for health_response->pgvec_collection_ok
    int *pgvec_collection_ok_local_var = NULL;

    // define the local variable for health_response->embed_ok
    int *embed_ok_local_var = NULL;

    char *embed_command_local_str = NULL;

    // define the local variable for health_response->chunk_count
    int *chunk_count_local_var = NULL;

    // define the local variable for health_response->embedding_count
    int *embedding_count_local_var = NULL;

    // define the local list for health_response->warnings
    list_t *warningsList = NULL;

    // health_response->status
    cJSON *status = cJSON_GetObjectItemCaseSensitive(health_responseJSON, "status");
    if (cJSON_IsNull(status)) {
        status = NULL;
    }
    if (!status) {
        goto end;
    }

    
    if(!cJSON_IsString(status))
    {
    goto end; //String
    }

    // health_response->db2_ok
    cJSON *db2_ok = cJSON_GetObjectItemCaseSensitive(health_responseJSON, "db2_ok");
    if (cJSON_IsNull(db2_ok)) {
        db2_ok = NULL;
    }
    if (db2_ok) { 
    if(!cJSON_IsBool(db2_ok))
    {
    goto end; //Bool
    }
    db2_ok_local_var = malloc(sizeof(int));
    if(!db2_ok_local_var)
    {
        goto end;
    }
    *db2_ok_local_var = db2_ok->valueint;
    }

    // health_response->db2_kb_tables_ok
    cJSON *db2_kb_tables_ok = cJSON_GetObjectItemCaseSensitive(health_responseJSON, "db2_kb_tables_ok");
    if (cJSON_IsNull(db2_kb_tables_ok)) {
        db2_kb_tables_ok = NULL;
    }
    if (db2_kb_tables_ok) { 
    if(!cJSON_IsBool(db2_kb_tables_ok))
    {
    goto end; //Bool
    }
    db2_kb_tables_ok_local_var = malloc(sizeof(int));
    if(!db2_kb_tables_ok_local_var)
    {
        goto end;
    }
    *db2_kb_tables_ok_local_var = db2_kb_tables_ok->valueint;
    }

    // health_response->pgvec_ok
    cJSON *pgvec_ok = cJSON_GetObjectItemCaseSensitive(health_responseJSON, "pgvec_ok");
    if (cJSON_IsNull(pgvec_ok)) {
        pgvec_ok = NULL;
    }
    if (pgvec_ok) { 
    if(!cJSON_IsBool(pgvec_ok))
    {
    goto end; //Bool
    }
    pgvec_ok_local_var = malloc(sizeof(int));
    if(!pgvec_ok_local_var)
    {
        goto end;
    }
    *pgvec_ok_local_var = pgvec_ok->valueint;
    }

    // health_response->pgvec_collection_ok
    cJSON *pgvec_collection_ok = cJSON_GetObjectItemCaseSensitive(health_responseJSON, "pgvec_collection_ok");
    if (cJSON_IsNull(pgvec_collection_ok)) {
        pgvec_collection_ok = NULL;
    }
    if (pgvec_collection_ok) { 
    if(!cJSON_IsBool(pgvec_collection_ok))
    {
    goto end; //Bool
    }
    pgvec_collection_ok_local_var = malloc(sizeof(int));
    if(!pgvec_collection_ok_local_var)
    {
        goto end;
    }
    *pgvec_collection_ok_local_var = pgvec_collection_ok->valueint;
    }

    // health_response->embed_ok
    cJSON *embed_ok = cJSON_GetObjectItemCaseSensitive(health_responseJSON, "embed_ok");
    if (cJSON_IsNull(embed_ok)) {
        embed_ok = NULL;
    }
    if (embed_ok) { 
    if(!cJSON_IsBool(embed_ok))
    {
    goto end; //Bool
    }
    embed_ok_local_var = malloc(sizeof(int));
    if(!embed_ok_local_var)
    {
        goto end;
    }
    *embed_ok_local_var = embed_ok->valueint;
    }

    // health_response->embed_command
    cJSON *embed_command = cJSON_GetObjectItemCaseSensitive(health_responseJSON, "embed_command");
    if (cJSON_IsNull(embed_command)) {
        embed_command = NULL;
    }
    if (embed_command) { 
    if(!cJSON_IsString(embed_command) && !cJSON_IsNull(embed_command))
    {
    goto end; //String
    }
    }

    // health_response->chunk_count
    cJSON *chunk_count = cJSON_GetObjectItemCaseSensitive(health_responseJSON, "chunk_count");
    if (cJSON_IsNull(chunk_count)) {
        chunk_count = NULL;
    }
    if (chunk_count) { 
    if(!cJSON_IsNumber(chunk_count))
    {
    goto end; //Numeric
    }
    chunk_count_local_var = malloc(sizeof(int));
    if(!chunk_count_local_var)
    {
        goto end;
    }
    *chunk_count_local_var = chunk_count->valuedouble;
    }

    // health_response->embedding_count
    cJSON *embedding_count = cJSON_GetObjectItemCaseSensitive(health_responseJSON, "embedding_count");
    if (cJSON_IsNull(embedding_count)) {
        embedding_count = NULL;
    }
    if (embedding_count) { 
    if(!cJSON_IsNumber(embedding_count))
    {
    goto end; //Numeric
    }
    embedding_count_local_var = malloc(sizeof(int));
    if(!embedding_count_local_var)
    {
        goto end;
    }
    *embedding_count_local_var = embedding_count->valuedouble;
    }

    // health_response->warnings
    cJSON *warnings = cJSON_GetObjectItemCaseSensitive(health_responseJSON, "warnings");
    if (cJSON_IsNull(warnings)) {
        warnings = NULL;
    }
    if (warnings) { 
    cJSON *warnings_local = NULL;
    if(!cJSON_IsArray(warnings)) {
        goto end;//primitive container
    }
    warningsList = list_createList();

    cJSON_ArrayForEach(warnings_local, warnings)
    {
        if(!cJSON_IsString(warnings_local))
        {
            goto end;
        }
        list_addElement(warningsList , strdup(warnings_local->valuestring));
    }
    }


    if (status && !cJSON_IsNull(status)) status_local_str = strdup(status->valuestring);
    if (embed_command && !cJSON_IsNull(embed_command)) embed_command_local_str = strdup(embed_command->valuestring);

    health_response_local_var = health_response_create_internal (
        status_local_str,
        db2_ok_local_var,
        db2_kb_tables_ok_local_var,
        pgvec_ok_local_var,
        pgvec_collection_ok_local_var,
        embed_ok_local_var,
        embed_command_local_str,
        chunk_count_local_var,
        embedding_count_local_var,
        warnings ? warningsList : NULL
        );

    if (!health_response_local_var) {
        goto end;
    }

    return health_response_local_var;
end:
    if (status_local_str) {
        free(status_local_str);
        status_local_str = NULL;
    }
    if (db2_ok_local_var) {
        free(db2_ok_local_var);
        db2_ok_local_var = NULL;
    }
    if (db2_kb_tables_ok_local_var) {
        free(db2_kb_tables_ok_local_var);
        db2_kb_tables_ok_local_var = NULL;
    }
    if (pgvec_ok_local_var) {
        free(pgvec_ok_local_var);
        pgvec_ok_local_var = NULL;
    }
    if (pgvec_collection_ok_local_var) {
        free(pgvec_collection_ok_local_var);
        pgvec_collection_ok_local_var = NULL;
    }
    if (embed_ok_local_var) {
        free(embed_ok_local_var);
        embed_ok_local_var = NULL;
    }
    if (embed_command_local_str) {
        free(embed_command_local_str);
        embed_command_local_str = NULL;
    }
    if (chunk_count_local_var) {
        free(chunk_count_local_var);
        chunk_count_local_var = NULL;
    }
    if (embedding_count_local_var) {
        free(embedding_count_local_var);
        embedding_count_local_var = NULL;
    }
    if (warningsList) {
        listEntry_t *listEntry = NULL;
        list_ForEach(listEntry, warningsList) {
            free(listEntry->data);
            listEntry->data = NULL;
        }
        list_freeList(warningsList);
        warningsList = NULL;
    }
    return NULL;

}
