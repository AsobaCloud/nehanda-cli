#ifndef drain_request_TEST
#define drain_request_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define drain_request_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/drain_request.h"
drain_request_t* instantiate_drain_request(int include_optional);



drain_request_t* instantiate_drain_request(int include_optional) {
  drain_request_t* drain_request = NULL;
  if (include_optional) {
    drain_request = drain_request_create(
      "0",
      56
    );
  } else {
    drain_request = drain_request_create(
      "0",
      56
    );
  }

  return drain_request;
}


#ifdef drain_request_MAIN

void test_drain_request(int include_optional) {
    drain_request_t* drain_request_1 = instantiate_drain_request(include_optional);

	cJSON* jsondrain_request_1 = drain_request_convertToJSON(drain_request_1);
	printf("drain_request :\n%s\n", cJSON_Print(jsondrain_request_1));
	drain_request_t* drain_request_2 = drain_request_parseFromJSON(jsondrain_request_1);
	cJSON* jsondrain_request_2 = drain_request_convertToJSON(drain_request_2);
	printf("repeating drain_request:\n%s\n", cJSON_Print(jsondrain_request_2));
}

int main() {
  test_drain_request(1);
  test_drain_request(0);

  printf("Hello world \n");
  return 0;
}

#endif // drain_request_MAIN
#endif // drain_request_TEST
