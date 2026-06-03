#ifndef DEC_CLIENT_INTEGRATIONS_H
#define DEC_CLIENT_INTEGRATIONS_H 1

void ensure_client_integrations(void);
void ensure_codex_project_trusted(const char *codex_home, const char *project_root);
void ensure_codex_current_project_trusted(const char *codex_home);

#endif /* DEC_CLIENT_INTEGRATIONS_H */
