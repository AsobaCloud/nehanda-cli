#ifndef create_release_response_TEST
#define create_release_response_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define create_release_response_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/create_release_response.h"
create_release_response_t* instantiate_create_release_response(int include_optional);



create_release_response_t* instantiate_create_release_response(int include_optional) {
  create_release_response_t* create_release_response = NULL;
  if (include_optional) {
    create_release_response = create_release_response_create(
      56,
      aimee_kb_api_create_release_response_STATE_pending
    );
  } else {
    create_release_response = create_release_response_create(
      56,
      aimee_kb_api_create_release_response_STATE_pending
    );
  }

  return create_release_response;
}


#ifdef create_release_response_MAIN

void test_create_release_response(int include_optional) {
    create_release_response_t* create_release_response_1 = instantiate_create_release_response(include_optional);

	cJSON* jsoncreate_release_response_1 = create_release_response_convertToJSON(create_release_response_1);
	printf("create_release_response :\n%s\n", cJSON_Print(jsoncreate_release_response_1));
	create_release_response_t* create_release_response_2 = create_release_response_parseFromJSON(jsoncreate_release_response_1);
	cJSON* jsoncreate_release_response_2 = create_release_response_convertToJSON(create_release_response_2);
	printf("repeating create_release_response:\n%s\n", cJSON_Print(jsoncreate_release_response_2));
}

int main() {
  test_create_release_response(1);
  test_create_release_response(0);

  printf("Hello world \n");
  return 0;
}

#endif // create_release_response_MAIN
#endif // create_release_response_TEST
