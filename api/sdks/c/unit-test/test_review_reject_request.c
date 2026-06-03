#ifndef review_reject_request_TEST
#define review_reject_request_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define review_reject_request_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/review_reject_request.h"
review_reject_request_t* instantiate_review_reject_request(int include_optional);



review_reject_request_t* instantiate_review_reject_request(int include_optional) {
  review_reject_request_t* review_reject_request = NULL;
  if (include_optional) {
    review_reject_request = review_reject_request_create(
      "0"
    );
  } else {
    review_reject_request = review_reject_request_create(
      "0"
    );
  }

  return review_reject_request;
}


#ifdef review_reject_request_MAIN

void test_review_reject_request(int include_optional) {
    review_reject_request_t* review_reject_request_1 = instantiate_review_reject_request(include_optional);

	cJSON* jsonreview_reject_request_1 = review_reject_request_convertToJSON(review_reject_request_1);
	printf("review_reject_request :\n%s\n", cJSON_Print(jsonreview_reject_request_1));
	review_reject_request_t* review_reject_request_2 = review_reject_request_parseFromJSON(jsonreview_reject_request_1);
	cJSON* jsonreview_reject_request_2 = review_reject_request_convertToJSON(review_reject_request_2);
	printf("repeating review_reject_request:\n%s\n", cJSON_Print(jsonreview_reject_request_2));
}

int main() {
  test_review_reject_request(1);
  test_review_reject_request(0);

  printf("Hello world \n");
  return 0;
}

#endif // review_reject_request_MAIN
#endif // review_reject_request_TEST
