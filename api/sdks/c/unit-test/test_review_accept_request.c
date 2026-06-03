#ifndef review_accept_request_TEST
#define review_accept_request_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define review_accept_request_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/review_accept_request.h"
review_accept_request_t* instantiate_review_accept_request(int include_optional);



review_accept_request_t* instantiate_review_accept_request(int include_optional) {
  review_accept_request_t* review_accept_request = NULL;
  if (include_optional) {
    review_accept_request = review_accept_request_create(
      56
    );
  } else {
    review_accept_request = review_accept_request_create(
      56
    );
  }

  return review_accept_request;
}


#ifdef review_accept_request_MAIN

void test_review_accept_request(int include_optional) {
    review_accept_request_t* review_accept_request_1 = instantiate_review_accept_request(include_optional);

	cJSON* jsonreview_accept_request_1 = review_accept_request_convertToJSON(review_accept_request_1);
	printf("review_accept_request :\n%s\n", cJSON_Print(jsonreview_accept_request_1));
	review_accept_request_t* review_accept_request_2 = review_accept_request_parseFromJSON(jsonreview_accept_request_1);
	cJSON* jsonreview_accept_request_2 = review_accept_request_convertToJSON(review_accept_request_2);
	printf("repeating review_accept_request:\n%s\n", cJSON_Print(jsonreview_accept_request_2));
}

int main() {
  test_review_accept_request(1);
  test_review_accept_request(0);

  printf("Hello world \n");
  return 0;
}

#endif // review_accept_request_MAIN
#endif // review_accept_request_TEST
