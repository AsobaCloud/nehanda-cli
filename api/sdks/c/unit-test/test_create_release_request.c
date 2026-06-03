#ifndef create_release_request_TEST
#define create_release_request_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define create_release_request_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/create_release_request.h"
create_release_request_t* instantiate_create_release_request(int include_optional);



create_release_request_t* instantiate_create_release_request(int include_optional) {
  create_release_request_t* create_release_request = NULL;
  if (include_optional) {
    create_release_request = create_release_request_create(
      "0"
    );
  } else {
    create_release_request = create_release_request_create(
      "0"
    );
  }

  return create_release_request;
}


#ifdef create_release_request_MAIN

void test_create_release_request(int include_optional) {
    create_release_request_t* create_release_request_1 = instantiate_create_release_request(include_optional);

	cJSON* jsoncreate_release_request_1 = create_release_request_convertToJSON(create_release_request_1);
	printf("create_release_request :\n%s\n", cJSON_Print(jsoncreate_release_request_1));
	create_release_request_t* create_release_request_2 = create_release_request_parseFromJSON(jsoncreate_release_request_1);
	cJSON* jsoncreate_release_request_2 = create_release_request_convertToJSON(create_release_request_2);
	printf("repeating create_release_request:\n%s\n", cJSON_Print(jsoncreate_release_request_2));
}

int main() {
  test_create_release_request(1);
  test_create_release_request(0);

  printf("Hello world \n");
  return 0;
}

#endif // create_release_request_MAIN
#endif // create_release_request_TEST
