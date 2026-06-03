/* test_code_index_ops.c: code-chunk replay bookkeeping over the sqlite shim. */
#include <assert.h>
#include <stdio.h>

#include "aimee.h"
#include "db2_test_shim.h"
#include "../db2/code_index_ops.h"

int main(void)
{
   db2_test_shim_open();

   db2_code_index_ops_summary_t sum;
   /* ok embed recorded */
   db2_code_index_op_record(1, "proj", "file:src/a.c", "src/a.c", 1, NULL);
   /* a failing embed, recorded twice → attempts climbs */
   db2_code_index_op_record(2, "proj", "file:src/b.c", "src/b.c", 0, "boom");
   db2_code_index_op_record(2, "proj", "file:src/b.c", "src/b.c", 0, "boom");

   assert(db2_code_index_ops_summary(2, &sum) == 0);
   assert(sum.ok_ops == 1);
   assert(sum.failed_ops == 1);
   assert(sum.stuck_ops == 1); /* point 2 has attempts >= 2 */
   printf("  record ok/failed + summary OK (ok=%lld failed=%lld stuck=%lld)\n",
          (long long)sum.ok_ops, (long long)sum.failed_ops, (long long)sum.stuck_ops);

   /* reset-stuck clears the stuck failed row's attempts */
   int reset = db2_code_index_ops_reset_stuck(2);
   assert(reset == 1);
   assert(db2_code_index_ops_summary(2, &sum) == 0);
   assert(sum.stuck_ops == 0);  /* no longer stuck */
   assert(sum.failed_ops == 1); /* still failed, but retryable */
   printf("  reset-stuck retries a stuck code embed OK\n");

   db2_test_shim_close();
   printf("code_index_ops: all tests passed\n");
   return 0;
}
