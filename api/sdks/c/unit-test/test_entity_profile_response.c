#ifndef entity_profile_response_TEST
#define entity_profile_response_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define entity_profile_response_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/entity_profile_response.h"
entity_profile_response_t* instantiate_entity_profile_response(int include_optional);



entity_profile_response_t* instantiate_entity_profile_response(int include_optional) {
  entity_profile_response_t* entity_profile_response = NULL;
  if (include_optional) {
    entity_profile_response = entity_profile_response_create(
      "0",
      "0",
      "0",
      list_createList(),
      list_createList(),
      "0"
    );
  } else {
    entity_profile_response = entity_profile_response_create(
      "0",
      "0",
      "0",
      list_createList(),
      list_createList(),
      "0"
    );
  }

  return entity_profile_response;
}


#ifdef entity_profile_response_MAIN

void test_entity_profile_response(int include_optional) {
    entity_profile_response_t* entity_profile_response_1 = instantiate_entity_profile_response(include_optional);

	cJSON* jsonentity_profile_response_1 = entity_profile_response_convertToJSON(entity_profile_response_1);
	printf("entity_profile_response :\n%s\n", cJSON_Print(jsonentity_profile_response_1));
	entity_profile_response_t* entity_profile_response_2 = entity_profile_response_parseFromJSON(jsonentity_profile_response_1);
	cJSON* jsonentity_profile_response_2 = entity_profile_response_convertToJSON(entity_profile_response_2);
	printf("repeating entity_profile_response:\n%s\n", cJSON_Print(jsonentity_profile_response_2));
}

int main() {
  test_entity_profile_response(1);
  test_entity_profile_response(0);

  printf("Hello world \n");
  return 0;
}

#endif // entity_profile_response_MAIN
#endif // entity_profile_response_TEST
