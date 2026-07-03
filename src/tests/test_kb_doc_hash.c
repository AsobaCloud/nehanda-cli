/* test_kb_doc_hash.c: frontmatter-insensitive Markdown content hashing
 * (graph-feedback §5 / S4). A metadata-only frontmatter edit (status/reviewed/
 * tags/date) must NOT change the hash; a title or body edit always must. Pure. */
#include "kb_doc_hash.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void md(const char *s, char out[KB_DOC_HASH_HEX_LEN + 1])
{
   kb_doc_content_hash_md(s, (int)strlen(s), out);
}

/* No leading frontmatter → identical to the plain content hash. */
static void test_no_frontmatter_equals_plain(void)
{
   const char *doc = "# Title\n\nsome body text\n";
   char a[KB_DOC_HASH_HEX_LEN + 1], b[KB_DOC_HASH_HEX_LEN + 1];
   md(doc, a);
   kb_doc_content_hash(doc, (int)strlen(doc), b);
   assert(strcmp(a, b) == 0);
   printf("  test_no_frontmatter_equals_plain: ok\n");
}

/* An ignorable-key edit (status/reviewed/tags/date) does NOT change the hash. */
static void test_ignorable_key_edit_stable(void)
{
   const char *v1 = "---\ntitle: My Doc\nstatus: draft\ndate: 2026-01-01\n---\n\nbody\n";
   const char *v2 = "---\ntitle: My Doc\nstatus: published\ndate: 2026-07-03\n---\n\nbody\n";
   char a[KB_DOC_HASH_HEX_LEN + 1], b[KB_DOC_HASH_HEX_LEN + 1];
   md(v1, a);
   md(v2, b);
   assert(strcmp(a, b) == 0); /* only ignorable keys changed */
   printf("  test_ignorable_key_edit_stable: ok\n");
}

/* A multi-line ignorable value (tags list) is skipped wholesale. */
static void test_multiline_ignorable(void)
{
   const char *v1 = "---\ntitle: T\ntags:\n  - a\n  - b\nreviewed: no\n---\nbody\n";
   const char *v2 = "---\ntitle: T\ntags:\n  - x\n  - y\n  - z\nreviewed: yes\n---\nbody\n";
   char a[KB_DOC_HASH_HEX_LEN + 1], b[KB_DOC_HASH_HEX_LEN + 1];
   md(v1, a);
   md(v2, b);
   assert(strcmp(a, b) == 0);
   printf("  test_multiline_ignorable: ok\n");
}

/* A title (non-ignorable frontmatter) edit DOES change the hash. */
static void test_title_edit_changes(void)
{
   const char *v1 = "---\ntitle: Old\nstatus: draft\n---\nbody\n";
   const char *v2 = "---\ntitle: New\nstatus: draft\n---\nbody\n";
   char a[KB_DOC_HASH_HEX_LEN + 1], b[KB_DOC_HASH_HEX_LEN + 1];
   md(v1, a);
   md(v2, b);
   assert(strcmp(a, b) != 0);
   printf("  test_title_edit_changes: ok\n");
}

/* A body edit DOES change the hash even if frontmatter is identical. */
static void test_body_edit_changes(void)
{
   const char *v1 = "---\ntitle: T\nstatus: draft\n---\nold body\n";
   const char *v2 = "---\ntitle: T\nstatus: draft\n---\nnew body\n";
   char a[KB_DOC_HASH_HEX_LEN + 1], b[KB_DOC_HASH_HEX_LEN + 1];
   md(v1, a);
   md(v2, b);
   assert(strcmp(a, b) != 0);
   printf("  test_body_edit_changes: ok\n");
}

/* A key that merely CONTAINS an ignorable name (e.g. "status_note") is NOT
 * ignorable — exact key match only. */
static void test_prefix_not_ignorable(void)
{
   const char *v1 = "---\nstatus_note: a\n---\nbody\n";
   const char *v2 = "---\nstatus_note: b\n---\nbody\n";
   char a[KB_DOC_HASH_HEX_LEN + 1], b[KB_DOC_HASH_HEX_LEN + 1];
   md(v1, a);
   md(v2, b);
   assert(strcmp(a, b) != 0); /* status_note is a real content key */
   printf("  test_prefix_not_ignorable: ok\n");
}

int main(void)
{
   printf("test_kb_doc_hash:\n");
   test_no_frontmatter_equals_plain();
   test_ignorable_key_edit_stable();
   test_multiline_ignorable();
   test_title_edit_changes();
   test_body_edit_changes();
   test_prefix_not_ignorable();
   printf("ALL PASS\n");
   return 0;
}
