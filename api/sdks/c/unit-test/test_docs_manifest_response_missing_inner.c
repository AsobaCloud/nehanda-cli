#ifndef docs_manifest_response_missing_inner_TEST
#define docs_manifest_response_missing_inner_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define docs_manifest_response_missing_inner_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/docs_manifest_response_missing_inner.h"
docs_manifest_response_missing_inner_t* instantiate_docs_manifest_response_missing_inner(int include_optional);



docs_manifest_response_missing_inner_t* instantiate_docs_manifest_response_missing_inner(int include_optional) {
  docs_manifest_response_missing_inner_t* docs_manifest_response_missing_inner = NULL;
  if (include_optional) {
    docs_manifest_response_missing_inner = docs_manifest_response_missing_inner_create(
      "0",
      "0",
      "0"
    );
  } else {
    docs_manifest_response_missing_inner = docs_manifest_response_missing_inner_create(
      "0",
      "0",
      "0"
    );
  }

  return docs_manifest_response_missing_inner;
}


#ifdef docs_manifest_response_missing_inner_MAIN

void test_docs_manifest_response_missing_inner(int include_optional) {
    docs_manifest_response_missing_inner_t* docs_manifest_response_missing_inner_1 = instantiate_docs_manifest_response_missing_inner(include_optional);

	cJSON* jsondocs_manifest_response_missing_inner_1 = docs_manifest_response_missing_inner_convertToJSON(docs_manifest_response_missing_inner_1);
	printf("docs_manifest_response_missing_inner :\n%s\n", cJSON_Print(jsondocs_manifest_response_missing_inner_1));
	docs_manifest_response_missing_inner_t* docs_manifest_response_missing_inner_2 = docs_manifest_response_missing_inner_parseFromJSON(jsondocs_manifest_response_missing_inner_1);
	cJSON* jsondocs_manifest_response_missing_inner_2 = docs_manifest_response_missing_inner_convertToJSON(docs_manifest_response_missing_inner_2);
	printf("repeating docs_manifest_response_missing_inner:\n%s\n", cJSON_Print(jsondocs_manifest_response_missing_inner_2));
}

int main() {
  test_docs_manifest_response_missing_inner(1);
  test_docs_manifest_response_missing_inner(0);

  printf("Hello world \n");
  return 0;
}

#endif // docs_manifest_response_missing_inner_MAIN
#endif // docs_manifest_response_missing_inner_TEST
