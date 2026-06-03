#ifndef capabilities_response_TEST
#define capabilities_response_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define capabilities_response_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/capabilities_response.h"
capabilities_response_t* instantiate_capabilities_response(int include_optional);



capabilities_response_t* instantiate_capabilities_response(int include_optional) {
  capabilities_response_t* capabilities_response = NULL;
  if (include_optional) {
    capabilities_response = capabilities_response_create(
      [memory, search, index],
      "0"
    );
  } else {
    capabilities_response = capabilities_response_create(
      [memory, search, index],
      "0"
    );
  }

  return capabilities_response;
}


#ifdef capabilities_response_MAIN

void test_capabilities_response(int include_optional) {
    capabilities_response_t* capabilities_response_1 = instantiate_capabilities_response(include_optional);

	cJSON* jsoncapabilities_response_1 = capabilities_response_convertToJSON(capabilities_response_1);
	printf("capabilities_response :\n%s\n", cJSON_Print(jsoncapabilities_response_1));
	capabilities_response_t* capabilities_response_2 = capabilities_response_parseFromJSON(jsoncapabilities_response_1);
	cJSON* jsoncapabilities_response_2 = capabilities_response_convertToJSON(capabilities_response_2);
	printf("repeating capabilities_response:\n%s\n", cJSON_Print(jsoncapabilities_response_2));
}

int main() {
  test_capabilities_response(1);
  test_capabilities_response(0);

  printf("Hello world \n");
  return 0;
}

#endif // capabilities_response_MAIN
#endif // capabilities_response_TEST
