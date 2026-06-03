#ifndef rollback_request_TEST
#define rollback_request_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define rollback_request_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/rollback_request.h"
rollback_request_t* instantiate_rollback_request(int include_optional);



rollback_request_t* instantiate_rollback_request(int include_optional) {
  rollback_request_t* rollback_request = NULL;
  if (include_optional) {
    rollback_request = rollback_request_create(
      56
    );
  } else {
    rollback_request = rollback_request_create(
      56
    );
  }

  return rollback_request;
}


#ifdef rollback_request_MAIN

void test_rollback_request(int include_optional) {
    rollback_request_t* rollback_request_1 = instantiate_rollback_request(include_optional);

	cJSON* jsonrollback_request_1 = rollback_request_convertToJSON(rollback_request_1);
	printf("rollback_request :\n%s\n", cJSON_Print(jsonrollback_request_1));
	rollback_request_t* rollback_request_2 = rollback_request_parseFromJSON(jsonrollback_request_1);
	cJSON* jsonrollback_request_2 = rollback_request_convertToJSON(rollback_request_2);
	printf("repeating rollback_request:\n%s\n", cJSON_Print(jsonrollback_request_2));
}

int main() {
  test_rollback_request(1);
  test_rollback_request(0);

  printf("Hello world \n");
  return 0;
}

#endif // rollback_request_MAIN
#endif // rollback_request_TEST
