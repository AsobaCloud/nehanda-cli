/* http_uds_client.h: minimal HTTP/1.1 client over aimee-server's /v1 Unix
 * socket. Lets the thin client / TUI reach the server's HTTP API without the
 * legacy RPC socket and without reading server-owned files directly. */
#ifndef DEC_HTTP_UDS_CLIENT_H
#define DEC_HTTP_UDS_CLIENT_H 1

#ifdef __cplusplus
extern "C"
{
#endif

   /* Send an HTTP request to aimee-server's /v1 socket (<aimee_home>/aimee-http.sock).
    * method: "GET"/"POST"; path: e.g. "/v1/personas"; body: JSON or NULL.
    * Returns the response body (heap; caller frees) and sets *status_out to the
    * HTTP status. Returns NULL on connect/transport failure (status_out = 0). */
   char *http_uds_request(const char *method, const char *path, const char *body, int *status_out);

#ifdef __cplusplus
}
#endif

#endif /* DEC_HTTP_UDS_CLIENT_H */
