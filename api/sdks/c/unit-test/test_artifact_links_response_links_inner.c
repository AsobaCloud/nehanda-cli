#ifndef artifact_links_response_links_inner_TEST
#define artifact_links_response_links_inner_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define artifact_links_response_links_inner_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/artifact_links_response_links_inner.h"
artifact_links_response_links_inner_t* instantiate_artifact_links_response_links_inner(int include_optional);



artifact_links_response_links_inner_t* instantiate_artifact_links_response_links_inner(int include_optional) {
  artifact_links_response_links_inner_t* artifact_links_response_links_inner = NULL;
  if (include_optional) {
    artifact_links_response_links_inner = artifact_links_response_links_inner_create(
      "0",
      "0"
    );
  } else {
    artifact_links_response_links_inner = artifact_links_response_links_inner_create(
      "0",
      "0"
    );
  }

  return artifact_links_response_links_inner;
}


#ifdef artifact_links_response_links_inner_MAIN

void test_artifact_links_response_links_inner(int include_optional) {
    artifact_links_response_links_inner_t* artifact_links_response_links_inner_1 = instantiate_artifact_links_response_links_inner(include_optional);

	cJSON* jsonartifact_links_response_links_inner_1 = artifact_links_response_links_inner_convertToJSON(artifact_links_response_links_inner_1);
	printf("artifact_links_response_links_inner :\n%s\n", cJSON_Print(jsonartifact_links_response_links_inner_1));
	artifact_links_response_links_inner_t* artifact_links_response_links_inner_2 = artifact_links_response_links_inner_parseFromJSON(jsonartifact_links_response_links_inner_1);
	cJSON* jsonartifact_links_response_links_inner_2 = artifact_links_response_links_inner_convertToJSON(artifact_links_response_links_inner_2);
	printf("repeating artifact_links_response_links_inner:\n%s\n", cJSON_Print(jsonartifact_links_response_links_inner_2));
}

int main() {
  test_artifact_links_response_links_inner(1);
  test_artifact_links_response_links_inner(0);

  printf("Hello world \n");
  return 0;
}

#endif // artifact_links_response_links_inner_MAIN
#endif // artifact_links_response_links_inner_TEST
