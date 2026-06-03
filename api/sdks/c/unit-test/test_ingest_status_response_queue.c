#ifndef ingest_status_response_queue_TEST
#define ingest_status_response_queue_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define ingest_status_response_queue_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/ingest_status_response_queue.h"
ingest_status_response_queue_t* instantiate_ingest_status_response_queue(int include_optional);



ingest_status_response_queue_t* instantiate_ingest_status_response_queue(int include_optional) {
  ingest_status_response_queue_t* ingest_status_response_queue = NULL;
  if (include_optional) {
    ingest_status_response_queue = ingest_status_response_queue_create(
      56,
      56,
      56,
      56
    );
  } else {
    ingest_status_response_queue = ingest_status_response_queue_create(
      56,
      56,
      56,
      56
    );
  }

  return ingest_status_response_queue;
}


#ifdef ingest_status_response_queue_MAIN

void test_ingest_status_response_queue(int include_optional) {
    ingest_status_response_queue_t* ingest_status_response_queue_1 = instantiate_ingest_status_response_queue(include_optional);

	cJSON* jsoningest_status_response_queue_1 = ingest_status_response_queue_convertToJSON(ingest_status_response_queue_1);
	printf("ingest_status_response_queue :\n%s\n", cJSON_Print(jsoningest_status_response_queue_1));
	ingest_status_response_queue_t* ingest_status_response_queue_2 = ingest_status_response_queue_parseFromJSON(jsoningest_status_response_queue_1);
	cJSON* jsoningest_status_response_queue_2 = ingest_status_response_queue_convertToJSON(ingest_status_response_queue_2);
	printf("repeating ingest_status_response_queue:\n%s\n", cJSON_Print(jsoningest_status_response_queue_2));
}

int main() {
  test_ingest_status_response_queue(1);
  test_ingest_status_response_queue(0);

  printf("Hello world \n");
  return 0;
}

#endif // ingest_status_response_queue_MAIN
#endif // ingest_status_response_queue_TEST
