#ifndef entity_search_response_TEST
#define entity_search_response_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define entity_search_response_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/entity_search_response.h"
entity_search_response_t* instantiate_entity_search_response(int include_optional);



entity_search_response_t* instantiate_entity_search_response(int include_optional) {
  entity_search_response_t* entity_search_response = NULL;
  if (include_optional) {
    entity_search_response = entity_search_response_create(
      list_createList(),
      "0"
    );
  } else {
    entity_search_response = entity_search_response_create(
      list_createList(),
      "0"
    );
  }

  return entity_search_response;
}


#ifdef entity_search_response_MAIN

void test_entity_search_response(int include_optional) {
    entity_search_response_t* entity_search_response_1 = instantiate_entity_search_response(include_optional);

	cJSON* jsonentity_search_response_1 = entity_search_response_convertToJSON(entity_search_response_1);
	printf("entity_search_response :\n%s\n", cJSON_Print(jsonentity_search_response_1));
	entity_search_response_t* entity_search_response_2 = entity_search_response_parseFromJSON(jsonentity_search_response_1);
	cJSON* jsonentity_search_response_2 = entity_search_response_convertToJSON(entity_search_response_2);
	printf("repeating entity_search_response:\n%s\n", cJSON_Print(jsonentity_search_response_2));
}

int main() {
  test_entity_search_response(1);
  test_entity_search_response(0);

  printf("Hello world \n");
  return 0;
}

#endif // entity_search_response_MAIN
#endif // entity_search_response_TEST
