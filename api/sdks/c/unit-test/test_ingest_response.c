#ifndef ingest_response_TEST
#define ingest_response_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define ingest_response_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/ingest_response.h"
ingest_response_t* instantiate_ingest_response(int include_optional);



ingest_response_t* instantiate_ingest_response(int include_optional) {
  ingest_response_t* ingest_response = NULL;
  if (include_optional) {
    ingest_response = ingest_response_create(
      "ok",
      56,
      "0"
    );
  } else {
    ingest_response = ingest_response_create(
      "ok",
      56,
      "0"
    );
  }

  return ingest_response;
}


#ifdef ingest_response_MAIN

void test_ingest_response(int include_optional) {
    ingest_response_t* ingest_response_1 = instantiate_ingest_response(include_optional);

	cJSON* jsoningest_response_1 = ingest_response_convertToJSON(ingest_response_1);
	printf("ingest_response :\n%s\n", cJSON_Print(jsoningest_response_1));
	ingest_response_t* ingest_response_2 = ingest_response_parseFromJSON(jsoningest_response_1);
	cJSON* jsoningest_response_2 = ingest_response_convertToJSON(ingest_response_2);
	printf("repeating ingest_response:\n%s\n", cJSON_Print(jsoningest_response_2));
}

int main() {
  test_ingest_response(1);
  test_ingest_response(0);

  printf("Hello world \n");
  return 0;
}

#endif // ingest_response_MAIN
#endif // ingest_response_TEST
