/* db1_internal.h: private to src/db1/.
 *
 * Shared state between subsystem .c files (wm.c, sessions.c, ...) and
 * the init/shutdown pair in db1_init.c. NOT installed; NOT included by
 * anything outside src/db1/.
 */
#ifndef DEC_DB1_INTERNAL_H
#define DEC_DB1_INTERNAL_H 1

#include <sqlite3.h>
#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C"
{
#endif

   /* Returns the module-private sqlite3 connection, or NULL if db1_init
    * has not been called (or db1_shutdown has been called). Subsystem
    * .c files call this at the top of every public function. */
   sqlite3 *db1_conn(void);

   /* Copy a TEXT column into a fixed-size buffer with NULL-safety — the
    * `snprintf(dst, cap, "%s", sqlite3_column_text(...) ?: "")` shape that
    * ~19 db1 row-mapper .c files each open-coded as a private static. */
   static inline void db1_copy_col_text(char *dst, size_t cap, sqlite3_stmt *stmt, int col)
   {
      const unsigned char *v = sqlite3_column_text(stmt, col);
      snprintf(dst, cap, "%s", v ? (const char *)v : "");
   }

#ifdef __cplusplus
}
#endif

#endif /* DEC_DB1_INTERNAL_H */
