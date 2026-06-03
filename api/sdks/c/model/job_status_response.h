/*
 * job_status_response.h
 *
 * 
 */

#ifndef _job_status_response_H_
#define _job_status_response_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct job_status_response_t job_status_response_t;


// Enum STATUS for job_status_response

typedef enum  { aimee_kb_api_job_status_response_STATUS_NULL = 0, aimee_kb_api_job_status_response_STATUS_pending, aimee_kb_api_job_status_response_STATUS_running, aimee_kb_api_job_status_response_STATUS_done, aimee_kb_api_job_status_response_STATUS_failed } aimee_kb_api_job_status_response_STATUS_e;

char* job_status_response_status_ToString(aimee_kb_api_job_status_response_STATUS_e status);

aimee_kb_api_job_status_response_STATUS_e job_status_response_status_FromString(char* status);



typedef struct job_status_response_t {
    long *id; //numeric
    char *kind; // string
    long *document_id; //numeric
    char *project; // string
    aimee_kb_api_job_status_response_STATUS_e status; //enum
    int *attempts; //numeric
    char *last_error; // string
    char *claimed_by; // string
    char *claimed_at; // string
    char *created_at; // string
    char *updated_at; // string

    int _library_owned; // Is the library responsible for freeing this object?
} job_status_response_t;

__attribute__((deprecated)) job_status_response_t *job_status_response_create(
    long *id,
    char *kind,
    long *document_id,
    char *project,
    aimee_kb_api_job_status_response_STATUS_e status,
    int *attempts,
    char *last_error,
    char *claimed_by,
    char *claimed_at,
    char *created_at,
    char *updated_at
);

void job_status_response_free(job_status_response_t *job_status_response);

job_status_response_t *job_status_response_parseFromJSON(cJSON *job_status_responseJSON);

cJSON *job_status_response_convertToJSON(job_status_response_t *job_status_response);

#endif /* _job_status_response_H_ */

