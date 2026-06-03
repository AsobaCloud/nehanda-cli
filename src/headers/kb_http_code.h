#ifndef DEC_KB_HTTP_CODE_H
#define DEC_KB_HTTP_CODE_H 1

int handle_post_code_scan(const char *body, char *out_buf, int out_cap);
int handle_post_code_scan_route(const char *method, const char *body, char *out_buf, int out_cap);
int handle_get_code_projects(const char *query_string, char *out_buf, int out_cap);
int handle_get_code_projects_route(const char *method, const char *query_string, char *out_buf,
                                   int out_cap);
int handle_get_code_find(const char *query_string, char *out_buf, int out_cap);
int handle_get_code_find_route(const char *method, const char *query_string, char *out_buf,
                               int out_cap);
int handle_get_code_blast_radius(const char *query_string, char *out_buf, int out_cap);
int handle_get_code_blast_radius_route(const char *method, const char *query_string, char *out_buf,
                                       int out_cap);
int handle_get_code_structure(const char *query_string, char *out_buf, int out_cap);
int handle_get_code_structure_route(const char *method, const char *query_string, char *out_buf,
                                    int out_cap);
int handle_get_code_search(const char *query_string, char *out_buf, int out_cap);
int handle_get_code_search_route(const char *method, const char *query_string, char *out_buf,
                                 int out_cap);
int handle_get_code_callers(const char *query_string, char *out_buf, int out_cap);
int handle_get_code_callers_route(const char *method, const char *query_string, char *out_buf,
                                  int out_cap);
int handle_get_code_project_stats(const char *query_string, char *out_buf, int out_cap);
int handle_get_code_project_stats_route(const char *method, const char *query_string, char *out_buf,
                                        int out_cap);

#endif
