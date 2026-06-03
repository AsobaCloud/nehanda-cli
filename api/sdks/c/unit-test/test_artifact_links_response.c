#ifndef artifact_links_response_TEST
#define artifact_links_response_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define artifact_links_response_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/artifact_links_response.h"
artifact_links_response_t* instantiate_artifact_links_response(int include_optional);



artifact_links_response_t* instantiate_artifact_links_response(int include_optional) {
  artifact_links_response_t* artifact_links_response = NULL;
  if (include_optional) {
    artifact_links_response = artifact_links_response_create(
      "0",
      list_createList()
    );
  } else {
    artifact_links_response = artifact_links_response_create(
      "0",
      list_createList()
    );
  }

  return artifact_links_response;
}


#ifdef artifact_links_response_MAIN

void test_artifact_links_response(int include_optional) {
    artifact_links_response_t* artifact_links_response_1 = instantiate_artifact_links_response(include_optional);

	cJSON* jsonartifact_links_response_1 = artifact_links_response_convertToJSON(artifact_links_response_1);
	printf("artifact_links_response :\n%s\n", cJSON_Print(jsonartifact_links_response_1));
	artifact_links_response_t* artifact_links_response_2 = artifact_links_response_parseFromJSON(jsonartifact_links_response_1);
	cJSON* jsonartifact_links_response_2 = artifact_links_response_convertToJSON(artifact_links_response_2);
	printf("repeating artifact_links_response:\n%s\n", cJSON_Print(jsonartifact_links_response_2));
}

int main() {
  test_artifact_links_response(1);
  test_artifact_links_response(0);

  printf("Hello world \n");
  return 0;
}

#endif // artifact_links_response_MAIN
#endif // artifact_links_response_TEST
