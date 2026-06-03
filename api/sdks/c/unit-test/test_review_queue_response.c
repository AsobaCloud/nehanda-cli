#ifndef review_queue_response_TEST
#define review_queue_response_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define review_queue_response_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/review_queue_response.h"
review_queue_response_t* instantiate_review_queue_response(int include_optional);



review_queue_response_t* instantiate_review_queue_response(int include_optional) {
  review_queue_response_t* review_queue_response = NULL;
  if (include_optional) {
    review_queue_response = review_queue_response_create(
      list_createList(),
      56
    );
  } else {
    review_queue_response = review_queue_response_create(
      list_createList(),
      56
    );
  }

  return review_queue_response;
}


#ifdef review_queue_response_MAIN

void test_review_queue_response(int include_optional) {
    review_queue_response_t* review_queue_response_1 = instantiate_review_queue_response(include_optional);

	cJSON* jsonreview_queue_response_1 = review_queue_response_convertToJSON(review_queue_response_1);
	printf("review_queue_response :\n%s\n", cJSON_Print(jsonreview_queue_response_1));
	review_queue_response_t* review_queue_response_2 = review_queue_response_parseFromJSON(jsonreview_queue_response_1);
	cJSON* jsonreview_queue_response_2 = review_queue_response_convertToJSON(review_queue_response_2);
	printf("repeating review_queue_response:\n%s\n", cJSON_Print(jsonreview_queue_response_2));
}

int main() {
  test_review_queue_response(1);
  test_review_queue_response(0);

  printf("Hello world \n");
  return 0;
}

#endif // review_queue_response_MAIN
#endif // review_queue_response_TEST
