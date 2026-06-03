#ifndef ingest_status_response_TEST
#define ingest_status_response_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define ingest_status_response_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/ingest_status_response.h"
ingest_status_response_t* instantiate_ingest_status_response(int include_optional);

#include "test_ingest_status_response_queue.c"
#include "test_ingest_status_response_workers.c"


ingest_status_response_t* instantiate_ingest_status_response(int include_optional) {
  ingest_status_response_t* ingest_status_response = NULL;
  if (include_optional) {
    ingest_status_response = ingest_status_response_create(
      "ok",
       // false, not to have infinite recursion
      instantiate_ingest_status_response_queue(0),
       // false, not to have infinite recursion
      instantiate_ingest_status_response_workers(0),
      list_createList()
    );
  } else {
    ingest_status_response = ingest_status_response_create(
      "ok",
      NULL,
      NULL,
      list_createList()
    );
  }

  return ingest_status_response;
}


#ifdef ingest_status_response_MAIN

void test_ingest_status_response(int include_optional) {
    ingest_status_response_t* ingest_status_response_1 = instantiate_ingest_status_response(include_optional);

	cJSON* jsoningest_status_response_1 = ingest_status_response_convertToJSON(ingest_status_response_1);
	printf("ingest_status_response :\n%s\n", cJSON_Print(jsoningest_status_response_1));
	ingest_status_response_t* ingest_status_response_2 = ingest_status_response_parseFromJSON(jsoningest_status_response_1);
	cJSON* jsoningest_status_response_2 = ingest_status_response_convertToJSON(ingest_status_response_2);
	printf("repeating ingest_status_response:\n%s\n", cJSON_Print(jsoningest_status_response_2));
}

int main() {
  test_ingest_status_response(1);
  test_ingest_status_response(0);

  printf("Hello world \n");
  return 0;
}

#endif // ingest_status_response_MAIN
#endif // ingest_status_response_TEST
