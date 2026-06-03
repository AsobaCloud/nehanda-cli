#ifndef entity_search_request_TEST
#define entity_search_request_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define entity_search_request_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/entity_search_request.h"
entity_search_request_t* instantiate_entity_search_request(int include_optional);



entity_search_request_t* instantiate_entity_search_request(int include_optional) {
  entity_search_request_t* entity_search_request = NULL;
  if (include_optional) {
    entity_search_request = entity_search_request_create(
      "0",
      56,
      "0"
    );
  } else {
    entity_search_request = entity_search_request_create(
      "0",
      56,
      "0"
    );
  }

  return entity_search_request;
}


#ifdef entity_search_request_MAIN

void test_entity_search_request(int include_optional) {
    entity_search_request_t* entity_search_request_1 = instantiate_entity_search_request(include_optional);

	cJSON* jsonentity_search_request_1 = entity_search_request_convertToJSON(entity_search_request_1);
	printf("entity_search_request :\n%s\n", cJSON_Print(jsonentity_search_request_1));
	entity_search_request_t* entity_search_request_2 = entity_search_request_parseFromJSON(jsonentity_search_request_1);
	cJSON* jsonentity_search_request_2 = entity_search_request_convertToJSON(entity_search_request_2);
	printf("repeating entity_search_request:\n%s\n", cJSON_Print(jsonentity_search_request_2));
}

int main() {
  test_entity_search_request(1);
  test_entity_search_request(0);

  printf("Hello world \n");
  return 0;
}

#endif // entity_search_request_MAIN
#endif // entity_search_request_TEST
