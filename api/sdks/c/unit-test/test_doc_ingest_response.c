#ifndef doc_ingest_response_TEST
#define doc_ingest_response_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define doc_ingest_response_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/doc_ingest_response.h"
doc_ingest_response_t* instantiate_doc_ingest_response(int include_optional);



doc_ingest_response_t* instantiate_doc_ingest_response(int include_optional) {
  doc_ingest_response_t* doc_ingest_response = NULL;
  if (include_optional) {
    doc_ingest_response = doc_ingest_response_create(
      56,
      aimee_kb_api_doc_ingest_response_STATE_staged
    );
  } else {
    doc_ingest_response = doc_ingest_response_create(
      56,
      aimee_kb_api_doc_ingest_response_STATE_staged
    );
  }

  return doc_ingest_response;
}


#ifdef doc_ingest_response_MAIN

void test_doc_ingest_response(int include_optional) {
    doc_ingest_response_t* doc_ingest_response_1 = instantiate_doc_ingest_response(include_optional);

	cJSON* jsondoc_ingest_response_1 = doc_ingest_response_convertToJSON(doc_ingest_response_1);
	printf("doc_ingest_response :\n%s\n", cJSON_Print(jsondoc_ingest_response_1));
	doc_ingest_response_t* doc_ingest_response_2 = doc_ingest_response_parseFromJSON(jsondoc_ingest_response_1);
	cJSON* jsondoc_ingest_response_2 = doc_ingest_response_convertToJSON(doc_ingest_response_2);
	printf("repeating doc_ingest_response:\n%s\n", cJSON_Print(jsondoc_ingest_response_2));
}

int main() {
  test_doc_ingest_response(1);
  test_doc_ingest_response(0);

  printf("Hello world \n");
  return 0;
}

#endif // doc_ingest_response_MAIN
#endif // doc_ingest_response_TEST
