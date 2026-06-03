#ifndef TESTS_SUPPORT_MOCK_AGENT_HTTP_H
#define TESTS_SUPPORT_MOCK_AGENT_HTTP_H

#include <stddef.h>

typedef int (*agent_http_stream_cb)(const char *data, size_t len, void *userdata);

typedef int (*mock_agent_http_stream_handler_t)(const char *url, const char *extra_headers,
                                                agent_http_stream_cb callback, void *userdata,
                                                int timeout_ms);
typedef int (*mock_agent_http_post_handler_t)(const char *url, const char *auth_header,
                                              const char *body, char **response_buf, int timeout_ms,
                                              const char *extra_headers);
typedef int (*mock_agent_http_get_handler_t)(const char *url, const char *extra_headers,
                                             char **response_buf, int timeout_ms);

void mock_agent_http_reset(void);
void mock_agent_http_set_stream_handler(mock_agent_http_stream_handler_t handler);
void mock_agent_http_set_post_handler(mock_agent_http_post_handler_t handler);
void mock_agent_http_set_get_handler(mock_agent_http_get_handler_t handler);

#endif
