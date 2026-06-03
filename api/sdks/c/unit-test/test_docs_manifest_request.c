#ifndef docs_manifest_request_TEST
#define docs_manifest_request_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define docs_manifest_request_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/docs_manifest_request.h"
docs_manifest_request_t* instantiate_docs_manifest_request(int include_optional);



docs_manifest_request_t* instantiate_docs_manifest_request(int include_optional) {
  docs_manifest_request_t* docs_manifest_request = NULL;
  if (include_optional) {
    docs_manifest_request = docs_manifest_request_create(
      "global",
      list_createList()
    );
  } else {
    docs_manifest_request = docs_manifest_request_create(
      "global",
      list_createList()
    );
  }

  return docs_manifest_request;
}


#ifdef docs_manifest_request_MAIN

void test_docs_manifest_request(int include_optional) {
    docs_manifest_request_t* docs_manifest_request_1 = instantiate_docs_manifest_request(include_optional);

	cJSON* jsondocs_manifest_request_1 = docs_manifest_request_convertToJSON(docs_manifest_request_1);
	printf("docs_manifest_request :\n%s\n", cJSON_Print(jsondocs_manifest_request_1));
	docs_manifest_request_t* docs_manifest_request_2 = docs_manifest_request_parseFromJSON(jsondocs_manifest_request_1);
	cJSON* jsondocs_manifest_request_2 = docs_manifest_request_convertToJSON(docs_manifest_request_2);
	printf("repeating docs_manifest_request:\n%s\n", cJSON_Print(jsondocs_manifest_request_2));
}

int main() {
  test_docs_manifest_request(1);
  test_docs_manifest_request(0);

  printf("Hello world \n");
  return 0;
}

#endif // docs_manifest_request_MAIN
#endif // docs_manifest_request_TEST
