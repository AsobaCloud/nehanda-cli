#ifndef docs_manifest_response_TEST
#define docs_manifest_response_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define docs_manifest_response_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/docs_manifest_response.h"
docs_manifest_response_t* instantiate_docs_manifest_response(int include_optional);



docs_manifest_response_t* instantiate_docs_manifest_response(int include_optional) {
  docs_manifest_response_t* docs_manifest_response = NULL;
  if (include_optional) {
    docs_manifest_response = docs_manifest_response_create(
      list_createList(),
      56,
      56,
      56
    );
  } else {
    docs_manifest_response = docs_manifest_response_create(
      list_createList(),
      56,
      56,
      56
    );
  }

  return docs_manifest_response;
}


#ifdef docs_manifest_response_MAIN

void test_docs_manifest_response(int include_optional) {
    docs_manifest_response_t* docs_manifest_response_1 = instantiate_docs_manifest_response(include_optional);

	cJSON* jsondocs_manifest_response_1 = docs_manifest_response_convertToJSON(docs_manifest_response_1);
	printf("docs_manifest_response :\n%s\n", cJSON_Print(jsondocs_manifest_response_1));
	docs_manifest_response_t* docs_manifest_response_2 = docs_manifest_response_parseFromJSON(jsondocs_manifest_response_1);
	cJSON* jsondocs_manifest_response_2 = docs_manifest_response_convertToJSON(docs_manifest_response_2);
	printf("repeating docs_manifest_response:\n%s\n", cJSON_Print(jsondocs_manifest_response_2));
}

int main() {
  test_docs_manifest_response(1);
  test_docs_manifest_response(0);

  printf("Hello world \n");
  return 0;
}

#endif // docs_manifest_response_MAIN
#endif // docs_manifest_response_TEST
