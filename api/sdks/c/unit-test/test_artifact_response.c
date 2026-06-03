#ifndef artifact_response_TEST
#define artifact_response_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define artifact_response_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/artifact_response.h"
artifact_response_t* instantiate_artifact_response(int include_optional);



artifact_response_t* instantiate_artifact_response(int include_optional) {
  artifact_response_t* artifact_response = NULL;
  if (include_optional) {
    artifact_response = artifact_response_create(
      "0",
      "0",
      "0",
      "0",
      "0",
      1.337,
      0,
      list_createList(),
      "2013-10-20T19:20:30+01:00"
    );
  } else {
    artifact_response = artifact_response_create(
      "0",
      "0",
      "0",
      "0",
      "0",
      1.337,
      0,
      list_createList(),
      "2013-10-20T19:20:30+01:00"
    );
  }

  return artifact_response;
}


#ifdef artifact_response_MAIN

void test_artifact_response(int include_optional) {
    artifact_response_t* artifact_response_1 = instantiate_artifact_response(include_optional);

	cJSON* jsonartifact_response_1 = artifact_response_convertToJSON(artifact_response_1);
	printf("artifact_response :\n%s\n", cJSON_Print(jsonartifact_response_1));
	artifact_response_t* artifact_response_2 = artifact_response_parseFromJSON(jsonartifact_response_1);
	cJSON* jsonartifact_response_2 = artifact_response_convertToJSON(artifact_response_2);
	printf("repeating artifact_response:\n%s\n", cJSON_Print(jsonartifact_response_2));
}

int main() {
  test_artifact_response(1);
  test_artifact_response(0);

  printf("Hello world \n");
  return 0;
}

#endif // artifact_response_MAIN
#endif // artifact_response_TEST
