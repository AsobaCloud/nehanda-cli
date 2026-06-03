#ifndef doc_metadata_response_TEST
#define doc_metadata_response_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define doc_metadata_response_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/doc_metadata_response.h"
doc_metadata_response_t* instantiate_doc_metadata_response(int include_optional);



doc_metadata_response_t* instantiate_doc_metadata_response(int include_optional) {
  doc_metadata_response_t* doc_metadata_response = NULL;
  if (include_optional) {
    doc_metadata_response = doc_metadata_response_create(
      56,
      "0",
      "0",
      "0",
      "0",
      "0",
      "0",
      1,
      "2013-10-20T19:20:30+01:00"
    );
  } else {
    doc_metadata_response = doc_metadata_response_create(
      56,
      "0",
      "0",
      "0",
      "0",
      "0",
      "0",
      1,
      "2013-10-20T19:20:30+01:00"
    );
  }

  return doc_metadata_response;
}


#ifdef doc_metadata_response_MAIN

void test_doc_metadata_response(int include_optional) {
    doc_metadata_response_t* doc_metadata_response_1 = instantiate_doc_metadata_response(include_optional);

	cJSON* jsondoc_metadata_response_1 = doc_metadata_response_convertToJSON(doc_metadata_response_1);
	printf("doc_metadata_response :\n%s\n", cJSON_Print(jsondoc_metadata_response_1));
	doc_metadata_response_t* doc_metadata_response_2 = doc_metadata_response_parseFromJSON(jsondoc_metadata_response_1);
	cJSON* jsondoc_metadata_response_2 = doc_metadata_response_convertToJSON(doc_metadata_response_2);
	printf("repeating doc_metadata_response:\n%s\n", cJSON_Print(jsondoc_metadata_response_2));
}

int main() {
  test_doc_metadata_response(1);
  test_doc_metadata_response(0);

  printf("Hello world \n");
  return 0;
}

#endif // doc_metadata_response_MAIN
#endif // doc_metadata_response_TEST
