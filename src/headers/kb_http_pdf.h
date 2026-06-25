/* kb_http_pdf.h: structured-PDF Phase 2 read routes (citation retrieval). */
#ifndef DEC_KB_HTTP_PDF_H
#define DEC_KB_HTTP_PDF_H 1

/* GET /v1/pdf/search?query=..&project=..&max_results=N — the access-controlled
 * search_chunks tool. Returns matching structured-PDF chunks with line-level
 * {page_no, bbox, quote} citations. Restricted/quarantined PDFs are withheld by the
 * underlying query; the caller's token scope is enforced by kb_http_route_ex before this
 * runs. Writes a JSON body to out_buf and returns the HTTP status. */
int handle_get_pdf_search_route(const char *method, const char *query_string, char *out_buf,
                                int out_cap);

/* POST /v1/pdf/quarantine — §6 quarantine admin. Body: {project, document_key,
 * action:"confirm"|"reject"}. confirm releases a pending restricted PDF (it becomes
 * retrievable via search_chunks); reject purges it. The OWNER-only authorization is enforced
 * by the caller (kb_http_route_ex) before this runs — this handler does body parsing + the
 * state transition only. Writes a JSON body to out_buf and returns the HTTP status. */
int handle_post_pdf_quarantine_route(const char *method, const char *body, int body_len,
                                     char *out_buf, int out_cap);

#endif /* DEC_KB_HTTP_PDF_H */
