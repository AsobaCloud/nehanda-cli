#ifndef code_update_request_TEST
#define code_update_request_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define code_update_request_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/code_update_request.h"
code_update_request_t* instantiate_code_update_request(int include_optional);



code_update_request_t* instantiate_code_update_request(int include_optional) {
  code_update_request_t* code_update_request = NULL;
  if (include_optional) {
    code_update_request = code_update_request_create(
      "0",
      "0",
      "0"
    );
  } else {
    code_update_request = code_update_request_create(
      "0",
      "0",
      "0"
    );
  }

  return code_update_request;
}


#ifdef code_update_request_MAIN

void test_code_update_request(int include_optional) {
    code_update_request_t* code_update_request_1 = instantiate_code_update_request(include_optional);

	cJSON* jsoncode_update_request_1 = code_update_request_convertToJSON(code_update_request_1);
	printf("code_update_request :\n%s\n", cJSON_Print(jsoncode_update_request_1));
	code_update_request_t* code_update_request_2 = code_update_request_parseFromJSON(jsoncode_update_request_1);
	cJSON* jsoncode_update_request_2 = code_update_request_convertToJSON(code_update_request_2);
	printf("repeating code_update_request:\n%s\n", cJSON_Print(jsoncode_update_request_2));
}

int main() {
  test_code_update_request(1);
  test_code_update_request(0);

  printf("Hello world \n");
  return 0;
}

#endif // code_update_request_MAIN
#endif // code_update_request_TEST
