#ifndef ingest_status_response_recent_inner_TEST
#define ingest_status_response_recent_inner_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define ingest_status_response_recent_inner_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/ingest_status_response_recent_inner.h"
ingest_status_response_recent_inner_t* instantiate_ingest_status_response_recent_inner(int include_optional);



ingest_status_response_recent_inner_t* instantiate_ingest_status_response_recent_inner(int include_optional) {
  ingest_status_response_recent_inner_t* ingest_status_response_recent_inner = NULL;
  if (include_optional) {
    ingest_status_response_recent_inner = ingest_status_response_recent_inner_create(
      "0",
      "0",
      "0",
      56,
      56,
      "0"
    );
  } else {
    ingest_status_response_recent_inner = ingest_status_response_recent_inner_create(
      "0",
      "0",
      "0",
      56,
      56,
      "0"
    );
  }

  return ingest_status_response_recent_inner;
}


#ifdef ingest_status_response_recent_inner_MAIN

void test_ingest_status_response_recent_inner(int include_optional) {
    ingest_status_response_recent_inner_t* ingest_status_response_recent_inner_1 = instantiate_ingest_status_response_recent_inner(include_optional);

	cJSON* jsoningest_status_response_recent_inner_1 = ingest_status_response_recent_inner_convertToJSON(ingest_status_response_recent_inner_1);
	printf("ingest_status_response_recent_inner :\n%s\n", cJSON_Print(jsoningest_status_response_recent_inner_1));
	ingest_status_response_recent_inner_t* ingest_status_response_recent_inner_2 = ingest_status_response_recent_inner_parseFromJSON(jsoningest_status_response_recent_inner_1);
	cJSON* jsoningest_status_response_recent_inner_2 = ingest_status_response_recent_inner_convertToJSON(ingest_status_response_recent_inner_2);
	printf("repeating ingest_status_response_recent_inner:\n%s\n", cJSON_Print(jsoningest_status_response_recent_inner_2));
}

int main() {
  test_ingest_status_response_recent_inner(1);
  test_ingest_status_response_recent_inner(0);

  printf("Hello world \n");
  return 0;
}

#endif // ingest_status_response_recent_inner_MAIN
#endif // ingest_status_response_recent_inner_TEST
