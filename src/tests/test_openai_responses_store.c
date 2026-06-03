/* test_openai_responses_store.c: unit tests for the in-process /v1/responses
 * continuation store (no sockets, no agent execution). */
#include "openai_responses_store.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
   printf("openai_responses_store: ");
   openai_responses_store_reset(); /* order-independent */

   char buf[256];

   /* unknown id -> 0, out cleared */
   {
      strcpy(buf, "dirty");
      assert(openai_responses_store_get("nope", buf, sizeof(buf)) == 0);
      assert(buf[0] == '\0');
   }

   /* store + retrieve exact bytes */
   {
      const char *t = "user: hi\nassistant: hello\n";
      openai_responses_store_put("resp_1", t);
      assert(openai_responses_store_get("resp_1", buf, sizeof(buf)) == 1);
      assert(strcmp(buf, t) == 0);
   }

   /* overwrite same id */
   {
      openai_responses_store_put("resp_1", "first");
      openai_responses_store_put("resp_1", "second");
      assert(openai_responses_store_get("resp_1", buf, sizeof(buf)) == 1);
      assert(strcmp(buf, "second") == 0);
   }

   /* two distinct ids are independent */
   {
      openai_responses_store_put("resp_a", "alpha");
      openai_responses_store_put("resp_b", "beta");
      assert(openai_responses_store_get("resp_a", buf, sizeof(buf)) == 1);
      assert(strcmp(buf, "alpha") == 0);
      assert(openai_responses_store_get("resp_b", buf, sizeof(buf)) == 1);
      assert(strcmp(buf, "beta") == 0);
   }

   /* NULL / empty id -> 0 */
   {
      assert(openai_responses_store_get(NULL, buf, sizeof(buf)) == 0);
      assert(openai_responses_store_get("", buf, sizeof(buf)) == 0);
   }

   /* truncation into a small buffer: found, NUL-terminated within bounds */
   {
      char small[8];
      openai_responses_store_put("resp_long", "0123456789abcdef");
      assert(openai_responses_store_get("resp_long", small, sizeof(small)) == 1);
      assert(small[sizeof(small) - 1] == '\0');
      assert(strncmp(small, "0123456789abcdef", sizeof(small) - 1) == 0);
   }

   /* reset drops everything */
   {
      openai_responses_store_reset();
      assert(openai_responses_store_get("resp_1", buf, sizeof(buf)) == 0);
      assert(openai_responses_store_get("resp_a", buf, sizeof(buf)) == 0);
   }

   printf("ok\n");
   return 0;
}
