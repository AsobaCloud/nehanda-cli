#ifndef ingest_request_TEST
#define ingest_request_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define ingest_request_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/ingest_request.h"
ingest_request_t* instantiate_ingest_request(int include_optional);



ingest_request_t* instantiate_ingest_request(int include_optional) {
  ingest_request_t* ingest_request = NULL;
  if (include_optional) {
    ingest_request = ingest_request_create(
      "0",
      "0",
      1
    );
  } else {
    ingest_request = ingest_request_create(
      "0",
      "0",
      1
    );
  }

  return ingest_request;
}


#ifdef ingest_request_MAIN

void test_ingest_request(int include_optional) {
    ingest_request_t* ingest_request_1 = instantiate_ingest_request(include_optional);

	cJSON* jsoningest_request_1 = ingest_request_convertToJSON(ingest_request_1);
	printf("ingest_request :\n%s\n", cJSON_Print(jsoningest_request_1));
	ingest_request_t* ingest_request_2 = ingest_request_parseFromJSON(jsoningest_request_1);
	cJSON* jsoningest_request_2 = ingest_request_convertToJSON(ingest_request_2);
	printf("repeating ingest_request:\n%s\n", cJSON_Print(jsoningest_request_2));
}

int main() {
  test_ingest_request(1);
  test_ingest_request(0);

  printf("Hello world \n");
  return 0;
}

#endif // ingest_request_MAIN
#endif // ingest_request_TEST
