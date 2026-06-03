#ifndef drain_response_TEST
#define drain_response_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define drain_response_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/drain_response.h"
drain_response_t* instantiate_drain_response(int include_optional);



drain_response_t* instantiate_drain_response(int include_optional) {
  drain_response_t* drain_response = NULL;
  if (include_optional) {
    drain_response = drain_response_create(
      aimee_kb_api_drain_response_STATE_idle,
      56,
      56,
      56,
      56,
      56,
      56
    );
  } else {
    drain_response = drain_response_create(
      aimee_kb_api_drain_response_STATE_idle,
      56,
      56,
      56,
      56,
      56,
      56
    );
  }

  return drain_response;
}


#ifdef drain_response_MAIN

void test_drain_response(int include_optional) {
    drain_response_t* drain_response_1 = instantiate_drain_response(include_optional);

	cJSON* jsondrain_response_1 = drain_response_convertToJSON(drain_response_1);
	printf("drain_response :\n%s\n", cJSON_Print(jsondrain_response_1));
	drain_response_t* drain_response_2 = drain_response_parseFromJSON(jsondrain_response_1);
	cJSON* jsondrain_response_2 = drain_response_convertToJSON(drain_response_2);
	printf("repeating drain_response:\n%s\n", cJSON_Print(jsondrain_response_2));
}

int main() {
  test_drain_response(1);
  test_drain_response(0);

  printf("Hello world \n");
  return 0;
}

#endif // drain_response_MAIN
#endif // drain_response_TEST
