/* test_kb_doc_pdf.c: structured-PDF Phase 1 (first increment). Pure tests of the
 * parse->normalize->chunk pipeline over `pdftotext -bbox-layout` XHTML, plus a
 * shim-backed ingest test asserting kb_documents + kb_doc_regions + embed enqueue. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aimee.h"
#include "db2.h"
#include "db2_test_shim.h"
#include "../db2/db_postgres.h"
#include "../db2/kb_payload.h"
#include "kb_doc_pdf.h"

#define PASS(name) printf("  PASS: %s\n", (name))

/* A minimal two-page bbox-layout fixture: page 1 has two lines, page 2 one line.
 * Coords are raw points; page boxes are 600x800. */
static const char *FIXTURE_2PAGE =
    "<html><body><doc>\n"
    "<page width=\"600.000000\" height=\"800.000000\">\n"
    "<flow><block>\n"
    "<line xMin=\"60\" yMin=\"40\" xMax=\"300\" yMax=\"60\">"
    "<word xMin=\"60\" yMin=\"40\" xMax=\"150\" yMax=\"60\">Hello</word>"
    "<word xMin=\"160\" yMin=\"40\" xMax=\"300\" yMax=\"60\">world</word></line>\n"
    "<line xMin=\"60\" yMin=\"80\" xMax=\"420\" yMax=\"100\">"
    "<word xMin=\"60\" yMin=\"80\" xMax=\"420\" yMax=\"100\">A&amp;B &lt;tag&gt;</word></line>\n"
    "</block></flow>\n"
    "</page>\n"
    "<page width=\"600.000000\" height=\"800.000000\">\n"
    "<flow><block>\n"
    "<line xMin=\"60\" yMin=\"40\" xMax=\"900\" yMax=\"60\">"
    "<word xMin=\"60\" yMin=\"40\" xMax=\"900\" yMax=\"60\">Second page</word></line>\n"
    "</block></flow>\n"
    "</page>\n"
    "</doc></body></html>\n";

static void test_parse(void)
{
   kb_pdf_doc_t doc;
   assert(kb_pdf_parse_bbox_layout(FIXTURE_2PAGE, &doc) == 0);
   assert(doc.n_pages == 2);
   assert(doc.pages[0].width == 600.0 && doc.pages[0].height == 800.0);
   assert(doc.pages[0].n_lines == 2);
   assert(doc.pages[1].n_lines == 1);
   assert(strcmp(doc.pages[0].lines[0].text, "Hello world") == 0);
   /* entity decode */
   assert(strcmp(doc.pages[0].lines[1].text, "A&B <tag>") == 0);
   assert(strcmp(doc.pages[1].lines[0].text, "Second page") == 0);
   assert(doc.pages[0].lines[0].page_no == 1 && doc.pages[1].lines[0].page_no == 2);
   /* raw (un-normalized) bbox */
   assert(doc.pages[0].lines[0].x0 == 60.0 && doc.pages[0].lines[0].x1 == 300.0);
   kb_pdf_free_doc(&doc);
   PASS("parse");
}

static void test_normalize(void)
{
   kb_pdf_doc_t doc;
   assert(kb_pdf_parse_bbox_layout(FIXTURE_2PAGE, &doc) == 0);
   kb_pdf_normalize(&doc);
   assert(doc.normalized);
   kb_pdf_line_t *l = &doc.pages[0].lines[0];
   /* 60/600=0.1, 40/800=0.05, 300/600=0.5, 60/800=0.075 */
   assert(l->x0 > 0.099 && l->x0 < 0.101);
   assert(l->y0 > 0.049 && l->y0 < 0.051);
   assert(l->x1 > 0.499 && l->x1 < 0.501);
   /* all within [0,1] */
   for (int p = 0; p < doc.n_pages; p++)
      for (int i = 0; i < doc.pages[p].n_lines; i++)
      {
         kb_pdf_line_t *q = &doc.pages[p].lines[i];
         assert(q->x0 >= 0 && q->x0 <= 1 && q->y0 >= 0 && q->y0 <= 1);
         assert(q->x1 >= 0 && q->x1 <= 1 && q->y1 >= 0 && q->y1 <= 1);
      }
   /* page-2 line had xMax=900 > width 600 -> clamps to 1.0 */
   assert(doc.pages[1].lines[0].x1 == 1.0);
   kb_pdf_free_doc(&doc);
   PASS("normalize");
}

static void test_chunk_page_boundary(void)
{
   kb_pdf_doc_t doc;
   assert(kb_pdf_parse_bbox_layout(FIXTURE_2PAGE, &doc) == 0);
   kb_pdf_normalize(&doc);
   kb_pdf_chunk_t *chunks = NULL;
   int n = 0;
   assert(kb_pdf_chunk(&doc, &chunks, &n) == 2); /* page-boundary: one chunk per page */
   /* chunk 0 = page 1 (2 lines), chunk 1 = page 2 (1 line) */
   assert(chunks[0].page_start == 1 && chunks[0].page_end == 1);
   assert(chunks[1].page_start == 2 && chunks[1].page_end == 2);
   assert(chunks[0].n_lines == 2 && chunks[1].n_lines == 1);
   /* content joins the page's lines with '\n' */
   assert(strcmp(chunks[0].content, "Hello world\nA&B <tag>") == 0);
   /* global line ordinals are contiguous across chunks */
   assert(chunks[0].line_start == 0 && chunks[0].line_end == 1);
   assert(chunks[1].line_start == 2 && chunks[1].line_end == 2);
   kb_pdf_free_chunks(chunks, n);
   kb_pdf_free_doc(&doc);
   PASS("chunk_page_boundary");
}

/* Build a one-page fixture with `nlines` lines to exercise the line-count cap. */
static char *build_big_page(int nlines)
{
   size_t cap = (size_t)nlines * 120 + 256;
   char *b = malloc(cap);
   size_t off = 0;
   off += (size_t)snprintf(b + off, cap - off,
                           "<doc><page width=\"600\" height=\"800\"><flow><block>\n");
   for (int i = 0; i < nlines; i++)
      off += (size_t)snprintf(b + off, cap - off,
                              "<line xMin=\"10\" yMin=\"%d\" xMax=\"20\" yMax=\"%d\">"
                              "<word xMin=\"10\" yMin=\"%d\" xMax=\"20\" yMax=\"%d\">w%d</word>"
                              "</line>\n",
                              i, i + 1, i, i + 1, i);
   snprintf(b + off, cap - off, "</block></flow></page></doc>\n");
   return b;
}

static void test_chunk_line_cap(void)
{
   char *xhtml = build_big_page(250); /* > 2 * cap(100) */
   kb_pdf_doc_t doc;
   assert(kb_pdf_parse_bbox_layout(xhtml, &doc) == 0);
   assert(doc.pages[0].n_lines == 250);
   kb_pdf_chunk_t *chunks = NULL;
   int n = 0;
   assert(kb_pdf_chunk(&doc, &chunks, &n) == 3); /* 100 + 100 + 50 */
   assert(chunks[0].n_lines == KB_PDF_MAX_CHUNK_LINES);
   assert(chunks[1].n_lines == KB_PDF_MAX_CHUNK_LINES);
   assert(chunks[2].n_lines == 50);
   /* all same page despite the split */
   for (int i = 0; i < n; i++)
      assert(chunks[i].page_start == 1 && chunks[i].page_end == 1);
   kb_pdf_free_chunks(chunks, n);
   kb_pdf_free_doc(&doc);
   free(xhtml);
   PASS("chunk_line_cap");
}

static void test_degraded_no_line_tags(void)
{
   /* A page with bare <word>s and no <line> wrapper -> one line per word, no text lost. */
   const char *x = "<doc><page width=\"600\" height=\"800\">"
                   "<word xMin=\"10\" yMin=\"10\" xMax=\"50\" yMax=\"20\">alpha</word>"
                   "<word xMin=\"60\" yMin=\"10\" xMax=\"99\" yMax=\"20\">beta</word>"
                   "</page></doc>";
   kb_pdf_doc_t doc;
   assert(kb_pdf_parse_bbox_layout(x, &doc) == 0);
   assert(doc.n_pages == 1 && doc.pages[0].n_lines == 2);
   assert(strcmp(doc.pages[0].lines[0].text, "alpha") == 0);
   assert(strcmp(doc.pages[0].lines[1].text, "beta") == 0);
   kb_pdf_free_doc(&doc);
   PASS("degraded_no_line_tags");
}

/* ---- shim-backed ingest ---- */

static int count_rows(const char *sql)
{
   char err[256] = "";
   aimee_pg_stmt_t *st = aimee_pg_prepare(db2_conn(), sql, err, sizeof(err));
   if (!st)
      return -1;
   int n = -1;
   if (aimee_pg_step(st, err, sizeof(err)) == AIMEE_PG_ROW)
      n = (int)aimee_pg_column_int64(st, 0);
   aimee_pg_finalize(st);
   return n;
}

static void test_ingest_shim(void)
{
   db2_test_shim_open();

   kb_pdf_ingest_stats_t stats;
   int rc =
       kb_doc_pdf_ingest_xhtml("proj", "report.pdf", "hash1", FIXTURE_2PAGE, "internal", &stats);
   assert(rc == 2); /* two chunks (one per page) */
   assert(stats.chunks == 2);
   assert(stats.regions == 3); /* 2 lines + 1 line */

   assert(count_rows("SELECT COUNT(*) FROM kb_documents WHERE doc_kind='pdf'") == 2);
   assert(count_rows("SELECT COUNT(*) FROM kb_documents WHERE chunk_strategy='page'") == 2);
   assert(count_rows("SELECT COUNT(*) FROM kb_documents WHERE page_start=1 AND page_end=1") == 1);
   assert(count_rows("SELECT COUNT(*) FROM kb_documents WHERE page_start=2 AND page_end=2") == 1);
   assert(count_rows("SELECT COUNT(*) FROM kb_doc_regions") == 3);
   assert(count_rows("SELECT COUNT(*) FROM kb_doc_regions WHERE content_type='text'") == 3);
   assert(count_rows("SELECT COUNT(*) FROM kb_doc_regions WHERE document_key='report.pdf'") == 3);
   /* per-chunk line_index starts at 0; the 2-line chunk has indices 0 and 1 */
   assert(count_rows("SELECT COUNT(*) FROM kb_doc_regions WHERE line_index=1") == 1);
   /* §6: sensitivity stamped on chunks AND regions; non-restricted -> no quarantine */
   assert(count_rows("SELECT COUNT(*) FROM kb_documents WHERE sensitivity_class='internal'") == 2);
   assert(count_rows("SELECT COUNT(*) FROM kb_doc_regions WHERE sensitivity_class='internal'") ==
          3);
   assert(count_rows("SELECT COUNT(*) FROM kb_documents WHERE quarantine_state=''") == 2);
   /* Phase 1b: PDFs are NOT embedded (no vectors -> invisible to vector-only search). */
   assert(count_rows("SELECT COUNT(*) FROM kb_async_jobs WHERE kind='embed_raw'") == 0);
   /* neighbour threading: exactly one row has a prev pointer (the 2nd chunk) */
   assert(count_rows("SELECT COUNT(*) FROM kb_documents WHERE prev_chunk_id IS NOT NULL") == 1);

   /* Re-ingest the same file: delete-then-insert replaces, regions cascade — no growth. */
   rc = kb_doc_pdf_ingest_xhtml("proj", "report.pdf", "hash2", FIXTURE_2PAGE, "internal", &stats);
   assert(rc == 2);
   assert(count_rows("SELECT COUNT(*) FROM kb_documents WHERE doc_kind='pdf'") == 2);
   assert(count_rows("SELECT COUNT(*) FROM kb_doc_regions") == 3);

   /* Empty extraction for the same file must NOT wipe the prior rows (0-chunk guard). */
   rc = kb_doc_pdf_ingest_xhtml("proj", "report.pdf", "hash3", "<doc></doc>", "internal", &stats);
   assert(rc == 0);
   assert(count_rows("SELECT COUNT(*) FROM kb_documents WHERE doc_kind='pdf'") == 2);
   assert(count_rows("SELECT COUNT(*) FROM kb_doc_regions") == 3);

   /* A `restricted` document is quarantined (pending) and still not embedded. */
   rc = kb_doc_pdf_ingest_xhtml("proj", "secret.pdf", "h4", FIXTURE_2PAGE, "restricted", &stats);
   assert(rc == 2);
   assert(count_rows("SELECT COUNT(*) FROM kb_documents WHERE file_path='secret.pdf' AND "
                     "quarantine_state='pending'") == 2);
   assert(count_rows("SELECT COUNT(*) FROM kb_documents WHERE file_path='secret.pdf' AND "
                     "sensitivity_class='restricted'") == 2);
   assert(count_rows("SELECT COUNT(*) FROM kb_async_jobs WHERE kind='embed_raw'") == 0);

   /* An invalid/empty sensitivity class is refused at ingest — no rows written. */
   rc = kb_doc_pdf_ingest_xhtml("proj", "bad.pdf", "h5", FIXTURE_2PAGE, "secret", &stats);
   assert(rc < 0);
   assert(count_rows("SELECT COUNT(*) FROM kb_documents WHERE file_path='bad.pdf'") == 0);
   rc = kb_doc_pdf_ingest_xhtml("proj", "bad2.pdf", "h6", FIXTURE_2PAGE, "", &stats);
   assert(rc < 0);
   assert(count_rows("SELECT COUNT(*) FROM kb_documents WHERE file_path='bad2.pdf'") == 0);

   /* The validator itself. */
   assert(kb_pdf_sensitivity_valid("public") && kb_pdf_sensitivity_valid("internal") &&
          kb_pdf_sensitivity_valid("restricted"));
   assert(!kb_pdf_sensitivity_valid("") && !kb_pdf_sensitivity_valid("secret") &&
          !kb_pdf_sensitivity_valid(NULL));

   db2_test_shim_close();
   PASS("ingest_shim");
}

int main(void)
{
   printf("structured-pdf Phase 1 (kb_doc_pdf) tests:\n");
   test_parse();
   test_normalize();
   test_chunk_page_boundary();
   test_chunk_line_cap();
   test_degraded_no_line_tags();
   test_ingest_shim();
   printf("ALL PASS\n");
   return 0;
}
