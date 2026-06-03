#ifndef job_status_response_TEST
#define job_status_response_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define job_status_response_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/job_status_response.h"
job_status_response_t* instantiate_job_status_response(int include_optional);



job_status_response_t* instantiate_job_status_response(int include_optional) {
  job_status_response_t* job_status_response = NULL;
  if (include_optional) {
    job_status_response = job_status_response_create(
      56,
      "0",
      56,
      "0",
      aimee_kb_api_job_status_response_STATUS_pending,
      56,
      "0",
      "0",
      "0",
      "0",
      "0"
    );
  } else {
    job_status_response = job_status_response_create(
      56,
      "0",
      56,
      "0",
      aimee_kb_api_job_status_response_STATUS_pending,
      56,
      "0",
      "0",
      "0",
      "0",
      "0"
    );
  }

  return job_status_response;
}


#ifdef job_status_response_MAIN

void test_job_status_response(int include_optional) {
    job_status_response_t* job_status_response_1 = instantiate_job_status_response(include_optional);

	cJSON* jsonjob_status_response_1 = job_status_response_convertToJSON(job_status_response_1);
	printf("job_status_response :\n%s\n", cJSON_Print(jsonjob_status_response_1));
	job_status_response_t* job_status_response_2 = job_status_response_parseFromJSON(jsonjob_status_response_1);
	cJSON* jsonjob_status_response_2 = job_status_response_convertToJSON(job_status_response_2);
	printf("repeating job_status_response:\n%s\n", cJSON_Print(jsonjob_status_response_2));
}

int main() {
  test_job_status_response(1);
  test_job_status_response(0);

  printf("Hello world \n");
  return 0;
}

#endif // job_status_response_MAIN
#endif // job_status_response_TEST
