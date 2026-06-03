#ifndef active_release_response_TEST
#define active_release_response_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define active_release_response_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/active_release_response.h"
active_release_response_t* instantiate_active_release_response(int include_optional);



active_release_response_t* instantiate_active_release_response(int include_optional) {
  active_release_response_t* active_release_response = NULL;
  if (include_optional) {
    active_release_response = active_release_response_create(
      56,
      "0",
      "0",
      "0"
    );
  } else {
    active_release_response = active_release_response_create(
      56,
      "0",
      "0",
      "0"
    );
  }

  return active_release_response;
}


#ifdef active_release_response_MAIN

void test_active_release_response(int include_optional) {
    active_release_response_t* active_release_response_1 = instantiate_active_release_response(include_optional);

	cJSON* jsonactive_release_response_1 = active_release_response_convertToJSON(active_release_response_1);
	printf("active_release_response :\n%s\n", cJSON_Print(jsonactive_release_response_1));
	active_release_response_t* active_release_response_2 = active_release_response_parseFromJSON(jsonactive_release_response_1);
	cJSON* jsonactive_release_response_2 = active_release_response_convertToJSON(active_release_response_2);
	printf("repeating active_release_response:\n%s\n", cJSON_Print(jsonactive_release_response_2));
}

int main() {
  test_active_release_response(1);
  test_active_release_response(0);

  printf("Hello world \n");
  return 0;
}

#endif // active_release_response_MAIN
#endif // active_release_response_TEST
