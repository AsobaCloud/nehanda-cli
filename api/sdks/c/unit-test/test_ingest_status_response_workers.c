#ifndef ingest_status_response_workers_TEST
#define ingest_status_response_workers_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define ingest_status_response_workers_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/ingest_status_response_workers.h"
ingest_status_response_workers_t* instantiate_ingest_status_response_workers(int include_optional);



ingest_status_response_workers_t* instantiate_ingest_status_response_workers(int include_optional) {
  ingest_status_response_workers_t* ingest_status_response_workers = NULL;
  if (include_optional) {
    ingest_status_response_workers = ingest_status_response_workers_create(
      56,
      56
    );
  } else {
    ingest_status_response_workers = ingest_status_response_workers_create(
      56,
      56
    );
  }

  return ingest_status_response_workers;
}


#ifdef ingest_status_response_workers_MAIN

void test_ingest_status_response_workers(int include_optional) {
    ingest_status_response_workers_t* ingest_status_response_workers_1 = instantiate_ingest_status_response_workers(include_optional);

	cJSON* jsoningest_status_response_workers_1 = ingest_status_response_workers_convertToJSON(ingest_status_response_workers_1);
	printf("ingest_status_response_workers :\n%s\n", cJSON_Print(jsoningest_status_response_workers_1));
	ingest_status_response_workers_t* ingest_status_response_workers_2 = ingest_status_response_workers_parseFromJSON(jsoningest_status_response_workers_1);
	cJSON* jsoningest_status_response_workers_2 = ingest_status_response_workers_convertToJSON(ingest_status_response_workers_2);
	printf("repeating ingest_status_response_workers:\n%s\n", cJSON_Print(jsoningest_status_response_workers_2));
}

int main() {
  test_ingest_status_response_workers(1);
  test_ingest_status_response_workers(0);

  printf("Hello world \n");
  return 0;
}

#endif // ingest_status_response_workers_MAIN
#endif // ingest_status_response_workers_TEST
